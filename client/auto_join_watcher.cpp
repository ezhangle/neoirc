// auto_join_watcher.cpp
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
#include "auto_join_watcher.hpp"

namespace irc
{
	auto_join_watcher::auto_join_watcher(neolib::random& aRandom, connection_manager& aConnectionManager, identity_list& aIdentityList, server_list& aServerList) : 
		iRandom(aRandom), iConnectionManager(aConnectionManager), iIdentityList(aIdentityList), iServerList(aServerList), iAutoJoinList(aConnectionManager.auto_joins()), iIsAutoJoin(false)
	{
		iConnectionManager.add_observer(*this);
	}

	auto_join_watcher::~auto_join_watcher()
	{
		for (watched_connections::iterator i = iWatchedConnections.begin(); i != iWatchedConnections.end(); ++i)
			(*i)->remove_observer(*this);
		iConnectionManager.remove_observer(*this);
	}

	void auto_join_watcher::startup_connect()
	{
		typedef std::pair<std::string, server_key> connection_entry;
		typedef std::map<connection_entry, unsigned int> connections;
		unsigned int orderNumber = 0;
		connections newConnections;
		for (auto_joins::container_type::const_iterator i = iAutoJoinList.entries().begin(); i != iAutoJoinList.entries().end(); ++i)
		{
			connection_entry newEntry(std::make_pair(i->nick_name(), i->server()));
			if (newConnections.find(newEntry) == newConnections.end())
				newConnections[newEntry] = orderNumber++;
		}
		for (connections::iterator i = newConnections.begin(); i != newConnections.end();)
		{
			const connection_entry& theEntry = i->first;
			bool removed = false;
			if (theEntry.second.second == "*")
				for (connections::iterator j = newConnections.begin(); !removed && j != newConnections.end(); ++j)
					if (j->first.first == theEntry.first && j->first.second.first == theEntry.second.first && j->first.second.second != "*")
					{
						if (j->second > i->second)
							j->second = i->second;
						newConnections.erase(i++);
						removed = true;
					}
			if (!removed)
				++i;
		}
		typedef std::vector<std::pair<std::pair<std::string, server_key>, unsigned int> > sorted_connections;
		sorted_connections sortedConnections(newConnections.begin(), newConnections.end());
		struct sorter
		{
			bool operator()(const sorted_connections::value_type& left, const sorted_connections::value_type& right) const
			{
				return left.second < right.second;
			}
		};
		std::sort(sortedConnections.begin(), sortedConnections.end(), sorter());
		for (sorted_connections::iterator c = sortedConnections.begin(); c != sortedConnections.end(); ++c)
		{
			const connection_entry& theEntry = c->first;
			bool found = false;
			for (identity_list::const_iterator i = iIdentityList.begin(); !found && i != iIdentityList.end(); ++i)
				if (i->nick_name() == theEntry.first)
					if (theEntry.second.second != "*")
					{
						for (server_list::const_iterator s = iServerList.begin(); !found && s != iServerList.end(); ++s)
							if (s->key() == theEntry.second)
							{
								iConnectionManager.add_connection(*s, *i);
								found = true;
							}
					}
					else
					{
						int number = 0;
						for (server_list::const_iterator s = iServerList.begin(); !found && s != iServerList.end(); ++s)
							if (s->key().first == theEntry.second.first)
								++number;
						number = iRandom.get(0, number-1);
						for (server_list::const_iterator s = iServerList.begin(); !found && s != iServerList.end(); ++s)
							if (s->key().first == theEntry.second.first && --number < 0)
							{
								iConnectionManager.add_connection(*s, *i);
								found = true;
							}
					}
		}
	}

	bool auto_join_watcher::join_pending(connection& aConnection, const std::string& aChannelName) const
	{
		pending_joins::const_iterator c = iPendingJoins.find(&aConnection);
		return c != iPendingJoins.end() && c->second.find(irc::make_string(aConnection, aChannelName)) != c->second.end();
	}

