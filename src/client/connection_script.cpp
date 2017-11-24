// connection_scripts.cpp
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
#include <fstream>
#include <neoirc/client/model.hpp>
#include <neoirc/client/connection_script.hpp>

namespace irc
{
	namespace
	{
		template <typename List, typename Iter, typename Result>
		Iter find_impl(casemapping::type aCasemapping, List& aList, const server_key& aServer, const std::string& aNickName, Result& aResult)
		{
			for (Iter i = aList.entries().begin(); i != aList.entries().end(); ++i)
			{
				const connection_script& theEntry = *i;
				if (aList.matches(aCasemapping, theEntry, aServer, aNickName))
					aResult.push_back(i);
			}
			if (!aResult.empty())
				return aResult.front();
			else
				return aList.entries().end();
		}
	}

	connection_scripts::connection_scripts(identities& aIdentities) : iIdentities(aIdentities), iLoading(false)
	{
		iIdentities.add_observer(*this);
	}

	connection_scripts::~connection_scripts()
	{
		iIdentities.remove_observer(*this);
	}

	connection_scripts::container_type::const_iterator connection_scripts::find(casemapping::type aCasemapping, const server_key& aServer, const std::string& aNickName) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const connection_scripts, container_type::const_iterator>(aCasemapping, *this, aServer, aNickName, result);
	}

	connection_scripts::container_type::iterator connection_scripts::find(casemapping::type aCasemapping, const server_key& aServer, const std::string& aNickName)
	{
		std::vector<container_type::iterator> result;
		return find_impl<connection_scripts, container_type::iterator>(aCasemapping, *this, aServer, aNickName, result);
	}

	connection_scripts::container_type::const_iterator connection_scripts::find(casemapping::type aCasemapping, const connection_script& aEntry) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const connection_scripts, container_type::const_iterator>(aCasemapping, *this, aEntry.server(), aEntry.nick_name(), result);
	}

	connection_scripts::container_type::iterator connection_scripts::find(casemapping::type aCasemapping, const connection_script& aEntry)
	{
		std::vector<container_type::iterator> result;
		return find_impl<connection_scripts, container_type::iterator>(aCasemapping, *this, aEntry.server(), aEntry.nick_name(), result);
	}

	bool connection_scripts::matches(casemapping::type aCasemapping, const connection_script& aEntry, const server_key& aServer, const std::string& aNickName) const
	{
		if (aEntry.server().first != aServer.first ||
			(aEntry.server().second != aServer.second && aEntry.server().second != "*"))
			return false;
		if (irc::make_string(aCasemapping, aEntry.nick_name()) != aNickName)
			return false;
		return true;
	}

	bool connection_scripts::has_connection_scripts(const server_key& aServer, const std::string& aNickName) const
	{
		return find(casemapping::ascii, aServer, aNickName) != iEntries.end();
	}

	bool connection_scripts::has_connection_scripts(const server_key& aServer, const std::string& aNickName, const std::string& aScript) const
	{
		std::vector<container_type::const_iterator> result;
		find_impl<const connection_scripts, container_type::const_iterator>(casemapping::ascii, *this, aServer, aNickName, result);
		for (std::vector<container_type::const_iterator>::const_iterator i = result.begin(); i != result.end(); ++i)
			if ((**i).script() == aScript)
				return true;
		return false;
	}

	bool connection_scripts::add(const server_key& aServer, const std::string& aNickName, const std::string& aScript, bool aEnabled)
	{
		if (has_connection_scripts(aServer, aNickName, aScript))
			return false;

		iEntries.push_back(connection_script(aServer, aNickName, aScript, aEnabled));
		if (!iLoading)
			notify_observers(connection_scripts_observer::NotifyAdded, iEntries.back());

		return true;
	}

	bool connection_scripts::add(const connection_script& aEntry)
	{
		return add(aEntry.server(), aEntry.nick_name(), aEntry.script(), aEntry.enabled());
	}

	void connection_scripts::update(connection_script& aExistingEntry, const connection_script& aNewEntry)
	{
		aExistingEntry = aNewEntry;
		if (!iLoading)
			notify_observers(connection_scripts_observer::NotifyUpdated, aExistingEntry);
	}

	void connection_scripts::remove(const server_key& aServer, const std::string& aNickName)
	{
		std::vector<container_type::iterator> result;
		find_impl<connection_scripts, container_type::iterator>(casemapping::ascii, *this, aServer, aNickName, result);
		for (std::vector<container_type::iterator>::iterator i = result.begin(); i != result.end(); ++i)
		{
			container_type temp;
			temp.splice(temp.begin(), iEntries, *i);
			notify_observers(connection_scripts_observer::NotifyRemoved, *temp.begin());
			break;
		}
	}

	void connection_scripts::remove(const connection_script& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (&*i == &aEntry)
			{
				container_type temp;
				temp.splice(temp.begin(), iEntries, i);
				notify_observers(connection_scripts_observer::NotifyRemoved, *temp.begin());
				break;
			}
	}

	void connection_scripts::notify_observer(connection_scripts_observer& aObserver, connection_scripts_observer::notify_type aType, const void* aParameter, const void*)
	{
		switch(aType)
		{
		case connection_scripts_observer::NotifyAdded:
			aObserver.connection_script_added(*static_cast<const connection_script*>(aParameter));
			break;
		case connection_scripts_observer::NotifyUpdated:
			aObserver.connection_script_updated(*static_cast<const connection_script*>(aParameter));
			break;
		case connection_scripts_observer::NotifyRemoved:
			aObserver.connection_script_removed(*static_cast<const connection_script*>(aParameter));
			break;
		}
	}

	void connection_scripts::identity_added(const identity& aEntry)
	{
	}

	void connection_scripts::identity_updated(const identity& aEntry, const identity& aOldEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (i->nick_name() == aOldEntry.nick_name() &&
				i->nick_name() != aEntry.nick_name())
			{
				connection_script newEntry(*i);
				newEntry.set_nick_name(aEntry.nick_name());
				update(*i, newEntry);
			}
	}

	void connection_scripts::identity_removed(const identity& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end();)
			if (i->nick_name() == aEntry.nick_name())
				remove(*i++);
			else
				++i;
	}

	void read_connection_scripts(model& aModel, std::function<bool()> aErrorFunction)
	{
		connection_scripts& theConnectionScripts = aModel.connection_scripts();

		neolib::xml xmlConnectionScripts(true);
		std::ifstream input((aModel.root_path() + "connection_scripts.xml").c_str());
		xmlConnectionScripts.read(input);

		if (xmlConnectionScripts.error() && aErrorFunction && aErrorFunction())
		{
			theConnectionScripts.entries().clear();
			write_connection_scripts(aModel);
			return;
		}

		theConnectionScripts.loading(true);

		for (neolib::xml::element::const_iterator i = xmlConnectionScripts.root().begin(); i != xmlConnectionScripts.root().end(); ++i)
		{
			if (i->name() == "entry")
			{
				theConnectionScripts.entries().push_back(connection_script());
				connection_script& e = theConnectionScripts.entries().back();
				for (neolib::xml::element::const_iterator j = i->begin(); j != i->end(); ++j)
				{
					if (j->name() == "server_network" || j->name() == "server_group")
						e.set_server(server_key(j->attribute_value("value"), e.server().second));
					else if (j->name() == "server_name")
						e.set_server(server_key(e.server().first, j->attribute_value("value")));
					else if (j->name() == "nick_name")
						e.set_nick_name(j->attribute_value("value"));
					else if (j->name() == "enabled")
						e.set_enabled(j->attribute_value("value") == "1" ? true : false);
					else if (j->name() == "script")
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
			}
		}

		theConnectionScripts.loading(false);
	}

	void write_connection_scripts(const model& aModel)
	{
		const connection_scripts& theConnectionScripts = aModel.connection_scripts();

		neolib::xml xmlConnectionScripts(true);

		xmlConnectionScripts.root().name() = "connection_scripts";

		for (connection_scripts::container_type::const_iterator i = theConnectionScripts.entries().begin(); i != theConnectionScripts.entries().end(); ++i)
		{
			const connection_script& entry = *i;
			neolib::xml::element& xmlEntry = xmlConnectionScripts.append(xmlConnectionScripts.root(), "entry");
			xmlConnectionScripts.append(xmlEntry, "server_network").set_attribute("value", entry.server().first);
			xmlConnectionScripts.append(xmlEntry, "server_name").set_attribute("value", entry.server().second);
			xmlConnectionScripts.append(xmlEntry, "nick_name").set_attribute("value", entry.nick_name());
			xmlConnectionScripts.append(xmlEntry, "enabled").set_attribute("value", entry.enabled() ? "1" : "0");
			neolib::xml::element& xmlEntryScript = xmlConnectionScripts.append(xmlEntry, "script");
			typedef std::vector<std::string> lines_t;
			lines_t lines;
			neolib::tokens(entry.script(), std::string("\r\n"), lines);
			for (lines_t::iterator i = lines.begin(); i != lines.end(); ++i)
			{
				xmlConnectionScripts.append(xmlEntryScript, "line").append_text(*i);
			}
		}

		std::ofstream output((aModel.root_path() + "connection_scripts.xml").c_str());
		xmlConnectionScripts.write(output);
	}
}