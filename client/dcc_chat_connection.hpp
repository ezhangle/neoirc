// dcc_chat_connection.h
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

#ifndef IRC_CLIENT_DCC_CHAT_CONNECTION
#define IRC_CLIENT_DCC_CHAT_CONNECTION

#include "model.hpp"
#include "dcc_connection.hpp"
#include "dcc_message.hpp"
#include "connection.hpp"
#include "connection_manager.hpp"

namespace irc
{
	class dcc_chat_connection_observer
	{
		friend class dcc_chat_connection;
	private:
		virtual void dcc_chat_open(dcc_chat_connection& aConnection, const dcc_message& aMessage) = 0;
		virtual void dcc_chat_message(dcc_chat_connection& aConnection, const dcc_message& aMessage) = 0;
		virtual void dcc_chat_message_failure(dcc_chat_connection& aConnection, const dcc_message& aMessage) = 0;
		virtual void dcc_chat_reloaded(dcc_chat_connection& aConnection) = 0;
	public:
		enum notify_type { NotifyOpen, NotifyMessage, NotifyMessageFailure, NotifyReloaded };
	};

	typedef dcc_chat_connection_observer dcc_buffer_observer;

	class dcc_chat_connection : public dcc_connection, public neolib::observable<dcc_chat_connection_observer>, private model_observer, private connection_manager_observer
	{
	public:
		// types
		enum origin_e { Listen, Local, Remote };
		typedef std::deque<dcc_message> message_list;
	public:
		// construction
		dcc_chat_connection(connection* aConnection, origin_e aOrigin, const std::string& aName, const user& aLocalUser, const user& aRemoteUser, irc::dcc_connection_manager& aConnectionManager, neolib::io_thread& aOwnerThread);
		~dcc_chat_connection();

	public:
		// operations
		irc::connection* connection() { return iConnection; }
		origin_e origin() const { return iOrigin; }
		void new_message(const std::string& aMessage, bool aNotCommand = false);
		void new_message(const dcc_message& aMessage, bool aNotCommand = false);
		const message_list& messages() const { return iMessages; }
		message_list& messages() { return iMessages; }
		virtual void open(const dcc_message& aMessage);
		virtual bool find_message(model::id aMessageId, dcc_connection*& aConnection, dcc_message*& aMessage);
		void reload(const message_list& aMessages);

	private:
		// implementation
		void add_message(const dcc_message& aMessage);
		void handle_message(const dcc_message& aMessage);
		void send_message(const dcc_message& aMessage);

		// from neolib::observable<dcc_chat_connection_observer>
		virtual void notify_observer(dcc_chat_connection_observer& aObserver, dcc_chat_connection_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from dcc_connection
		virtual void packet_sent(dcc_stream& aStream, const dcc_packet& aPacket);
		virtual void packet_arrived(dcc_stream& aStream, const dcc_packet& aPacket);
		// from model_observer
		virtual void buffer_size_changed(std::size_t aNewBufferSize);
		// from connection_manager_observer
		virtual void connection_added(irc::connection& aConnection) {}
		virtual void connection_removed(irc::connection& aConnection) { if (iConnection == &aConnection) iConnection = 0; }
		virtual void filter_message(irc::connection& aConnection, const message& aMessage, bool& aFiltered) {}
		virtual bool query_disconnect(const irc::connection& aConnection) { return false; }
		virtual void query_nickname(irc::connection& aConnection) {}
		virtual void disconnect_timeout_changed() {}
		virtual void retry_network_delay_changed() {}
		virtual void buffer_activated(buffer& aActiveBuffer) {}
		virtual void buffer_deactivated(buffer& aDeactivatedBuffer) {}

	private:
		// attributes
		irc::connection* iConnection;
		origin_e iOrigin;
		message_list iMessages;
		std::size_t iBufferSize;
		dcc_packet::contents_type iPartialLine;
	};

	typedef dcc_chat_connection dcc_buffer;
}

#endif //IRC_CLIENT_DCC_CHAT_CONNECTION
