// auto_mode_watcher.cpp
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
#include <neoirc/client/auto_mode_watcher.hpp>

namespace irc
{
	auto_mode_watcher::auto_mode_watcher(connection_manager& aConnectionManager) : 
		iConnectionManager(aConnectionManager), iAutoModeList(aConnectionManager.auto_mode_list())
	{
		iConnectionManager.add_observer(*this);
	}

	auto_mode_watcher::~auto_mode_watcher()
	{
		for (watched_connections::iterator i = iWatchedConnections.begin(); i != iWatchedConnections.end(); ++i)
			(*i)->remove_observer(*this);
		for (watched_channels::iterator i = iWatchedChannels.begin(); i != iWatchedChannels.end(); ++i)
			(*i)->neolib::observable<channel_buffer_observer>::remove_observer(*this);
		iConnectionManager.remove_observer(*this);
	}

	void auto_mode_watcher::process_user(connection& aConnection, const user& aUser, const channel_buffer& aChannel)
	{
		if (!aChannel.is_operator())
			return;
		if (aUser.nick_name() == aConnection.nick_name())
			return;
		if (iConnectionManager.auto_mode_list().has_auto_mode(aConnection, aConnection.server().key(), aUser, aChannel.name()))
		{
			for (auto_mode::container_type::iterator i = iAutoModeList.entries().begin(); i != iAutoModeList.entries().end(); ++i)
				if (iAutoModeList.matches(aConnection, *i, aConnection.server().key(), aUser, aChannel.name()))
				{
					auto_mode_entry& entry = *i;
					message modeMessage(aConnection, message::OUTGOING);
					modeMessage.set_command(message::MODE);
					modeMessage.parameters().push_back(aChannel.name());
					bool send = false;
					switch(entry.type())
					{
					case auto_mode_entry::Op:
						if (!aUser.is_operator())
						{
							modeMessage.parameters().push_back("+o");
							modeMessage.parameters().push_back(aUser.nick_name());
							send = true;
						}
						break;
					case auto_mode_entry::Voice:
						if (!aUser.is_voice() && !aUser.is_operator())
						{
							modeMessage.parameters().push_back("+v");
							modeMessage.parameters().push_back(aUser.nick_name());
							send = true;
						}
						break;
					case auto_mode_entry::BanKick:
						modeMessage.parameters().push_back("+b");
						modeMessage.parameters().push_back(aUser.ban_mask());
						send = true;
						break;
					}
					if (send)
						aConnection.send_message(modeMessage);
					if (entry.type() == auto_mode_entry::BanKick)
					{
						message kickMessage(aConnection, message::OUTGOING);
						kickMessage.set_command(message::KICK);
						kickMessage.parameters().push_back(aChannel.name());
						kickMessage.parameters().push_back(aUser.nick_name());
						kickMessage.parameters().push_back(entry.data());
						aConnection.send_message(kickMessage);
					}
				}
		}
	}

	void auto_mode_watcher::connection_added(connection& aConnection)
	{
		aConnection.add_observer(*this);
		iWatchedConnections.push_back(&aConnection);
	}

	void auto_mode_watcher::connection_removed(connection& aConnection)
	{
		iWatchedConnections.remove(&aConnection);
	}

	void auto_mode_watcher::buffer_added(buffer& aBuffer)
	{
		if (aBuffer.type() == buffer::CHANNEL)
		{
			static_cast<channel_buffer&>(aBuffer).neolib::observable<channel_buffer_observer>::add_observer(*this);
			iWatchedChannels.push_back(static_cast<channel_buffer*>(&aBuffer));
		}
	}

	void auto_mode_watcher::buffer_removed(buffer& aBuffer)
	{
		if (aBuffer.type() == buffer::CHANNEL)
			iWatchedChannels.remove(static_cast<channel_buffer*>(&aBuffer));
	}

	void auto_mode_watcher::user_added(channel_buffer& aBuffer, channel_user_list::iterator aUser)
	{
		process_user(aBuffer.connection(), *aUser, aBuffer);
	}

	void auto_mode_watcher::user_updated(channel_buffer& aBuffer, channel_user_list::iterator aOldUser, channel_user_list::iterator aNewUser)
	{
		if (aBuffer.connection().nick_name() == aNewUser->nick_name() &&
			!aOldUser->is_operator() && aNewUser->is_operator())
		{
			for (channel_user_list::const_iterator i = aBuffer.users().begin(); i != aBuffer.users().end(); ++i)
				process_user(aBuffer.connection(), *i, aBuffer);
		}
		else
		{
			user::difference_e userDifference = aNewUser->difference(*aOldUser);
			if (userDifference != user::DifferenceNone &&	userDifference != user::DifferenceMode)
				process_user(aBuffer.connection(), *aNewUser, aBuffer);
		}
	}

	void auto_mode_watcher::user_host_info(channel_buffer& aBuffer, channel_user_list::iterator aUser)
	{
		process_user(aBuffer.connection(), *aUser, aBuffer);
	}
}