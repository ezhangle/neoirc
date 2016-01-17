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
#include "connection_manager.hpp"
#include "connection.hpp"
#include "dcc_connection_manager.hpp"
#include "dcc_chat_connection.hpp"
#include "buffer.hpp"

namespace irc
{
	class logger : private connection_manager_observer, private connection_observer, private buffer_observer, dcc_connection_manager_observer, dcc_buffer_observer
	{
	public:
		// types
		enum events_e { None = 0x0, Message = 0x1, Notice = 0x2, JoinPartQuit = 0x4, Kick = 0x8, Mode = 0x10, Other = 0x20 };

	private:
		// types
		class converter : public neolib::thread
		{
		public:
			enum type_e { ToAnsi, ToUnicode };
		public:
			// construction
			converter(logger& aParent, type_e aType, const std::string& aFileName) : iParent(aParent), iType(aType), iFileName(aFileName) {}
			~converter() { abort(); }
		public:
			// operations
			type_e type() const { return iType; }
			const std::string& file_name() const { return iFileName; }
		private:
			// implementation
			virtual void task();
			virtual bool soft_abort() const { return true; }
			virtual void cleanup();
		private:
			// attributes
			logger& iParent;
			type_e iType;
			std::string iFileName;
		};
		class reloader : public neolib::thread
		{
		public:
			typedef std::deque<message> buffer_messages;
			typedef std::deque<dcc_message> dcc_messages;
			typedef neolib::variant<buffer*, dcc_buffer*> buffer_t;
			typedef neolib::variant<buffer_messages, dcc_messages> messages_t;
		public:
			// construction
			reloader(logger& aParent, irc::buffer& aBuffer) : iParent(aParent), iBuffer(&aBuffer), iNewMessages(buffer_messages()), iBufferSize(aParent.iModel.buffer_size()) {}
			reloader(logger& aParent, dcc_buffer& aBuffer) : iParent(aParent), iBuffer(&aBuffer), iNewMessages(dcc_messages()), iBufferSize(aParent.iModel.buffer_size()) {}
			~reloader() { abort(); }
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
		typedef std::shared_ptr<reloader> reloader_pointer;
		friend class reloader;
		typedef std::vector<reloader_pointer> reloaders;

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
		bool converting() const { return iConverter.get() != 0; }
		bool pending() const { return !iPendingEntries.empty(); }
		void process_pending();
		void cancel_conversion();
		events_e events() const { return iEvents; }
		events_e& events() { return iEvents; }
		bool server_log() const { return iServerLog; }
		bool& server_log() { return iServerLog; }
		bool reloading() const { return iReloading; }
		bool& reloading() { return iReloading; }
		std::size_t reload_size() const { return iReloadSize; }
		std::size_t& reload_size() { return iReloadSize; }
		bool archive() const { return iArchive; }
		bool& archive() { return iArchive; }
		std::size_t archive_size() const { return iArchiveSize; }
		std::size_t& archive_size() { return iArchiveSize; }
		void create_directories() const;
		enum filename_type_e { Normal, Reload };
		std::string filename(const buffer& aBuffer, filename_type_e aType = Normal);
		std::string filename(const dcc_buffer& aBuffer, filename_type_e aType = Normal);

	private:
		// implementation
		static void get_timestamp(std::string& aTimeStamp, bool aContinuation = false);
		static void get_timestamp(std::wstring& aTimeStamp, bool aContinuation = false);
		void new_entry(buffer& aBuffer, const std::string& aText);
		void new_entry(buffer& aBuffer, const std::wstring& aText);
		void new_entry(dcc_buffer& aBuffer, const std::string& aText);
		void new_entry(dcc_buffer& aBuffer, const std::wstring& aText);
		void new_entry(const std::string& aFileName, const std::string& aText, filename_type_e aType = Normal);
		void new_entry(const std::string& aFileName, const std::wstring& aText, filename_type_e aType = Normal);
		reloaders::iterator reloader_for_buffer(buffer& aBuffer);
		reloaders::iterator reloader_for_buffer(dcc_buffer& aBuffer);
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
		virtual void buffer_ready_changed(buffer& aBuffer)  {}
		virtual void buffer_reloaded(buffer& aBuffer) {}
		virtual void buffer_cleared(buffer& aBuffer) {}
		virtual void buffer_hide(buffer& aBuffer) {}
		virtual void buffer_show(buffer& aBuffer) {}
		// from	dcc_connection_manager_observer
		virtual void dcc_connection_added(dcc_connection& aConnection);
		virtual void dcc_connection_removed(dcc_connection& aConnection);
		virtual void dcc_download_failure(dcc_connection& aConnection, const std::string& aErrorString) {}
		// from dcc_buffer_observer
		virtual void dcc_chat_open(dcc_buffer& aBuffer, const dcc_message& aMessage) {}
		virtual void dcc_chat_message(dcc_buffer& aBuffer, const dcc_message& aMessage);
		virtual void dcc_chat_message_failure(dcc_chat_connection& aConnection, const dcc_message& aMessage) {}
		virtual void dcc_chat_reloaded(dcc_buffer& aConnection) {}

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
		std::shared_ptr<converter> iConverter;
		bool iCancelConversion;
		typedef neolib::variant<std::string, std::wstring> line;
		typedef std::pair<std::string, line> pending_entry;
		typedef std::deque<pending_entry> pending_entries;
		pending_entries iPendingEntries;
		std::string iDirectory;
		events_e iEvents;
		bool iServerLog;
		bool iReloading;
		std::size_t iReloadSize; // KB
		bool iArchive;
		std::size_t iArchiveSize; // KB
		reloaders iReloaders;
		neolib::timer_callback iUpdateTimer;
	public:
		static std::string sConvertStartMessage;
		static std::string sConvertEndMessage;
	};
}

#endif //IRC_CLIENT_LOGGER
