// notify_watcher.cpp
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
#include "notify_watcher.hpp"

namespace irc
{
	void notify_action::destroy()
	{
		iValid = false;
		iWatcher.remove(*this);
	}

	notify_watcher::notify_watcher(connection_manager& aConnectionManager) : 
		iNotifyList(aConnectionManager.notify_list()), iConnectionManager(aConnectionManager)
	{
		iConnectionManager.add_observer(*this);
	}

	notify_watcher::~notify_watcher()
	{
		for (watched_buffers::iterator i = iWatchedBuffers.begin(); i != iWatchedBuffers.end(); ++i)
			(*i)->remove_observer(*this);
		for (watched_connections::iterator i = iWatchedConnections.begin(); i != iWatchedConnections.end(); ++i)
			(*i)->remove_observer(*this);
		iConnectionManager.remove_observer(*this);
	}

	void notify_watcher::remove(notify_action& aAction)
	{
		for (action_list::iterator i = iActions.begin(); i != iActions.end(); ++i)
			if ((*i).get() == &aAction)
			{
				iActions.erase(i);
				break;
			}
	}

	bool notify_watcher::action_pending(connection& aConnection, const user& aUser, const std::string& aChannel, message::command_e aCommand) const
	{
		const server_key& theServer = aConnection.server().key();
		for (action_list::const_iterator i = iActions.begin(); i != iActions.end(); ++i)
		{
			const notify_action& action = **i;
			if (server_match(action.entry().server(), theServer) && 
				(irc::make_string(aConnection, action.nick_name()) == aUser.nick_name()) &&
				(irc::make_string(aConnection, action.entry().channel()) == aChannel || action.entry().channel() == "*") && action.command() == aCommand)
				return true;
		}
		return false;
	}

	void notify_watcher::handle_message(buffer& aBuffer, const message& aMessage)
	{
		if (aMessage.is_ctcp())
			return;

		connection& theConnection = aBuffer.connection();

		buffer* activeBuffer = iConnectionManager.active_buffer();
		bool sameConnection = (activeBuffer != 0 && &activeBuffer->connection() == &theConnection);

		if (activeBuffer == &aBuffer)
			return;

		user theUser(aMessage.origin(), aBuffer);

		if (theUser.nick_name() == theConnection.nick_name() || theUser.nick_name().empty())
			return;

		std::string target = aBuffer.name();

		switch(aMessage.command())
		{
		case message::PRIVMSG:
			if (!aMessage.parameters().empty())
				target = aMessage.parameters()[0];
			if (iConnectionManager.ignore_list().ignored(aBuffer, theConnection.server(), theUser))
				return;
			break;
		case message::NOTICE:
			if (iConnectionManager.ignore_list().ignored(aBuffer, theConnection.server(), theUser))
				return;
			break;
		case message::JOIN:
		case message::PART:
			for (message::parameters_t::const_iterator i = aMessage.parameters().begin(); i != aMessage.parameters().end(); ++i)
				if (aBuffer.is_channel(*i))
				{
					target = *i;
					break;
				}
			break;
		case message::QUIT:
			if (sameConnection && activeBuffer->has_user(theUser.nick_name()))
				return;
			break;
		default:
			return;
		}
		
		if (target.empty())
			target = "*";

		if (action_pending(theConnection, theUser, target, aMessage.command()))
			return;

		if (iNotifyList.notified(theConnection, theConnection.server().key(), theUser, target, aMessage.command()))
		{
			for (notify::container_type::iterator i = iNotifyList.entries().begin(); i != iNotifyList.entries().end(); ++i)
				if (iNotifyList.matches(theConnection, **i, theConnection.server().key(), theUser, target))
				{
					notify_entry& entry = **i;
					bool gotMatch = false;
					switch(aMessage.command())
					{
					case message::PRIVMSG:
					case message::NOTICE:
						if (entry.event() & notify_entry::Message)
							gotMatch = true;
						break;
					case message::JOIN:
						if (entry.event() & notify_entry::Join)
							gotMatch = true;
						break;
					case message::PART:
					case message::QUIT:
						if (entry.event() & notify_entry::PartQuit)
							gotMatch = true;
						break;
					}
					if (gotMatch)
						notify_observers(notify_watcher_observer::NotifyAction, std::make_pair(&aBuffer, &entry), std::make_pair(theUser.nick_name(), &aMessage));
				}
		}
	}

	void notify_watcher::notify_observer(notify_watcher_observer& aObserver, notify_watcher_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		const std::pair<buffer*, notify_entry*>& param1 = *static_cast<const std::pair<buffer*, notify_entry*>*>(aParameter);
		const std::pair<std::string, const message*>& param2 = *static_cast<const std::pair<std::string, const message*>*>(aParameter2);
		notify_action_ptr newAction = aObserver.notify_action(*param1.first, *param1.second, param2.first, *param2.second);
		if (newAction.get() != 0 && newAction->valid())
			iActions.push_back(newAction);
	}

	void notify_watcher::connection_added(connection& aConnection)
	{
		iWatchedConnections.push_back(&aConnection);
		aConnection.add_observer(*this);
	}

	void notify_watcher::connection_removed(connection& aConnection)
	{
		iWatchedConnections.remove(&aConnection);
	}

	void notify_watcher::buffer_added(buffer& aBuffer)
	{
		iWatchedBuffers.push_back(&aBuffer);
		aBuffer.add_observer(*this);
	}

	void notify_watcher::buffer_removed(buffer& aBuffer)
	{
		for (action_list::iterator i = iActions.begin(); i != iActions.end();)
		{
			notify_action& action = **i;
			if (&action.buffer() == &aBuffer)
			{
				action.destroy();
				i = iActions.begin();
			}
			else
				++i;
		}

		iWatchedBuffers.remove(&aBuffer);
	}

	void notify_watcher::buffer_message(buffer& aBuffer, const message& aMessage)
	{
		if (aMessage.direction() != message::INCOMING)
			return;
		switch(aMessage.command())
		{
		case message::PRIVMSG:
		case message::NOTICE:
		case message::JOIN:
		case message::PART:
		case message::QUIT:
			handle_message(aBuffer, aMessage);
			break;
		default:
			// do nothing
			break;
		}
	}
}