// channel_buffer.cpp
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
#include <neoirc/client/channel_buffer.hpp>
#include <neoirc/client/connection.hpp>
#include <neoirc/client/mode.hpp>

namespace irc
{
	channel_buffer::channel_buffer(irc::model& aModel, irc::connection& aConnection, const channel& aChannel) : 
		buffer(aModel, CHANNEL, aConnection, aChannel.name()), iChannel(aChannel), iCreationTime(0), iNewNamesList(true), iUpdatingUserList(false)
	{
		set_ready(false);
		update_title();
		connection().object_created(*this);
	}

	channel_buffer::~channel_buffer()
	{
		neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserListUpdating);
		while(!iUsers.empty())
		{
			neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserRemoved, iUsers.begin());
			iUsers.erase(iUsers.begin());
		}
		neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserListUpdated);
		connection().object_destroyed(*this);
	}
			
	bool channel_buffer::is_operator() const
	{
		list::const_iterator i = find_user(connection().nick_name());
		return i != iUsers.end() && i->is_operator();
	}

	channel_buffer::list::iterator channel_buffer::find_user(const std::string& aUser)
	{
		for (list::iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			if (*i == irc::user(aUser, *this))
				return i;
		return iUsers.end();
	}

	channel_buffer::list::const_iterator channel_buffer::find_user(const std::string& aUser) const
	{
		for (list::const_iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			if (*i == irc::user(aUser, *this))
				return i;
		return iUsers.end();
	}

	bool channel_buffer::has_user(const std::string& aUser) const
	{
		for (list::const_iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			if (*i == irc::user(aUser, *this))
				return true;
		return false;
	}

	const user& channel_buffer::user(const std::string& aUser) const
	{
		for (list::const_iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			if (*i == irc::user(aUser, *this))
				return *i;
		throw invalid_user();
	}

	user& channel_buffer::user(const std::string& aUser)
	{
		for (list::iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			if (*i == irc::user(aUser, *this))
				return *i;
		throw invalid_user();
	}

	bool channel_buffer::has_user(const irc::user& aUser) const
	{
		for (list::const_iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			if (*i == irc::user(std::string(), aUser.user_name(), aUser.host_name(), *this))
				return true;
		return false;
	}

	const user& channel_buffer::user(const irc::user& aUser) const
	{
		typedef std::vector<const irc::user*> matches_t;
		matches_t matches;
		for (list::const_iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			if (*i == irc::user(std::string(), aUser.user_name(), aUser.host_name(), *this))
				matches.push_back(&*i);
		if (!matches.empty())
		{
			for (matches_t::const_iterator i = matches.begin(); i != matches.end(); ++i)
				if ((*i)->nick_name() == aUser.nick_name())
					return **i;
			return *matches[0];
		}
		throw invalid_user();
	}

	user& channel_buffer::user(const irc::user& aUser)
	{
		for (list::iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			if (*i == irc::user(std::string(), aUser.user_name(), aUser.host_name(), *this))
				return *i;
		throw user_not_found();
	}

	std::vector<const user*> channel_buffer::find_users(const std::string& aSearchString) const
	{
		std::vector<neolib::ci_string> words;
		neolib::tokens(aSearchString, std::string(" "), words);
		std::vector<const irc::user*> ret;
		for (list::const_iterator i = iUsers.begin(); i != iUsers.end(); ++i)
		{
			bool found = false;
			for (std::vector<neolib::ci_string>::const_iterator j = words.begin(); !found && j != words.end(); ++j)
				if (neolib::wildcard_match(irc::make_string(*this, i->nick_name()), irc::make_string(*this, *j)) ||
					neolib::wildcard_match(neolib::make_ci_string(i->user_name()), *j) ||
					neolib::wildcard_match(neolib::make_ci_string(i->host_name()), *j) ||
					neolib::wildcard_match(neolib::make_ci_string(i->full_name()), *j))
					found = true;
			if (found)
				ret.push_back(&*i);
		}
		return ret;
	}

	void channel_buffer::update_title()
	{
		std::string newTitle = name();
		if (!iMode.empty())
			newTitle = newTitle + " (" + iMode + ")";
		if (!iTopic.empty())
			newTitle = newTitle + " - " + iTopic;
		set_title(newTitle);
	}

	bool channel_buffer::on_close()
	{
		if (is_ready())
		{
			message partMessage(*this, message::OUTGOING);
			partMessage.set_origin("");
			partMessage.set_command(message::PART);
			partMessage.parameters().push_back(name());
			new_message(partMessage);
		}
		return true;
	}

	void channel_buffer::on_set_ready()
	{
		buffer::on_set_ready();
		if (!is_ready())
			iNewNamesList = true;
	}

	bool channel_buffer::add_message(const message& aMessage)
	{
		if (aMessage.direction() == message::INCOMING)
		{
			switch(aMessage.command())
			{
			case message::RPL_NAMREPLY:
			case message::RPL_ENDOFNAMES:
			case message::RPL_CHANNELMODEIS:
			case message::RPL_CREATIONTIME:
				// don't add as these messages should not be displayed
				return false;
			default:
				break;
			}
		}
		return buffer::add_message(aMessage);
	}

	void channel_buffer::handle_message(const message& aMessage)
	{
		switch(aMessage.command())
		{
		case message::TOPIC:
		case message::RPL_TOPIC:
			iTopic = aMessage.content();
			update_title();
			break;
		case message::RPL_CHANNELMODEIS:
			{
				iMode = "";
				bool skipFirst = true;
				for (message::parameters_t::const_iterator i = aMessage.parameters().begin(); i != aMessage.parameters().end(); ++i)
				{
					if (!skipFirst)
					{
						if (!iMode.empty())
							iMode += " ";
						iMode += *i;
					}
					else
						skipFirst = false;
				}
				update_title();
			}
			break;
		case message::RPL_CREATIONTIME:
			if (aMessage.parameters().size() > 1)
				iCreationTime = neolib::string_to_integer(aMessage.parameters()[1]);
			break;
		case message::RPL_NAMREPLY:
			{
				if (iNewNamesList)
				{
					iNewNamesList = false;
					iUpdatingUserList = true;
					neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserListUpdating);
					while(!iUsers.empty())
					{
						neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserRemoved, iUsers.begin());
						iUsers.erase(iUsers.begin());
					}
				}
				std::size_t names_param = 1;
				if ((aMessage.parameters()[0][0] == '=' || 
					aMessage.parameters()[0][0] == '*' ||
					aMessage.parameters()[0][0] == '@') && 
					aMessage.parameters()[0].size() == 1)
				{
					names_param = 2;
				}
				if (aMessage.parameters().size() > names_param)
				{
					typedef std::vector<std::pair<std::string::const_iterator, std::string::const_iterator> > names_t;
					names_t names;
					neolib::tokens(aMessage.parameters()[names_param], std::string(" "), names);
					for (names_t::const_iterator i = names.begin(); i != names.end(); ++i)
					{
						channel_user_list::iterator theUser;
						neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserAdded,
							theUser = iUsers.insert(channel_user(*this, std::string(i->first, i->second), *this)));
						if (theUser->nick_name() == connection().nick_name())
							*theUser = connection().user();
					}
				}
			}
			break;
		case message::NICK:
			{
				irc::user theUser(aMessage.origin(), *this);
				list::iterator i = find_user(theUser.nick_name());
				if (i != iUsers.end())
				{
					channel_user updatedUser(*i);
					if (!aMessage.parameters().empty())
						updatedUser.nick_name() = aMessage.parameters()[0];
					neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserUpdated, i, iUsers.insert(updatedUser));
					iUsers.erase(i);
				}
			}
			break;
		case message::JOIN:
			{
				channel_user theUser(*this, aMessage.origin(), *this);
				if (theUser.nick_name() == connection().nick_name())
					theUser = connection().user();
				if (irc::make_string(*this, theUser.nick_name()) != connection().nick_name())
					neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserAdded, iUsers.insert(theUser));
			}
			break;
		case message::PART:
			{
				list::iterator i = find_user(irc::user(aMessage.origin(), *this).nick_name());
				if (i != iUsers.end())
				{
					neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserRemoved, i);
					iUsers.erase(i);
				}
			}
			break;
		case message::KICK:
			{
				if (aMessage.parameters().size() >= 2)
				{
					irc::user theUser(aMessage.parameters()[1], *this);
					if (irc::make_string(*this, theUser.nick_name()) == connection().nick_name())
						set_ready(false);
					list::iterator i = find_user(theUser.nick_name());
					if (i != iUsers.end())
					{
						neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserRemoved, i);
						iUsers.erase(i);
					}
				}
			}
			break;
		case message::PRIVMSG:
			{
				irc::user theUser(aMessage.origin(), *this);
				list::iterator i = find_user(theUser.nick_name());
				if (i != iUsers.end())
					i->set_last_message_id(aMessage.id());
			}
			break;
		case message::QUIT:
			{
				list::iterator i = find_user(irc::user(aMessage.origin(), *this).nick_name());
				if (i != iUsers.end())
				{
					neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserRemoved, i);
					iUsers.erase(i);
				}
			}
			break;
		case message::RPL_ENDOFNAMES:
			iNewNamesList = true;
			iUpdatingUserList = false;
			neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserListUpdated);
			break;
		case message::MODE:
			{
				bool refreshChannelModes = false;
				mode::list modes;
				parse_mode(aMessage, modes);
				typedef std::map<string, std::pair<list::iterator, channel_user> > changed_users_t;
				changed_users_t changedUsers;
				for (mode::list::iterator i = modes.begin(); i != modes.end(); ++i)
				{
					irc::mode& currentMode = *i;
					switch(currentMode.iType)
					{
					case mode::BAN:
					case mode::EXCEPT:
					case mode::INVITE:
						// do nothing
						break;
					case mode::OPERATOR:
					case mode::VOICE:
					default:
						if (connection().is_prefix_mode(currentMode.iType))
						{
							string theNickName = irc::make_string(*this, currentMode.iParameter);
							changed_users_t::iterator changedUser = changedUsers.find(theNickName);
							if (changedUser == changedUsers.end())
							{
								list::iterator existingUser = find_user(currentMode.iParameter);
								if (existingUser != iUsers.end())
									changedUser = changedUsers.insert(std::make_pair(theNickName, std::make_pair(existingUser, *existingUser))).first;
							}
							if (changedUser == changedUsers.end())
								continue;
							if (currentMode.iDirection == mode::ADD)
							{
								std::string::size_type m = changedUser->second.second.modes().find(currentMode.iType);
								if (m == std::string::npos)
									changedUser->second.second.modes() += currentMode.iType;
							}
							else
							{
								std::string::size_type m = changedUser->second.second.modes().find(currentMode.iType);
								if (m != std::string::npos)
									changedUser->second.second.modes().erase(m, 1);
							}
						}
						else
							refreshChannelModes = true;
						break;
					}
				}
				for (changed_users_t::iterator i = changedUsers.begin(); i != changedUsers.end(); ++i)
				{
					neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserUpdated, i->second.first, iUsers.insert(i->second.second));
					iUsers.erase(i->second.first);
				}
				if (refreshChannelModes && !iMode.empty())
				{
					message modeMessage(*this, message::OUTGOING);
					modeMessage.set_command(message::MODE);
					modeMessage.parameters().push_back(name());
					connection().send_message(*this, modeMessage);
				}
			}
		}
		buffer::handle_message(aMessage);
	}

	void channel_buffer::user_new_host_info(const irc::user& aUser)
	{
		for (list::iterator i = iUsers.begin(); i != iUsers.end(); ++i)
		{
			if (&*i == &aUser)
			{
				neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserHostInfo, i);
				break;
			}
		}
	}

	void channel_buffer::user_away_status_changed(const irc::user& aUser)
	{
		for (list::iterator i = iUsers.begin(); i != iUsers.end(); ++i)
		{
			if (&*i == &aUser)
			{
				neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyUserAwayStatus, i);
				break;
			}
		}
	}

	void channel_buffer::joining()
	{
		neolib::observable<channel_buffer_observer>::notify_observers(channel_buffer_observer::NotifyJoiningChannel);
	}

	void channel_buffer::add_observer(channel_buffer_observer& aObserver)
	{
		for (list::iterator i = iUsers.begin(); i != iUsers.end(); ++i)
		{
			aObserver.user_added(*this, i);
		}
	}

	void channel_buffer::notify_observer(channel_buffer_observer& aObserver, channel_buffer_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case channel_buffer_observer::NotifyJoiningChannel:
			aObserver.joining_channel(*this);
			break;
		case channel_buffer_observer::NotifyUserAdded:
			aObserver.user_added(*this, *static_cast<list::iterator*>(const_cast<void*>(aParameter)));
			break;
		case channel_buffer_observer::NotifyUserUpdated:
			aObserver.user_updated(*this, *static_cast<list::iterator*>(const_cast<void*>(aParameter)), *static_cast<list::iterator*>(const_cast<void*>(aParameter2)));
			break;
		case channel_buffer_observer::NotifyUserRemoved:
			aObserver.user_removed(*this, *static_cast<list::iterator*>(const_cast<void*>(aParameter)));
			break;
		case channel_buffer_observer::NotifyUserHostInfo:
			aObserver.user_host_info(*this, *static_cast<list::iterator*>(const_cast<void*>(aParameter)));
			break;
		case channel_buffer_observer::NotifyUserAwayStatus:
			aObserver.user_away_status(*this, *static_cast<list::iterator*>(const_cast<void*>(aParameter)));
			break;
		case channel_buffer_observer::NotifyUserListUpdating:
			aObserver.user_list_updating(*this);
			break;
		case channel_buffer_observer::NotifyUserListUpdated:
			aObserver.user_list_updated(*this);
			break;
		}
	}
}