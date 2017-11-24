// logger.h
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

#ifndef IRC_CLIENT_LOGGER
#define IRC_CLIENT_LOGGER

#include <neolib/variant.hpp>
#include <neolib/timer.hpp>
#include <neoirc/client/connection_manager.hpp>
#include <neoirc/client/connection.hpp>
#include <neoirc/client/dcc_connection_manager.hpp>
#include <neoirc/client/dcc_chat_connection.hpp>
#include <neoirc/client/buffer.hpp>

namespace irc
{
	class logger : private connection_manager_observer, private connection_observer, private buffer_observer, dcc_connection_manager_observer, dcc_buffer_observer
	{
	public:
		// types
		enum events_e { None = 0x0, Message = 0x1, Notice = 0x2, JoinPartQuit = 0x4, Kick = 0x8, Mode = 0x10, Other = 0x20 };

	public:
		// exceptions
		struct utf16_logfile_unsupported : std::runtime_error { utf16_logfile_unsupported() : std::runtime_error("irc::logger::utf16_logfile_unsupported: UTF-16 log files unsupported, please convert log file(s) to UTF-8.") {} };

	private:
		// types
		class scrollbacker : public neolib::thread
		{
		public:
			typedef std::deque<message> buffer_messages;
			typedef std::deque<dcc_message> dcc_messages;
			typedef neolib::variant<buffer*, dcc_buffer*> buffer_t;
			typedef neolib::variant<buffer_messages, dcc_messages> messages_t;
		public:
			// construction
			scrollbacker(logger& aParent, irc::buffer& aBuffer) : iParent(aParent), iBuffer(&aBuffer), iNewMessages(buffer_messages()), iBufferSize(aParent.iModel.buffer_size()) {}
			scrollbacker(logger& aParent, dcc_buffer& aBuffer) : iParent(aParent), iBuffer(&aBuffer), iNewMessages(dcc_messages()), iBufferSize(aParent.iModel.buffer_size()) {}
			~scrollbacker() { abort(); }
		public:
			// operations
			bool is(buffer& aBuffer) const;
			bool is(dcc_buffer& aBuffer) const;
			buffer_t& buffer() { return iBuffer; }
			messages_t& messages() { return iMessages; }
			messages_t& new_messages() { return iNewMessages; }
		private:
			// implementation
			virtual void task();
			virtual bool soft_abort() const { return true; }
		private:
			// attributes
			logger& iParent;
			buffer_t iBuffer;
			messages_t iMessages;
			messages_t iNewMessages;
			std::size_t iBufferSize;
		};
		typedef std::shared_ptr<scrollbacker> scrollbacker_pointer;
		friend class scrollbacker;
		typedef std::vector<scrollbacker_pointer> scrollbackers;

	public:
		// construction
		logger(model& aModel, connection_manager& aConnectionManager, dcc_connection_manager& aDccConnectionManager);
		~logger();

	public:
		// operations
		bool enable(bool aEnable);
		bool enabled() const { return iEnabled; }
		void timestamp_all();
		void timestamp_servers();
		bool set_directory(const std::string& aDirectory);
		std::string directory(buffer::type_e aBufferType) const;
		std::string directory(dcc_connection::type_e aDccConnectionType) const;
		void process_pending();
		events_e events() const { return iEvents; }
		events_e& events() { return iEvents; }
		bool server_log() const { return iServerLog; }
		bool& server_log() { return iServerLog; }
		bool scrollback_logs() const { return iScrollbackLogs; }
		bool& scrollback_logs() { return iScrollbackLogs; }
		std::size_t scrollback_size() const { return iScrollbackSize; }
		std::size_t& scrollback_size() { return iScrollbackSize; }
		bool archive() const { return iArchive; }
		bool& archive() { return iArchive; }
		std::size_t archive_size() const { return iArchiveSize; }
		std::size_t& archive_size() { return iArchiveSize; }
		void create_directories() const;
		enum filename_type_e { Normal, Scrollback };
		std::string filename(const buffer& aBuffer, filename_type_e aType = Normal);
		std::string filename(const dcc_buffer& aBuffer, filename_type_e aType = Normal);

