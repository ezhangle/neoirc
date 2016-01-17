// notify.h
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

#ifndef IRC_CLIENT_NOTIFY
#define IRC_CLIENT_NOTIFY

#include "server.hpp"
#include "user.hpp"
#include "message.hpp"
#include "connection.hpp"

namespace irc
{
	class notify_entry
	{
	public:
		// types
		enum action_e { Notice, PopUp, Sound };
		enum event_e { Message = 0x01, Join = 0x02, PartQuit = 0x04 };
	public:
		// construction
		notify_entry() : iChannel("*"), iAction(Notice), iEvent(Message) {}
		notify_entry(const notify_entry& aOther) : iServer(aOther.iServer), iUser(aOther.iUser), iChannel(aOther.iChannel), iAction(aOther.iAction), iEvent(aOther.iEvent), iData(aOther.iData) {}
		notify_entry(const server_key& aServer, const user& aUser, const std::string& aChannel, action_e aAction, event_e aEvent, const std::string& aData) : iServer(aServer), iUser(aUser), iChannel(aChannel), iAction(aAction), iEvent(aEvent), iData(aData) {}
	public:
		// operations
		const irc::server_key& server() const { return iServer; }
		irc::server_key& server() { return iServer; }
		const irc::user& user() const { return iUser; }
		irc::user& user() { return iUser; }
		const std::string& channel() const { return iChannel; }
		std::string& channel() { return iChannel; }
		action_e action() const { return iAction; }
		action_e& action() { return iAction; }
		event_e event() const { return iEvent; }
		event_e& event() { return iEvent; }
		const std::string& data() const { return iData; }
		std::string& data() { return iData; }
		bool operator==(const notify_entry& aOther) const 
		{ 
			return server_match(iServer, aOther.iServer) && 
				iUser == aOther.iUser && 
				(irc::make_string(casemapping::ascii, iChannel) == aOther.iChannel || iChannel == "*" || aOther.iChannel == "*") &&
				iAction == aOther.iAction &&
				(iEvent & aOther.iEvent) != 0; 
		}
	private:
		// attributes
		server_key iServer;
		irc::user iUser;
		std::string iChannel;
		action_e iAction;
		event_e iEvent;
		std::string iData;
	};

	typedef std::shared_ptr<notify_entry> notify_entry_ptr;

	class notify_list_observer
	{
		friend class notify;
	private:
		virtual void notify_added(const notify_entry& aEntry) = 0;
		virtual void notify_updated(const notify_entry& aEntry) = 0;
		virtual void notify_removed(const notify_entry& aEntry) = 0;
	public:
		enum notify_type { NotifyAdded, NotifyUpdated, NotifyRemoved };
	};

	class notify : public neolib::observable<notify_list_observer>
	{
	public:
		// types
		typedef std::list<notify_entry_ptr> container_type;

	public:
		// construction
		notify(model& aModel) : iModel(aModel), iLoading(false) {}

	public:
		// operations
		const container_type& entries() const { return iEntries; }
		container_type& entries() { return iEntries; }
		bool notified(const connection& aConnection, const server_key& aServer, const user& aUser, const std::string& aChannel) const;
		bool notified(const connection& aConnection, const server_key& aServer, const user& aUser, const std::string& aChannel, message::command_e aMessage) const;
		bool add(const notify_entry& aEntry);
		void update(notify_entry& aExistingEntry, const notify_entry& aNewEntry);
		void update_user(const connection& aConnection, const server_key& aServer, const user& aOldUser, const user& aNewUser);
		void remove(const notify_entry& aEntry);
		container_type::const_iterator find(const connection& aConnection, const server_key& aServer, const user& aUser, const std::string& aChannel) const;
		container_type::iterator find(const connection& aConnection, const server_key& aServer, const user& aUser, const std::string& aChannel);
		container_type::const_iterator find(const connection& aConnection, const notify_entry& aEntry) const;
		container_type::iterator find(const connection& aConnection, const notify_entry& aEntry);
		bool matches(const connection& aConnection, const notify_entry& aEntry, const server_key& aServer, const user& aUser, const std::string& aChannel) const;
		notify_entry_ptr share(const connection& aConnection, const notify_entry& aEntry);
		void loading(bool aLoading) { iLoading = aLoading; }

	private:
		// implementation
		// from neolib::observable<notify_list_observer>
		virtual void notify_observer(notify_list_observer& aObserver, notify_list_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);

	private:
		// attributes
		model& iModel;
		container_type iEntries;
		bool iLoading;
	};

	void read_notify_list(model& aModel, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_notify_list(const model& aModel);
}

#endif //IRC_CLIENT_NOTIFY
