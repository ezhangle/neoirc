// notify_watcher.h
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

#ifndef IRC_CLIENT_NOTIFY_WATCHER
#define IRC_CLIENT_NOTIFY_WATCHER

#include <neoirc/client/notify.hpp>
#include <neoirc/client/connection_manager.hpp>
#include <neoirc/client/connection.hpp>
#include <neoirc/client/buffer.hpp>
#include <neoirc/client/message.hpp>

namespace irc
{
	class notify_watcher;

	class notify_action
	{
	public:
		// construction
		notify_action(notify_watcher& aWatcher, const buffer& aBuffer, const notify_entry_ptr& aEntry, const std::string& aNickName, const message& aMessage) : iValid(true), iWatcher(aWatcher), iBuffer(aBuffer), iEntry(aEntry), iNickName(aNickName), iMessage(aMessage) {}
		virtual ~notify_action() {}

	public:
		// operations
		void destroy();
		const irc::buffer& buffer() const { check(); return iBuffer; }
		const notify_entry& entry() const { return *iEntry; }
		const std::string& nick_name() const { return iNickName; }
		message::command_e command() const { return iMessage.command(); }
		const irc::message& message() const { return iMessage; }
		bool valid() const { return iValid; }

	private:
		// implementation
		void check() const { if (!iValid) throw std::logic_error("irc::notify_action::check"); }

	protected:
		// attributes
		bool iValid;
		notify_watcher& iWatcher;
		const irc::buffer& iBuffer;
		notify_entry_ptr iEntry;
		std::string iNickName;
		irc::message iMessage;
	};

	typedef std::shared_ptr<notify_action> notify_action_ptr;

	class notify_watcher_observer
	{
		friend class notify_watcher;
	private:
		virtual notify_action_ptr notify_action(const buffer& aBuffer, const notify_entry& aEntry, const std::string& aNickName, const message& aMessage) = 0;
	public:
		enum notify_type { NotifyAction };
	};

	class notify_watcher : public neolib::observable<notify_watcher_observer>, private connection_manager_observer, private connection_observer, private buffer_observer
	{
	public:
		// types

	public:
		// construction
		notify_watcher(connection_manager& aConnectionManager);
		~notify_watcher();

	public:
		// operations
		void remove(notify_action& aAction);

	private:
		// implementation
		bool action_pending(connection& aConnection, const user& aUser, const std::string& aChannel, message::command_e aCommand) const;
		void handle_message(buffer& aBuffer, const message& aMessage);
		// from neolib::observable<notify_watcher_observer>
		virtual void notify_observer(notify_watcher_observer& aObserver, notify_watcher_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
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
		virtual void connection_registered(connection& aConnection) {}
		virtual void buffer_added(buffer& aBuffer);
		virtual void buffer_removed(buffer& aBuffer);
		virtual void incoming_message(connection& aConnection, const message& aMessage) {}
		virtual void outgoing_message(connection& aConnection, const message& aMessage) {}
		virtual void connection_quitting(connection& aConnection) {}
		virtual void connection_disconnected(connection& aConnection) {}
		virtual void connection_giveup(connection& aConnection) {}
		// from buffer_observer
		virtual void buffer_message(buffer& aBuffer, const message& aMessage);
		virtual void buffer_message_updated(buffer& aBuffer, const message& aMessage) {}
		virtual void buffer_message_removed(buffer& aBuffer, const message& aMessage) {}
		virtual void buffer_message_failure(buffer& aBuffer, const message& aMessage) {}
		virtual void buffer_activate(buffer& aBuffer) {}
		virtual void buffer_reopen(buffer& aBuffer) {}
		virtual void buffer_open(buffer& aBuffer, const message& aMessage) {}
		virtual void buffer_closing(buffer& aBuffer) {}
		virtual bool is_weak_observer(buffer& aBuffer) { return true; }
		virtual void buffer_name_changed(buffer& aBuffer, const std::string& aOldName) {}
		virtual void buffer_title_changed(buffer& aBuffer, const std::string& aOldTitle) {}
		virtual void buffer_ready_changed(buffer& aBuffer) {}
		virtual void buffer_scrollbacked(buffer& aBuffer) {}
		virtual void buffer_cleared(buffer& aBuffer) {}
		virtual void buffer_hide(buffer& aBuffer) {}
		virtual void buffer_show(buffer& aBuffer) {}

	private:
		// attributes
		notify& iNotifyList;
		connection_manager& iConnectionManager;
		typedef std::list<connection*> watched_connections;
		watched_connections iWatchedConnections;
		typedef std::list<buffer*> watched_buffers;
		watched_buffers iWatchedBuffers;
		typedef std::vector<notify_action_ptr> action_list;
		action_list iActions;
	};
}

#endif //IRC_CLIENT_NOTIFY_WATCHER
