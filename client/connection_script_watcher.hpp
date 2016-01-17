// connection_script_watcher.h
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

#ifndef IRC_CLIENT_CONNECTION_SCRIPT_WATCHER
#define IRC_CLIENT_CONNECTION_SCRIPT_WATCHER

#include "connection_script.hpp"
#include "connection_manager.hpp"
#include "connection.hpp"

namespace irc
{
	class connection_script_watcher : private connection_manager_observer, private connection_observer
	{
	public:
		// types

	public:
		// construction
		connection_script_watcher(neolib::random& aRandom, connection_manager& aConnectionManager, identity_list& aIdentityList, server_list& aServerList);
		~connection_script_watcher();

	public:
		// operations

	private:
		// implementation
		// from connection_manager_observer
		virtual void connection_added(connection& aConnection);
		virtual void connection_removed(connection& aConnection);
		virtual void filter_message(connection& aConnection, const message& aMessage, bool& aFiltered) {}
		virtual bool query_disconnect(const connection& aConnection) { return false; }
		virtual void query_nickname(connection& aConnection) {}
		virtual void disconnect_timeout_changed() {}
		virtual void retry_network_delay_changed() {}
		virtual void buffer_activated(buffer& aActiveBuffer) {}
		virtual void buffer_deactivated(buffer& aDeactivatedBuffer) {}
		// from connection_observer
		virtual void connection_connecting(connection& aConnection) {}
		virtual void connection_registered(connection& aConnection);
		virtual void buffer_added(buffer& aBuffer) {}
		virtual void buffer_removed(buffer& aBuffer) {}
		virtual void incoming_message(connection& aConnection, const message& aMessage) {}
		virtual void outgoing_message(connection& aConnection, const message& aMessage) {}
		virtual void connection_quitting(connection& aConnection) {}
		virtual void connection_disconnected(connection& aConnection) {}
		virtual void connection_giveup(connection& aConnection) {}

	private:
		// attributes
		neolib::random& iRandom;
		connection_manager& iConnectionManager;
		identity_list& iIdentityList;
		server_list& iServerList;
		connection_scripts& iConnectionScripts;
		typedef std::list<connection*> watched_connections;
		watched_connections iWatchedConnections;
	};
}

#endif //IRC_CLIENT_CONNECTION_SCRIPT_WATCHER
