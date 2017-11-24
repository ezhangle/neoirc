// buffer.cpp
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
#include <ctime>
#include <neoirc/client/buffer.hpp>
#include <neoirc/client/connection.hpp>
#include <neoirc/client/connection_manager.hpp>
#include <neoirc/client/dcc_connection_manager.hpp>
#include <neoirc/client/whois.hpp>
#include <neoirc/client/who.hpp>
#include <neoirc/client/dns.hpp>
#include <neoirc/client/ignore.hpp>
#include <neoirc/client/macros.hpp>
#include <neoirc/client/auto_joins.hpp>

namespace irc
{
	buffer::buffer(irc::model& aModel, type_e aType, irc::connection& aConnection, const std::string& aName, const std::string& aTitle) :
		iModel(aModel), iType(aType), iId(aConnection.next_buffer_id()), iConnection(aConnection),
		iBufferSize(iModel.buffer_size()), iName(aName), iTitle(!aTitle.empty() ? aTitle : aName), iClosing(false), iReady(iConnection.registered())
	{
		iModel.neolib::observable<model_observer>::add_observer(*this);
	}

	buffer::~buffer()
	{
		iClosing = true;
		iModel.neolib::observable<model_observer>::remove_observer(*this);
		deactivate();
		notify_observers(buffer_observer::NotifyClosing);
		clear();
	}

	void buffer::scrollback(const message_list& aMessages)
	{
		message_list::const_reverse_iterator i = aMessages.rbegin();
		while (iMessages.size() < iBufferSize && i != aMessages.rend())
			iMessages.push_front(*i++);
		notify_observers(buffer_observer::NotifyScrollbacked);
	}

	void buffer::clear()
	{
		while (!iMessages.empty())
		{
			notify_observers(buffer_observer::NotifyMessageRemoved, iMessages.back());
			iMessages.pop_back();
		}
		notify_observers(buffer_observer::NotifyCleared);
	}

	bool buffer::just_weak_observers()
	{
		bool justWeakObservers = true;
		for (neolib::observable<buffer_observer>::observer_list::iterator i = iObservers.begin(); justWeakObservers && i != iObservers.end(); ++i)
		{
			bool isWeakObserver = false;
			notify_observer(**i, buffer_observer::NotifyIsWeakObserver, &isWeakObserver);
			if (!isWeakObserver)
				justWeakObservers = false;
		}
		return justWeakObservers;
	}

	void buffer::set_name(const std::string& aName)
	{
		if (iName == aName)
			return;
		std::string oldName = iName;
		iName = aName;
		notify_observers(buffer_observer::NotifyNameChange, oldName);
	}

	void buffer::set_title(const std::string& aTitle)
	{
		if (iTitle == aTitle)
			return;
		std::string oldTitle = iTitle;
		iTitle = aTitle;
		notify_observers(buffer_observer::NotifyTitleChange, oldTitle);
	}

	bool buffer::is_channel() const
	{
		return is_channel(name());
	}

	bool buffer::is_channel(const std::string& aName) const
	{
		return iConnection.is_channel(aName);
	}

	void buffer::set_ready(bool aReady)
	{
		if (iReady == aReady)
			return;
		iReady = aReady;
		if (!iReady)
			iDelayedCommands.clear();
		on_set_ready();
		notify_observers(buffer_observer::NotifyReadyChange);
	}

	void buffer::remove_observer(buffer_observer& aObserver)
	{
		bool beforeStrongObservers = !just_weak_observers();
		neolib::observable<buffer_observer>::remove_observer(aObserver);
		if (iClosing)
			return;
		if (beforeStrongObservers && just_weak_observers())
			iConnection.handle_orphan(*this);
	}

