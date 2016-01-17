// connection_script_watcher.cpp
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
#include "connection_script_watcher.hpp"

namespace irc
{
	connection_script_watcher::connection_script_watcher(neolib::random& aRandom, connection_manager& aConnectionManager, identity_list& aIdentityList, server_list& aServerList) : 
		iRandom(aRandom), iConnectionManager(aConnectionManager), iIdentityList(aIdentityList), iServerList(aServerList), iConnectionScripts(aConnectionManager.connection_scripts())
	{
		iConnectionManager.add_observer(*this);
	}

	connection_script_watcher::~connection_script_watcher()
	{
		for (watched_connections::iterator i = iWatchedConnections.begin(); i != iWatchedConnections.end(); ++i)
			(*i)->remove_observer(*this);
		iConnectionManager.remove_observer(*this);
	}

	void connection_script_watcher::connection_added(connection& aConnection)
	{
		iWatchedConnections.push_back(&aConnection);
		aConnection.add_observer(*this);
	}

	void connection_script_watcher::connection_removed(connection& aConnection)
	{
		iWatchedConnections.remove(&aConnection);
	}

	void connection_script_watcher::connection_registered(connection& aConnection)
	{
		for (connection_scripts::container_type::const_iterator i = iConnectionScripts.entries().begin(); i != iConnectionScripts.entries().end(); ++i)
		{
			const connection_script& entry = *i;
			if (entry.enabled() &&
				entry.nick_name() == aConnection.identity().nick_name() &&
				entry.server().first == aConnection.server().key().first &&
				(entry.server().second == aConnection.server().key().second ||
				entry.server().second == "*"))
			{
				typedef std::vector<std::string> commands_t;
				commands_t commands;
				neolib::tokens(entry.script(), std::string("\r\n"), commands);
				for (commands_t::iterator j = commands.begin(); j != commands.end(); ++j)
					aConnection.server_buffer().new_message(*j);
			}
		}
	}
}