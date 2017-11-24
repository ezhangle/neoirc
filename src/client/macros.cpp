// macros.cpp
/*
 *  Copyright (c) 2010 Leigh Johnston.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of Leigh Johnston nor the names of any
 *       other contributors to this software may be used to endorse or
 *       promote products derived from this software without specific prior
 *       written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 *  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <neolib/neolib.hpp>
#include <neolib/xml.hpp>
#include <neolib/vecarray.hpp>
#include <fstream>
#include <neoirc/client/model.hpp>
#include <neoirc/client/buffer.hpp>
#include <neoirc/client/channel_buffer.hpp>
#include <neoirc/client/macros.hpp>

namespace irc
{
	macros::macros() : iLoading(false), iParseDepth(0)
	{
	}

	macros::~macros()
	{
	}

	bool macros::has_macro(const std::string& aMacro) const
	{
		return iEntries.find(neolib::make_ci_string(aMacro)) != iEntries.end();
	}

	bool macros::add(const std::string& aMacro, const std::string& aDescription, const std::string& aScript, bool aEnabled, bool aUserMenu)
	{
		if (has_macro(aMacro))
			return false;

		container_type::iterator iterAdded = iEntries.insert(macro(aMacro, aDescription, aScript, aEnabled, aUserMenu));
		if (!iLoading)
			notify_observers(macros_observer::NotifyAdded, *iterAdded);

		return true;
	}

	bool macros::add(const macro& aEntry)
	{
		return add(aEntry.name(), aEntry.description(), aEntry.script(), aEntry.enabled(), aEntry.user_menu());
	}

	void macros::update(macro& aExistingEntry, const macro& aNewEntry)
	{
		if (aExistingEntry.name() == aNewEntry.name())
		{
			aExistingEntry = aNewEntry;
			if (!iLoading)
				notify_observers(macros_observer::NotifyUpdated, aExistingEntry);
		}
		else
		{
			remove(aExistingEntry);
			add(aNewEntry);
		}
	}

	void macros::remove(const std::string& aMacro)
	{
		container_type::iterator i = iEntries.find(neolib::make_ci_string(aMacro));
		if (i != iEntries.end())
		{
			notify_observers(macros_observer::NotifyRemoved, *i);
			iEntries.erase(i);
		}
	}

	void macros::remove(const macro& aEntry)
	{
		remove(aEntry.name());
	}

	void macros::notify_observer(macros_observer& aObserver, macros_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case macros_observer::NotifyAdded:
			aObserver.macro_added(*static_cast<const macro*>(aParameter));
			break;
		case macros_observer::NotifyUpdated:
			aObserver.macro_updated(*static_cast<const macro*>(aParameter));
			break;
		case macros_observer::NotifyRemoved:
			aObserver.macro_removed(*static_cast<const macro*>(aParameter));
			break;
		case macros_observer::NotifySyntaxError:
			aObserver.macro_syntax_error(*static_cast<const macro*>(aParameter), 
				static_cast<const std::pair<std::size_t, macro::error>*>(aParameter2)->first,
				static_cast<const std::pair<std::size_t, macro::error>*>(aParameter2)->second);
			break;
		}
	}

	bool macros::parse(buffer& aBuffer, const std::string& aMessage, bool aAll)
	{
		neolib::vecarray<std::string, 1> command;
		neolib::tokens(aMessage, std::string(" \t"), command, 1);
		if (command.empty())
			return false;
		if (command[0][0] != '/')
			command[0] = std::string("/") + command[0];
		if (!has_macro(command[0]))
			return false;
		const macro& theMacro = *iEntries.find(neolib::make_ci_string(command[0]));
		if (!theMacro.enabled())
			return true;
		if (++iParseDepth > 10)
		{
			notify_observers(macros_observer::NotifySyntaxError, theMacro, std::make_pair(static_cast<std::size_t>(1), macro::too_deep));
			--iParseDepth;
			return false;
		}
		typedef std::vector<std::string> lines_t;
		lines_t lines;
		neolib::tokens(theMacro.script(), std::string("\r\n"), lines, 0);
		for (lines_t::const_iterator i = lines.begin(); i != lines.end(); ++i)
		{
			macro::error e = parse_line(aBuffer, aMessage, *i, aAll);
			if (e != macro::ok)
			{
				notify_observers(macros_observer::NotifySyntaxError, theMacro, std::make_pair(static_cast<std::size_t>(i - lines.begin() + 1), e));
				break;
			}
		}
		--iParseDepth;
		return true;
	}

	macro::error macros::parse_line(buffer& aBuffer, const std::string& aMessage, const std::string& aLine, bool aAll)
	{
		if (aLine.empty())
			return macro::ok;
		typedef std::pair<std::string::const_iterator, std::string::const_iterator> parameter_t;
		typedef std::vector<parameter_t> parameters_t;
		parameters_t parameters;
		const std::string delim(" \t");
		neolib::tokens(aMessage.begin(), aMessage.end(), delim.begin(), delim.end(), parameters);
		std::string output;
		for (std::string::const_iterator i = aLine.begin(); i != aLine.end();)
		{
			char ch = *i++;
			if (ch != '%')
				output += ch;
			else
			{
				std::string::const_iterator j = std::find(i, aLine.end(), '%');
				if (j == aLine.end())
					return macro::syntax_error;
				std::string token(i, j);
				if (token.empty())
					output += ch;
				else
				{
					std::pair<std::size_t, std::size_t> range(0, 0);
					bool noParams = false;
					std::string value;
					neolib::ci_string function;
					if (token.find(':') != std::string::npos)
					{
						neolib::vecarray<std::string, 2> parts;
						neolib::tokens(token, std::string(":"), parts, 2);
						if (parts.size() != 2)
							return macro::syntax_error;
						token = parts[0];
						function = neolib::make_ci_string(parts[1]);
					}
					std::string::size_type pos = token.find("..");
					if (pos != std::string::npos)
					{
						std::string::const_iterator sep = token.begin() + pos;
						typedef std::pair<std::string::const_iterator, std::string::const_iterator> part_t;
						typedef neolib::vecarray<part_t, 2> parts_t;
						parts_t parts;
						const std::string delim(".");
						neolib::tokens(token.begin(), token.end(), delim.begin(), delim.end(), parts, 2);
						if (parts.empty())
						{
							if (parameters.size() >= 2)
							{
								range.first = 1;
								range.second = parameters.size() - 1;
							}
							else
								noParams = true;
						}
						else if (parts.size() == 1)
						{
							if (parts[0].first < sep)
							{
								range.first = neolib::string_to_unsigned_integer(std::string(parts[0].first, parts[0].second));
								range.second = parameters.size() - 1;
							}
							else
							{
								range.first = 1;
								range.second = neolib::string_to_unsigned_integer(std::string(parts[0].first, parts[0].second));
							}
						}					
						else
						{
							range.first = neolib::string_to_unsigned_integer(std::string(parts[0].first, parts[0].second));
							range.second = neolib::string_to_unsigned_integer(std::string(parts[1].first, parts[1].second));
						}
					}
					else if (token.find_first_not_of("0123456789") == std::string::npos)
					{
						range.first = range.second = neolib::string_to_unsigned_integer(token);
					}
					else if (neolib::make_ci_string(token) == neolib::ci_string("C"))
					{
						value = aBuffer.name();
					}
					if (range.first > range.second)
						std::swap(range.first, range.second);
					if (range.first >= parameters.size() || range.second >= parameters.size())
						return macro::insufficient_parameters;
					if (function == neolib::ci_string("banmask"))
					{
						if (!value.empty())
						{
							if (!aBuffer.has_user(value))
								return macro::not_found;
							output += aBuffer.user(value).ban_mask();
						}
						else if (!noParams)
						{
							for (std::size_t p = range.first; p <= range.second;++p)
							{
								if (!aBuffer.has_user(std::string(parameters[p].first, parameters[p].second)))
									return macro::not_found;
								if (p != range.first)
									output += ' ';
								output += aBuffer.user(std::string(parameters[p].first, parameters[p].second)).ban_mask();
							}
						}
						else
							return macro::insufficient_parameters;
					}
					else if (function == neolib::ci_string("ip"))
					{
						if (!value.empty())
						{
							if (!aBuffer.has_user(value))
								return macro::not_found;
							output += aBuffer.user(value).host_name();
						}
						else if (!noParams)
						{
							for (std::size_t p = range.first; p <= range.second;++p)
							{
								if (!aBuffer.has_user(std::string(parameters[p].first, parameters[p].second)))
									return macro::not_found;
								if (p != range.first)
									output += ' ';
								output += aBuffer.user(std::string(parameters[p].first, parameters[p].second)).host_name();
							}
						}
						else
							return macro::insufficient_parameters;
					}
					else if (function == neolib::ci_string("id") || function == neolib::ci_string("who"))
					{
						if (!value.empty())
						{
							if (!aBuffer.has_user(value))
								return macro::not_found;
							output += aBuffer.user(value).msgto_form();
						}
						else if (!noParams)
						{
							for (std::size_t p = range.first; p <= range.second;++p)
							{
								if (!aBuffer.has_user(std::string(parameters[p].first, parameters[p].second)))
									return macro::not_found;
								if (p != range.first)
									output += ' ';
								output += aBuffer.user(std::string(parameters[p].first, parameters[p].second)).msgto_form();
							}
						}
						else
							return macro::insufficient_parameters;
					}
					else if (function.empty())
					{
						if (!value.empty())
							output += value;
						else if (!noParams)
							output += std::string(parameters[range.first].first, parameters[range.second].second);
					}
					else
						return macro::syntax_error;
				}
				i = j;
				++i;
			}
		}
		if (!output.empty())
			aBuffer.new_message(output, false, aAll);
		return macro::ok;
	}

	void read_macros(model& aModel, std::function<bool()> aErrorFunction)
	{
		macros& theMacros = aModel.macros();

		neolib::xml xmlMacros(true);
		std::ifstream input((aModel.root_path() + "macros.xml").c_str());
		xmlMacros.read(input);

		if (xmlMacros.error() && aErrorFunction && aErrorFunction())
		{
			theMacros.entries().clear();
			write_macros(aModel);
			return;
		}

		theMacros.loading(true);

		for (neolib::xml::element::const_iterator i = xmlMacros.root().begin(); i != xmlMacros.root().end(); ++i)
		{
			if (i->name() == "entry")
			{
				macro e;
				for (neolib::xml::element::const_iterator j = i->begin(); j != i->end(); ++j)
				{
					if (j->name() == "macro")
						e.set_name(j->attribute_value("value"));
					if (j->name() == "description")
						e.set_description(std::string(j->attribute_value("value")));
					if (j->name() == "enabled")
						e.set_enabled(std::string(j->attribute_value("value")) == "1" ? true : false);
					if (j->name() == "user_menu")
						e.set_user_menu(std::string(j->attribute_value("value")) == "1" ? true : false);
					if (j->name() == "script")
					{
						std::string script;
						for (neolib::xml::element::const_iterator k = j->begin(); k != j->end(); ++k)
						{
							if (k->name() == "line")
								script += k->text() + "\r\n";
						}
						e.set_script(script);
					}
				}
				theMacros.add(e);
			}
		}

		theMacros.loading(false);
	}

	void write_macros(const model& aModel)
	{
		const macros& theMacros = aModel.macros();

		neolib::xml xmlMacros(true);

		xmlMacros.root().name() = "macros";

		for (macros::container_type::const_iterator i = theMacros.entries().begin(); i != theMacros.entries().end(); ++i)
		{
			const macro& entry = *i;
			neolib::xml::element& xmlEntry = xmlMacros.append(xmlMacros.root(), "entry");
			xmlMacros.append(xmlEntry, "macro").set_attribute("value", entry.name());
			xmlMacros.append(xmlEntry, "description").set_attribute("value", entry.description());
			xmlMacros.append(xmlEntry, "enabled").set_attribute("value", entry.enabled() ? "1" : "0");
			xmlMacros.append(xmlEntry, "user_menu").set_attribute("value", entry.user_menu() ? "1" : "0");
			neolib::xml::element& xmlEntryScript = xmlMacros.append(xmlEntry, "script");
			typedef std::vector<std::string> lines_t;
			lines_t lines;
			neolib::tokens(entry.script(), std::string("\r\n"), lines);
			for (lines_t::iterator i = lines.begin(); i != lines.end(); ++i)
				xmlMacros.append(xmlEntryScript, "line").append_text(*i);
		}

		std::ofstream output((aModel.root_path() + "macros.xml").c_str());
		xmlMacros.write(output);
	}
}