	void auto_join_watcher::process_connection(connection& aConnection)
	{
		iIsAutoJoin = true;
		for (auto_joins::container_type::const_iterator i = iAutoJoinList.entries().begin(); i != iAutoJoinList.entries().end(); ++i)
		{
			const auto_join& entry = *i;
			neolib::vecarray<std::string, 2> bits;
			neolib::tokens(entry.channel(), std::string(" "), bits, 2);
			bits.resize(2);
			const std::string& channelName = bits[0];
			const std::string& channelKey = bits[1];
			if (entry.nick_name() == aConnection.identity().nick_name() &&
				entry.server().first == aConnection.server().key().first &&
				(entry.server().second == aConnection.server().key().second ||
				entry.server().second == "*") &&
				!aConnection.buffer_exists(channelName))
			{
				if (aConnection.registered())
				{
					message joinMessage(aConnection, message::OUTGOING);
					joinMessage.set_command(message::JOIN);
					joinMessage.parse_parameters("JOIN " + entry.channel());
					aConnection.send_message(joinMessage);
				}
				else if (iConnectionManager.create_channel_buffer_upfront())
				{
					aConnection.server_buffer(true);
					aConnection.channel_buffer_from_name(channelName);
					if (!channelKey.empty())
						iConnectionManager.add_key(aConnection, channelName, channelKey);
					else
						iConnectionManager.remove_key(aConnection, channelName);
				}
			}
		}
		iIsAutoJoin = false;
	}

	void auto_join_watcher::connection_added(connection& aConnection)
	{
		iWatchedConnections.push_back(&aConnection);
		aConnection.add_observer(*this);
	}

	void auto_join_watcher::connection_removed(connection& aConnection)
	{
		iWatchedConnections.remove(&aConnection);
		pending_joins::iterator i = iPendingJoins.find(&aConnection);
		if (i != iPendingJoins.end())
			iPendingJoins.erase(i);
	}

	void auto_join_watcher::connection_connecting(connection& aConnection)
	{
		process_connection(aConnection);
	}

	void auto_join_watcher::connection_registered(connection& aConnection)
	{
		process_connection(aConnection);
	}

	void auto_join_watcher::incoming_message(connection& aConnection, const message& aMessage)
	{
		channel_list::iterator pje = iPendingJoins[&aConnection].end();
		switch(aMessage.command())
		{
		case message::JOIN:
			{
				if (irc::make_string(aConnection, user(aMessage.origin(), aConnection).nick_name()) != aConnection.nick_name())
					return;
				std::string target;
				for (message::parameters_t::const_iterator i = aMessage.parameters().begin(); i != aMessage.parameters().end(); ++i)
					if (aConnection.is_channel(*i))
					{
						target = *i;
						break;
					}
				if (target.empty())
					return;
				pje = iPendingJoins[&aConnection].find(irc::make_string(aConnection, target));
			}
			break;
		case message::ERR_BANNEDFROMCHAN:
		case message::ERR_INVITEONLYCHAN:
		case message::ERR_BADCHANNELKEY:
		case message::ERR_CHANNELISFULL:            
		case message::ERR_BADCHANMASK:
		case message::ERR_NOSUCHCHANNEL:
		case message::ERR_TOOMANYCHANNELS:
		case message::ERR_TOOMANYTARGETS:
		case message::ERR_UNAVAILRESOURCE:
			if (aMessage.parameters().size() >= 1)
				pje = iPendingJoins[&aConnection].find(irc::make_string(aConnection, aMessage.parameters()[0]));
			break;
		default:
			return;
		}
		if (pje != iPendingJoins[&aConnection].end())
			iPendingJoins[&aConnection].erase(pje);
	}

	void auto_join_watcher::outgoing_message(connection& aConnection, const message& aMessage)
	{
		if (aMessage.command() != message::JOIN || aMessage.parameters().empty())
			return;
		if (aConnection.buffer_exists(aMessage.parameters()[0]) &&
			aConnection.buffer_from_name(aMessage.parameters()[0]).is_ready())
			return;
		iPendingJoins[&aConnection].insert(irc::make_string(aConnection, aMessage.parameters()[0]));
	}

	void auto_join_watcher::connection_disconnected(connection& aConnection)
	{
		pending_joins::iterator i = iPendingJoins.find(&aConnection);
		if (i != iPendingJoins.end())
			iPendingJoins.erase(i);
	}
}