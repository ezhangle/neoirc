// contacts.h
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

#ifndef CAW_GROUP
#define CAW_GROUP

#include "server.hpp"
#include "user.hpp"
#include "connection_manager.hpp"

namespace irc
{
	class contact
	{
	public:
		// types
	public:
		// construction
		contact() {}
		contact(const std::string& aName, const std::string& aGroup, const server_key& aServer, const user& aUser) : iName(aName), iGroup(aGroup), iServer(aServer), iUser(aUser) {}
	public:
		// operations
		const std::string& name() const { return iName; }
		void set_name(const std::string& aName) { iName = aName; }
		const std::string& group() const { return iGroup; }
		void set_group(const std::string& aGroup) { iGroup = aGroup; }	
		const irc::server_key& server() const { return iServer; }
		void set_server(const irc::server_key& aServer) { iServer = aServer; }
		const irc::user& user() const { return iUser; }
		void set_user(const irc::user& aUser) { iUser = aUser; }
	private:
		// attributes
		std::string iName;
		std::string iGroup;
		server_key iServer;
		irc::user iUser;
	};

	class contacts_observer
	{
		friend class contacts;
	private:
		virtual void contact_added(const contact& aEntry) = 0;
		virtual void contact_updated(const contact& aEntry, const contact& aOldEntry) = 0;
		virtual void contact_removed(const contact& aEntry) = 0;
	public:
		enum notify_type { NotifyAdded, NotifyUpdated, NotifyRemoved };
	};

	class contacts : public neolib::observable<contacts_observer>, private connection_manager_observer, connection_observer
	{
	public:
		// types
		typedef std::list<contact> container_type;
	private:
		// types
		typedef std::list<connection*> watched_connections;

	public:
		// construction
		contacts(model& aModel, connection_manager& aConnectionManager);
		~contacts();

	public:
		// operations
		const container_type& entries() const { return iEntries; }
		container_type& entries() { return iEntries; }
		bool contains(casemapping::type aCasemapping, const server_key& aServer, const user& aUser) const;
		bool add(const contact& aEntry);
		void update(contact& aExistingEntry, const contact& aNewEntry);
		void update_user(casemapping::type aCasemapping, const server_key& aServer, const user& aOldUser, const user& aNewUser);
		void remove(const contact& aEntry);
		container_type::const_iterator find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser) const;
		container_type::iterator find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser);
		container_type::const_iterator find(casemapping::type aCasemapping, const contact& aEntry) const;
		container_type::iterator find(casemapping::type aCasemapping, const contact& aEntry);
		void loading(bool aLoading) { iLoading = aLoading; }

	private:
		// implementation
		// from observable<contacts_observer>
		virtual void notify_observer(contacts_observer& aObserver, contacts_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from connection_manager_observer
		virtual void connection_added(connection& aConnection);
		virtual void connection_removed(connection& aConnection);
		virtual void filter_message(connection& aConnection, const message& aMessage, bool& aFiltered);
		virtual bool query_disconnect(const connection& aConnection) { return false; }
		virtual void query_nickname(connection& aConnection) {}
		virtual void disconnect_timeout_changed() {}
		virtual void retry_network_delay_changed() {}
		virtual void buffer_activated(buffer& aActiveBuffer) {}
		virtual void buffer_deactivated(buffer& aDeactivatedBuffer) {}
		// from connection_observer
		virtual void connection_connecting(connection& aConnection) {}
		virtual void connection_registered(connection& aConnection) {}
		virtual void buffer_added(buffer& aBuffer) {}
		virtual void buffer_removed(buffer& aBuffer) {}
		virtual void incoming_message(connection& aConnection, const message& aMessage);
		virtual void outgoing_message(connection& aConnection, const message& aMessage) {}
		virtual void connection_quitting(connection& aConnection) {}
		virtual void connection_disconnected(connection& aConnection) {}
		virtual void connection_giveup(connection& aConnection) {}

	private:
		// attributes
		model& iModel;
		connection_manager& iConnectionManager;
		watched_connections iWatchedConnections;
		container_type iEntries;
		bool iLoading;
	};

	void read_contacts_list(model& aModel, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_contacts_list(const model& aModel);
}

#endif //CAW_GROUP