	private:
		// implementation
		static void get_timestamp(std::string& aTimeStamp, bool aContinuation = false);
		void new_entry(buffer& aBuffer, const std::string& aText);
		void new_entry(dcc_buffer& aBuffer, const std::string& aText);
		void new_entry(const std::string& aFileName, const std::string& aText, filename_type_e aType = Normal);
		scrollbackers::iterator scrollbacker_for_buffer(buffer& aBuffer);
		scrollbackers::iterator scrollbacker_for_buffer(dcc_buffer& aBuffer);
		// from connection_manager_observer
		void connection_added(connection& aConnection) override;
		void connection_removed(connection& aConnection) override;
		void filter_message(connection& aConnection, const message& aMessage, bool& aFiltered) override {}
		bool query_disconnect(const connection& aConnection) override { return false; }
		void query_nickname(connection& aConnection) override {}
		void disconnect_timeout_changed() override {}
		void retry_network_delay_changed() override {}
		void buffer_activated(buffer& aActiveBuffer) override {}
		void buffer_deactivated(buffer& aDeactivatedBuffer) override {}
		// from connection_observer
		void connection_connecting(connection& aConnection) override {}
		void connection_registered(connection& aConnection) override {}
		void buffer_added(buffer& aBuffer) override;
		void buffer_removed(buffer& aBuffer) override;
		void incoming_message(connection& aConnection, const message& aMessage) override {}
		void outgoing_message(connection& aConnection, const message& aMessage) override {}
		void connection_quitting(connection& aConnection) override {}
		void connection_disconnected(connection& aConnection) override {}
		void connection_giveup(connection& aConnection) override {}
		// from buffer_observer
		void buffer_message(buffer& aBuffer, const message& aMessage) override;
		void buffer_message_updated(buffer& aBuffer, const message& aMessage) override {}
		void buffer_message_removed(buffer& aBuffer, const message& aMessage) override {}
		void buffer_message_failure(buffer& aBuffer, const message& aMessage) override {}
		void buffer_activate(buffer& aBuffer) override {}
		void buffer_reopen(buffer& aBuffer) override {}
		void buffer_open(buffer& aBuffer, const message& aMessage) override {}
		void buffer_closing(buffer& aBuffer) override {}
		bool is_weak_observer(buffer& aBuffer) override { return true; }
		void buffer_name_changed(buffer& aBuffer, const std::string& aOldName) override {}
		void buffer_title_changed(buffer& aBuffer, const std::string& aOldTitle) override {}
		void buffer_ready_changed(buffer& aBuffer) override  {}
		void buffer_scrollbacked(buffer& aBuffer) override {}
		void buffer_cleared(buffer& aBuffer) override {}
		void buffer_hide(buffer& aBuffer) override {}
		void buffer_show(buffer& aBuffer) override {}
		// from	dcc_connection_manager_observer
		void dcc_connection_added(dcc_connection& aConnection) override;
		void dcc_connection_removed(dcc_connection& aConnection) override;
		void dcc_download_failure(dcc_connection& aConnection, const std::string& aErrorString) override {}
		// from dcc_buffer_observer
		void dcc_chat_open(dcc_buffer& aBuffer, const dcc_message& aMessage) override {}
		void dcc_chat_message(dcc_buffer& aBuffer, const dcc_message& aMessage) override;
		void dcc_chat_message_failure(dcc_chat_connection& aConnection, const dcc_message& aMessage) override {}
		void dcc_chat_scrollbacked(dcc_buffer& aConnection) override {}

	private:
		// attributes
		model& iModel;
		connection_manager& iConnectionManager;
		dcc_connection_manager& iDccConnectionManager;
		bool iEnabled;
		typedef std::list<connection*> logged_connections;
		logged_connections iLoggedConnections;
		typedef std::list<buffer*> logged_buffers;
		logged_buffers iLoggedBuffers;
		typedef std::list<dcc_buffer*> logged_dcc_buffers;
		logged_dcc_buffers iLoggedDccChatConnections;
		typedef std::string line;
		typedef std::pair<std::string, line> pending_entry;
		typedef std::deque<pending_entry> pending_entries;
		pending_entries iPendingEntries;
		std::string iDirectory;
		events_e iEvents;
		bool iServerLog;
		bool iScrollbackLogs;
		std::size_t iScrollbackSize; // KB
		bool iArchive;
		std::size_t iArchiveSize; // KB
		scrollbackers iScrollbackers;
		neolib::callback_timer iUpdateTimer;
	};
}

#endif //IRC_CLIENT_LOGGER