	void buffer::notify_observer(buffer_observer& aObserver, buffer_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case buffer_observer::NotifyMessage:
			aObserver.buffer_message(*this, *static_cast<const message*>(aParameter));
			break;
		case buffer_observer::NotifyMessageUpdated:
			aObserver.buffer_message_updated(*this, *static_cast<const message*>(aParameter));
			break;
		case buffer_observer::NotifyMessageRemoved:
			aObserver.buffer_message_removed(*this, *static_cast<const message*>(aParameter));
			break;
		case buffer_observer::NotifyMessageFailure:
			aObserver.buffer_message_failure(*this, *static_cast<const message*>(aParameter));
			break;
		case buffer_observer::NotifyActivate:
			aObserver.buffer_activate(*this);
			break;
		case buffer_observer::NotifyReopen:
			aObserver.buffer_reopen(*this);
			break;
		case buffer_observer::NotifyOpen:
			aObserver.buffer_open(*this, *static_cast<const message*>(aParameter));
			break;
		case buffer_observer::NotifyClosing:
			aObserver.buffer_closing(*this);
			break;
		case buffer_observer::NotifyIsWeakObserver:
			*static_cast<bool*>(const_cast<void*>(aParameter)) = aObserver.is_weak_observer(*this);
			break;
		case buffer_observer::NotifyNameChange:
			aObserver.buffer_name_changed(*this, *static_cast<const std::string*>(aParameter));
			break;
		case buffer_observer::NotifyTitleChange:
			aObserver.buffer_title_changed(*this, *static_cast<const std::string*>(aParameter));
			break;
		case buffer_observer::NotifyReadyChange:
			aObserver.buffer_ready_changed(*this);
			break;
		case buffer_observer::NotifyScrollbacked:
			aObserver.buffer_scrollbacked(*this);
			break;
		case buffer_observer::NotifyCleared:
			aObserver.buffer_cleared(*this);
			break;
		case buffer_observer::NotifyHide:
			aObserver.buffer_hide(*this);
			break;
		case buffer_observer::NotifyShow:
			aObserver.buffer_show(*this);
			break;
		}
	}

	void buffer::buffer_size_changed(std::size_t aNewBufferSize)
	{
		iBufferSize = aNewBufferSize;
		while (iMessages.size() > iBufferSize)
		{
			notify_observers(buffer_observer::NotifyMessageRemoved, iMessages.front());
			iMessages.pop_front();
		}
	}

	bool buffer::add_message(const message& aMessage)
	{
		iMessages.push_back(aMessage);
		buffer_size_changed(iBufferSize);
		return true;
	}

	void buffer::add_and_handle_message(const message& aMessage)
	{
		if (add_message(aMessage))
			handle_message(iMessages.back());
		else
			handle_message(aMessage);
	}

	void buffer::insert_channel_parameter(message& aMessage) const
	{
		if (iType != CHANNEL)
			return;
		if ((aMessage.parameters().size() >= 1 &&
			!is_channel(aMessage.parameters()[0])) ||
			aMessage.parameters().size() == 0)
		{
			aMessage.parameters().insert(aMessage.parameters().begin(), name());
			if (aMessage.content_param() != message::no_content && 
				aMessage.parameters().size() > aMessage.content_param() + 1)
			{
				aMessage.parameters()[aMessage.content_param()] += 
					" " + aMessage.parameters()[aMessage.content_param()+1];
				message::parameters_t::iterator oldContent = aMessage.parameters().end();
				aMessage.parameters().erase(--oldContent);
			}
		}
	}

	void buffer::update_chantype_messages()
	{
		for (message_list::const_iterator i = iMessages.begin(); i != iMessages.end(); ++i)
		{
			neolib::string_spans theSpans;
			i->to_nice_string(iModel.message_strings(), this, "", false, false, false, &theSpans);
			for (neolib::string_spans::const_iterator j = theSpans.begin(); j != theSpans.end(); ++j)
				if (j->iType == message::Channel)
				{
					notify_observers(buffer_observer::NotifyMessageUpdated, *i);
					break;
				}
		}
	}

	void buffer::handle_message(const message& aMessage)
	{
		notify_observers(buffer_observer::NotifyMessage, aMessage);
	}

	model::id buffer::next_message_id()
	{
		return iConnection.next_message_id();
	}

	bool buffer::send_message(const message& aMessage)
	{
		return iConnection.send_message(*this, aMessage);
	}

	internal_command sInternalCommandList[] =
	{
		{"OPEN", internal_command::OPEN},
		{"LEAVE", internal_command::LEAVE},
		{"MSG", internal_command::MSG}, 
		{"QUERY", internal_command::QUERY},
		{"PM", internal_command::MSG},
		{"RAW", internal_command::RAW},
		{"QUOTE", internal_command::RAW},
		{"ME", internal_command::ACTION},
		{"ACTION", internal_command::ACTION},
		{"EMOTE", internal_command::ACTION},
		{"IGNORE", internal_command::IGNORE_USER},
		{"UNIGNORE", internal_command::UNIGNORE_USER},
		{"VERSION", internal_command::VERSION},
		{"CLIENTINFO", internal_command::CLIENTINFO},
		{"FINGER", internal_command::FINGER},
		{"SOURCE", internal_command::SOURCE},
		{"USERINFO", internal_command::USERINFO},
		{"TIME", internal_command::TIME},
		{"SERVER", internal_command::SERVER},
		{"CTCP", internal_command::CTCP},
		{"CHAT", internal_command::CHAT},
		{"DNS", internal_command::DNS},
		{"SHOWPINGS", internal_command::SHOWPINGS},
		{"HIDEPINGS", internal_command::HIDEPINGS},
		{"AUTOJOIN", internal_command::AUTOJOIN},
		{"AUTOREJOIN", internal_command::AUTOREJOIN},
		{"ALL", internal_command::ALL},
		{"CLEAR", internal_command::CLEAR},
		{"HIDE", internal_command::HIDE},
		{"SHOW", internal_command::SHOW},
		{"CLS", internal_command::CLEAR},
		{"DELAY", internal_command::DELAY},
		{"AFTER", internal_command::DELAY},
		{"ECHO", internal_command::ECHO},
		{"XYZZY", internal_command::XYZZY},
		{"FINDUSER", internal_command::FINDUSER},
		{"TIMER", internal_command::TIMER}
	};

	std::pair<internal_command::command_e, std::string> get_internal_command(const std::string& aCommand)
	{
		typedef std::map<neolib::ci_string, internal_command::command_e> internal_commands;
		static internal_commands sInternalCommands;
		if (sInternalCommands.empty())
			for (int i = 0; i < sizeof(sInternalCommandList) / sizeof(sInternalCommandList[0]); ++i)
				sInternalCommands[sInternalCommandList[i].iString] = sInternalCommandList[i].iCommand;
		internal_commands::const_iterator it = sInternalCommands.find(neolib::make_ci_string(aCommand));
		return ((it != sInternalCommands.end() ? std::make_pair(it->second, std::string(it->first.c_str())) : std::make_pair(internal_command::UNKNOWN, std::string())));
	}

	void buffer::new_message(const std::string& aMessage, bool aAsPrivmsg, bool aAll)
	{
		if (aMessage.empty())
			return;

		bool isCommand = (!aAsPrivmsg && aMessage[0] == '/') || type() == SERVER || type() == NOTICE;

		typedef std::vector<std::pair<std::string::const_iterator, std::string::const_iterator> > lines_t;
		lines_t lines;
		neolib::tokens(aMessage, std::string("\r\n"), lines);
		if (lines.empty())
			return;
		else if (lines.size() > 1)
		{
			for (lines_t::iterator i = lines.begin(); i != lines.end(); ++i)
				new_message(std::string(i->first, i->second), aAsPrivmsg || !isCommand);
			return;
		}
		std::string strMessage = std::string(lines[0].first, lines[0].second);

		if (isCommand && strMessage[0] == '/')
		{
			if (connection().connection_manager().model().do_custom_command(strMessage))
				return;
			strMessage.erase(strMessage.begin());
		}

		if (!isCommand)
		{
			message newMessage(*this, message::OUTGOING);
			newMessage.set_origin(iConnection.user().msgto_form());
			newMessage.set_command(message::PRIVMSG);
			newMessage.parameters().push_back(name());
			newMessage.parameters().push_back(strMessage);
			if (newMessage.to_string(iModel.message_strings(), true).size() <= message::max_size())
			{
				if (!aAll)
					new_message(newMessage);
				else
					iConnection.connection_manager().new_message_all(newMessage);
			}
			else
			{
				std::string::size_type maxContentLength = message::max_size() - (newMessage.to_string(iModel.message_strings(), true).size() - newMessage.parameters().back().size());
				std::string content = newMessage.parameters().back();
				std::string::size_type s = 0;
				std::string::size_type n = content.size();
				bool utf8Check = iModel.is_unicode(*this);
				while(n > 0)
				{
					if (n > maxContentLength)
						n = maxContentLength;
					std::string::size_type n1 = n;
					while(n1 > 0 && utf8Check && s + n1 < content.size() &&
						neolib::is_utf8_trailing(content[s + n1]))
						--n1;
					if (n1 != 0)
						n = n1;
					newMessage.parameters().back() = content.substr(s, n);
					if (!aAll)
						new_message(newMessage);
					else
						iConnection.connection_manager().new_message_all(newMessage);
					s += n;
					n = content.size() - s;
					newMessage.set_id(next_message_id());
				}
			}
			return;
		}

		if (isCommand && iModel.macros().parse(*this, strMessage, aAll))
			return;
		
		message newMessage(*this, message::OUTGOING);
		newMessage.set_origin(iConnection.user().msgto_form());
		newMessage.parse_command(strMessage);
		newMessage.parse_parameters(strMessage);

		switch(newMessage.command())
		{
		case message::UNKNOWN:
			{
				neolib::vecarray<std::pair<std::string::iterator, std::string::iterator>, 5> command;
				std::string space(" ");
				neolib::tokens(strMessage.begin(), strMessage.end(), space.begin(), space.end(), command, 5);
				if (!command.empty())
				{
					std::pair<internal_command::command_e, std::string> internal = get_internal_command(std::string(command[0].first, command[0].second));
					switch(internal.first)
					{
					case internal_command::OPEN:
						{
							const char* usage = "Usage: /open <server>[:<port>] [<identity>] [<password>]";
							if (command.size() < 2)
							{
								echo(usage);
								break;
							}
							const identity* theIdentity = 0;
							if (command.size() >= 3)
							{
								neolib::ci_string identityName(command[2].first, command[2].second);
								for (identity_list::const_iterator i = iModel.identity_list().begin(); theIdentity == 0 && i != iModel.identity_list().end(); ++i)
									if (identityName == i->nick_name())
										theIdentity = &*i;
								if (theIdentity == 0)
								{
									echo("Identity not found");
									break;
								}
							}
							else
								theIdentity = &iModel.default_identity();
							std::string password;
							if (command.size() >= 4)
								password = std::string(command[3].first, command[3].second);
							iConnection.connection_manager().add_connection(std::string(command[1].first, command[1].second), *theIdentity, password, true);
							break;
						}
					case internal_command::LEAVE:
						newMessage.set_command(message::PART);
						newMessage.set_command("PART");
						if (!aAll)
						{
							insert_channel_parameter(newMessage);
							new_message(newMessage);
						}
						else
							iConnection.connection_manager().new_message_all(newMessage);
						break;
					case internal_command::MSG:
					case internal_command::QUERY:
						newMessage.set_buffer_required(internal.first == internal_command::QUERY);
						if (!aAll)
						{
							if (command.size() > 2)
							{
								std::string target;
								target.assign(command[1].first, command[1].second);
								newMessage.set_command(message::PRIVMSG);
								newMessage.set_direction(message::OUTGOING);
								newMessage.parameters().clear();
								newMessage.parameters().push_back(target);
								newMessage.parameters().push_back(std::string(command[2].first, strMessage.end()));
								if (iConnection.buffer_exists(target))
								{
									buffer& theBuffer = iConnection.buffer_from_name(target);
									theBuffer.new_message(newMessage);
									if (newMessage.buffer_required())
										theBuffer.activate();
								}
								else
									new_message(newMessage);
							}
							else if (command.size() == 2)
							{
								std::string target;
								target.assign(command[1].first, command[1].second);
								buffer& theBuffer = iConnection.buffer_from_name(target);
								theBuffer.activate();
							}
						}
						else
						{
							if (command.size() > 1)
							{
								newMessage.set_command(message::PRIVMSG);
								newMessage.set_direction(message::OUTGOING);
								newMessage.parameters().clear();
								newMessage.parameters().push_back("");
								newMessage.parameters().push_back(std::string(command[1].first, strMessage.end()));
								iConnection.connection_manager().new_message_all(newMessage);
							}
						}
						break;
					case internal_command::ACTION:
						newMessage.set_command(message::PRIVMSG);
						newMessage.set_direction(message::OUTGOING);
						newMessage.set_origin(iConnection.user().msgto_form());
						newMessage.parameters().clear();
						newMessage.parameters().push_back(name());
						newMessage.parameters().push_back("");
						if (command.size() >= 2)
						{
							newMessage.parameters().back().assign(command[1].first, strMessage.end());
							newMessage.parameters().back().insert(0, "\001ACTION ");
							newMessage.parameters().back().append("\001");
							if (!aAll)
							{
								if (type() == CHANNEL || type() == USER)
									new_message(newMessage);
								else
									notify_observers(buffer_observer::NotifyMessageFailure, newMessage);
							}
							else if (!iConnection.connection_manager().new_message_all(newMessage))
								notify_observers(buffer_observer::NotifyMessageFailure, newMessage);
						}
						else
							notify_observers(buffer_observer::NotifyMessageFailure, newMessage);
						break;
					case internal_command::IGNORE_USER:
						if (newMessage.parameters().size() > 0)
							iConnection.connection_manager().ignore_list().add(any_server(iConnection.server()), irc::user(newMessage.parameters()[0], *this, false, true));
						break;
					case internal_command::UNIGNORE_USER:
						if (newMessage.parameters().size() > 0)
							iConnection.connection_manager().ignore_list().remove(any_server(iConnection.server()), irc::user(newMessage.parameters()[0], *this, false, true));
						break;
					case internal_command::RAW:
						if (command.size() < 2)
							return;
						strMessage.erase(strMessage.begin(), command[1].first);
						newMessage.parse_command(strMessage);
						newMessage.parse_parameters(strMessage, false, true);
						// fall through..
					case internal_command::UNKNOWN:
						if (!aAll)
							new_message(newMessage);
						else
							iConnection.connection_manager().new_message_all(newMessage);
						break;
					case internal_command::VERSION:
					case internal_command::CLIENTINFO:
					case internal_command::TIME:
					case internal_command::FINGER:
					case internal_command::SOURCE:
					case internal_command::USERINFO:
						if (command.size() > 1)
						{
							std::string target;
							target.assign(command[1].first, command[1].second);
							newMessage.set_command(message::PRIVMSG);
							newMessage.set_direction(message::OUTGOING);
							newMessage.parameters().clear();
							newMessage.parameters().push_back(target);
							newMessage.parameters().push_back("\001%C%\001");
							newMessage.parameters().back().replace(newMessage.parameters().back().find("%C%"), 3, internal.second);
							if (iConnection.buffer_exists(target))
							{
								buffer& theBuffer = iConnection.buffer_from_name(target);
								theBuffer.new_message(newMessage);
							}
							else
								new_message(newMessage);
						}
						break;
					case internal_command::SERVER:
						if (newMessage.parameters().size() == 1)
							iConnection.change_server(newMessage.parameters()[0]);
						else
							iConnection.change_server(std::string());
						break;
					case internal_command::CTCP:
						if (command.size() > 2)
						{
							std::string target;
							target.assign(command[1].first, command[1].second);
							newMessage.set_command(message::PRIVMSG);
							newMessage.set_direction(message::OUTGOING);
							newMessage.parameters().clear();
							newMessage.parameters().push_back(target);
							newMessage.parameters().push_back("");
							if (neolib::to_upper(std::string(command[2].first, command[2].second)) == "PING")
							{
								newMessage.parameters().back().assign("\001PING %T%\001");
								newMessage.parameters().back().replace(newMessage.parameters().back().find("%T%"), 3, neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(std::time(NULL))));
							}
							else
							{
								newMessage.parameters().back().assign("\001%C%%P%\001");
								newMessage.parameters().back().replace(newMessage.parameters().back().find("%C%"), 3, std::string(command[2].first, command[2].second));
								if (command.size() > 3)
									newMessage.parameters().back().replace(newMessage.parameters().back().find("%P%"), 3, std::string(" ") + std::string(command[3].first, strMessage.end()));
								else
									newMessage.parameters().back().replace(newMessage.parameters().back().find("%P%"), 3, std::string(""));
							}
							if (iConnection.buffer_exists(target))
							{
								buffer& theBuffer = iConnection.buffer_from_name(target);
								theBuffer.new_message(newMessage);
							}
							else
								new_message(newMessage);
						}
						break;
					case internal_command::CHAT:
						if (command.size() >= 1)
						{
							std::string target;
							target.assign(command[1].first, command[1].second);
							iModel.dcc_connection_manager().request_chat(connection(), user(target));
						}
						break;
					case internal_command::DNS:
						if (command.size() > 1)
						{
							std::string target;
							target.assign(command[1].first, command[1].second);
							iConnection.dns().new_request(target, *this);
						}
						break;
					case internal_command::SHOWPINGS:
						iConnection.show_pings();
						echo("Showing server pings.");
						break;
					case internal_command::HIDEPINGS:
						iConnection.hide_pings();
						echo("Hiding server pings.");
						break;
					case internal_command::AUTOJOIN:
						if (type() == CHANNEL)
						{
							bool added = iModel.auto_joins().add(server_key(iConnection.server().network(), "*"), iConnection.identity().nick_name(), name());
							new_message(added ? "/echo Auto-join added." : "/echo Auto-join already exists.");
						}
						else
							notify_observers(buffer_observer::NotifyMessageFailure, newMessage);
						break;
					case internal_command::AUTOREJOIN:
						if (newMessage.parameters().empty() || neolib::make_ci_string(newMessage.parameters()[0]) == "on")
						{
							iModel.connection_manager().set_auto_rejoin_on_kick(true);
							echo("auto-rejoin on kick enabled.");
						}
						else if (neolib::make_ci_string(newMessage.parameters()[0]) == "off")
						{
							iModel.connection_manager().set_auto_rejoin_on_kick(false);
							echo("auto-rejoin on kick disabled.");
						}
						else if (newMessage.parameters()[0] == "?")
						{
							if (iModel.connection_manager().auto_rejoin_on_kick())
								echo("auto-rejoin on kick is currently enabled.");
							else
								echo("auto-rejoin on kick is currently disabled.");
						}
						else
							notify_observers(buffer_observer::NotifyMessageFailure, newMessage);
						break;
					case internal_command::ALL:
						if (command.size() > 1)
						{
							std::string allCommand(command[1].first, strMessage.end());
							if (!allCommand.empty() && allCommand[0] != '/')
								allCommand = "/" + allCommand;
							new_message(allCommand, false, true);
						}
						break;
					case internal_command::CLEAR:
						if (!aAll)
							clear();
						else
							iConnection.connection_manager().clear_all();
						break;
					case internal_command::HIDE:
						{
							buffer* target = 0;
							if (command.size() == 1)
								target = this;
							else if (command.size() > 1 && 
								iConnection.buffer_exists(std::string(command[1].first, command[1].second)))
								target = &iConnection.buffer_from_name(std::string(command[1].first, command[1].second));
							if (target != 0)
								target->hide();
							else
								notify_observers(buffer_observer::NotifyMessageFailure, newMessage);
						}
						break;
					case internal_command::SHOW:
						{
							buffer* target = 0;
							if (command.size() == 1)
								target = this;
							else if (command.size() > 1 && 
								iConnection.buffer_exists(std::string(command[1].first, command[1].second)))
								target = &iConnection.buffer_from_name(std::string(command[1].first, command[1].second));
							if (target != 0)
								target->show();
							else
								notify_observers(buffer_observer::NotifyMessageFailure, newMessage);
						}
						break;
					case internal_command::DELAY:
						if (command.size() > 2)
							iDelayedCommands.push_back(delayed_command(*this, std::string(command[2].first, strMessage.end()), neolib::string_to_unsigned_integer(std::string(command[1].first, command[1].second))));
						break;
					case internal_command::ECHO:
						if (command.size() > 1)
							echo(std::string(command[1].first, strMessage.end()), aAll);
						break;
					case internal_command::XYZZY:
						new_message(iConnection.random().get(1) == 0 ? "/echo Nothing happens" : "/echo D BREAK - CONT repeats, 10:1");
						break;
					case internal_command::FINDUSER:
						if (command.size() > 1)
						{
							std::vector<const irc::user*> users = find_users(std::string(command[1].first, strMessage.end()));
							if (users.empty())
							{
								echo("-------------------------------");
								echo("No users matching \"" + std::string(command[1].first, strMessage.end()) + "\" found.");
								echo("-------------------------------");
							}
							else
							{
								echo("-------------------------------");
								echo("" + neolib::unsigned_integer_to_string<char>(users.size()) + " user(s) matching \"" + std::string(command[1].first, strMessage.end()) + "\" found:-");
								for (std::vector<const irc::user*>::const_iterator i = users.begin(); i != users.end(); ++i)
									echo("" + (*i)->nick_name());
								echo("-------------------------------");
							}
						}
						break;
					case internal_command::TIMER:
						{
							const char* const addUsage = "Usage: /timer name=<name> interval=<interval in seconds> [repeat=<repeat count>] <command>";
							const char* const deleteUsage = "Usage: /timer delete <name>";
							const char* const listUsage = "Usage: /timer list";
							if (command.size() == 1 || neolib::ci_string(command[1].first, command[1].second) == "help")
							{
								echo(addUsage);
								echo(deleteUsage);
								echo(listUsage);
							}
							else if (neolib::ci_string(command[1].first, command[1].second) == "list")
							{
								echo("-------------------------------");
								for (connection::command_timer_list::const_iterator i = iConnection.command_timers().begin(); i != iConnection.command_timers().end(); ++i)
								{
									const connection::command_timer& theTimer = i->second;
									echo(i->first + 
										": interval=" + neolib::double_to_string<char>(theTimer.duration_ms() / 1000.0, 4) + 
										" repeat=" + (theTimer.repeat() ? neolib::unsigned_integer_to_string<char>(*theTimer.repeat()) : std::string("forever")) + 
										" " + theTimer.command());
								}
								if (iConnection.command_timers().empty())
									echo("No timers running");
								echo("-------------------------------");
							}
							else if (neolib::ci_string(command[1].first, command[1].second) == "delete")
							{
								if (command.size() >= 2)
								{
									for (std::size_t i = 2; i != command.size(); ++i)
										if (iConnection.delete_command_timer(std::string(command[i].first, command[i].second)))
											echo("Timer deleted");
										else
											echo("Timer not found");
								}
								else
									echo(deleteUsage);
							}
							else
							{
								neolib::optional<std::string> timerName;
								neolib::optional<double> timerInterval;
								neolib::optional<std::size_t> timerRepeat;
								neolib::optional<std::string> timerCommand;
								for (std::size_t i = 1; i != command.size(); ++i)
								{
									std::string bit(command[i].first, command[i].second);
									if (bit[0] == '/')
									{
										timerCommand = std::string(command[i].first, strMessage.end());
										break;
									}
									else
									{
										neolib::vecarray<std::string, 2> bits;
										neolib::tokens(bit, std::string("="), bits, 2);
										if (bits.size() != 2)
										{
											echo(addUsage);
											return;
										}
										else if (neolib::make_ci_string(bits[0]) == "name")
											timerName = bits[1];
										else if (neolib::make_ci_string(bits[0]) == "interval")
											timerInterval = neolib::string_to_double(bits[1]);
										else if (neolib::make_ci_string(bits[0]) == "repeat")
											timerRepeat = neolib::string_to_unsigned_integer(bits[1]);
										else
											echo(addUsage);
									}
								}
								if (!timerName)
								{
									echo("Timer name not specified");
									break;
								}
								else if (!timerInterval)
								{
									echo("Timer interval not specified");
									break;
								}
								else if (!timerCommand)
								{
									echo("Timer command not specified");
									break;
								}	
								if (iConnection.add_command_timer(*timerName, *timerInterval, timerRepeat, *timerCommand))
									echo("Timer added");
								else
									echo("Timer replaced");
							}
						break;
						}
					}
				}
			}
			break;
		case message::PING:
			if (newMessage.parameters().size() >= 1)
			{
				newMessage.set_command(message::PRIVMSG);
				newMessage.parameters().push_back("\001PING %T%\001");
				newMessage.parameters().back().replace(newMessage.parameters().back().find("%T%"), 3, neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(std::time(NULL))));
				if (iConnection.buffer_exists(newMessage.parameters()[0]))
				{
					buffer& theBuffer = iConnection.buffer_from_name(newMessage.parameters()[0]);
					theBuffer.new_message(newMessage);
				}
				else
					new_message(newMessage);
			}
			break;
		case message::WHOIS:
			if (newMessage.parameters().size() == 1)
				connection().whois().new_request(newMessage.parameters()[0], *this);
			else
				new_message(newMessage);
			break;
		case message::WHO:
			if (newMessage.parameters().size() == 1)
				connection().who().new_request(newMessage.parameters()[0], *this);
			else
				new_message(newMessage);
			break;
		case message::KICK:
			insert_channel_parameter(newMessage);
			new_message(newMessage);
			break;
		case message::PART:
			if (!aAll)
			{
				insert_channel_parameter(newMessage);
				new_message(newMessage);
			}
			else
				iConnection.connection_manager().new_message_all(newMessage);
			break;
		case message::INVITE:
			if (newMessage.parameters().size() == 1)
				newMessage.parameters().push_back(name());
			new_message(newMessage);
			break;
		case message::TOPIC:
			insert_channel_parameter(newMessage);
			new_message(newMessage);
			break;
		case message::NOTICE:
			if (newMessage.parameters().size() >= 1)
			{
				if (iConnection.buffer_exists(newMessage.parameters()[0]))
				{
					buffer& theBuffer = iConnection.buffer_from_name(newMessage.parameters()[0]);
					theBuffer.new_message(newMessage);
				}
				else
					new_message(newMessage);
			}
			break;
		default:
			if (!aAll)
				new_message(newMessage);
			else
				iConnection.connection_manager().new_message_all(newMessage);
			break;
		}
	}

	void buffer::echo(const std::string& aMessage, bool aAll)
	{
		message newMessage(*this, message::OUTGOING);
		newMessage.set_origin(iConnection.user().msgto_form());
		newMessage.set_command("");
		newMessage.parameters().push_back(aMessage);
		newMessage.set_direction(message::INCOMING);
		if (!aAll)
			new_message(newMessage);
		else
			iConnection.connection_manager().new_message_all(newMessage);
	}

	void buffer::new_message(const message& aMessage)
	{
		switch(aMessage.direction())
		{
		case message::INCOMING:
			add_and_handle_message(aMessage);
			break;
		case message::OUTGOING:
			switch(aMessage.command())
			{
			case message::PRIVMSG:
				if (!is_ready())
				{
					notify_observers(buffer_observer::NotifyMessageFailure, aMessage);
					return;
				}
				if (!aMessage.parameters().empty())
				{
					const std::string& target = aMessage.parameters()[0];
					buffer* targetBuffer = 0;
					if (aMessage.is_ctcp())
					{
						send_message(aMessage);
						return;
					}
					if (is_channel(target))
					{
						if (iConnection.buffer_exists(target))
							targetBuffer =&iConnection.buffer_from_name(target);
					}
					else
					{
						if (iConnection.buffer_exists(target) || aMessage.buffer_required())
							targetBuffer = &iConnection.buffer_from_name(target);
					}
					if (targetBuffer == this)
					{
						if (send_message(aMessage))
							add_and_handle_message(aMessage);
					}
					else if (targetBuffer != 0)
						targetBuffer->new_message(aMessage);
					else
						send_message(aMessage);
				}
				break;
			case message::NOTICE:
				if (send_message(aMessage))
					add_and_handle_message(aMessage);
				break;
			default:
				send_message(aMessage);
				break;
			}
			break;
		}
		return;
	}

	bool buffer::find_message(model::id aMessageId, buffer*& aBuffer, message*& aMessage)
	{
		for (message_list::iterator i = iMessages.begin(); i != iMessages.end(); ++i)
			if (i->id() == aMessageId)
			{
				aBuffer = this;
				aMessage = &*i;
				return true;
			}
		return false;
	}

	void buffer::activate()
	{
		if (iClosing)
			return;
		if (iConnection.connection_manager().active_buffer() == this)
			return;
		iConnection.connection_manager().set_active_buffer(this);
		notify_observers(buffer_observer::NotifyActivate);
	}

	void buffer::deactivate()
	{
		if (iConnection.connection_manager().active_buffer() != this)
			return;
		iConnection.connection_manager().set_active_buffer(0);
	}

	void buffer::show()
	{
		notify_observers(buffer_observer::NotifyShow);
	}

	void buffer::hide()
	{
		notify_observers(buffer_observer::NotifyHide);
	}

	void buffer::reopen()
	{
		notify_observers(buffer_observer::NotifyReopen);
		iModel.reopen_buffer(*this);
	}

	void buffer::open(const message& aMessage)
	{
		notify_observers(buffer_observer::NotifyOpen, aMessage);
	}

	void buffer::close()
	{
		if (iClosing)
			return;
		if (on_close())
			iConnection.remove_buffer(*this);
	}

	casemapping::type buffer::casemapping() const
	{
		return iConnection.casemapping();
	}
}