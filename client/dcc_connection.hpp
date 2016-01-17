// dcc_connection.h
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

#ifndef IRC_CLIENT_DCC_CONNECTION
#define IRC_CLIENT_DCC_CONNECTION

#include <neolib/observable.hpp>
#include <neolib/packet_stream.hpp>
#include <neolib/tcp_packet_stream_server.hpp>
#include <neolib/timer.hpp>
#include "model.hpp"
#include "user.hpp"

namespace irc
{
	class dcc_connection_manager;
	class dcc_message;

	class dcc_connection_observer
	{
		friend class dcc_connection;
	private:
		virtual void dcc_connection_activate(dcc_connection& aConnection) = 0;
		virtual void dcc_connection_ready_changed(dcc_connection& aConnection) = 0;
		virtual void dcc_connection_disconnected(dcc_connection& aConnection) = 0;
		virtual void dcc_connection_closing(dcc_connection& aConnection) = 0;
		virtual bool is_weak_observer(dcc_connection& aConnection) = 0;
	public:
		enum notify_type { NotifyActivate, NotifyReadyChange, NotifyConnectionDisconnected, NotifyClosing, NotifyIsWeakObserver };
	};

	typedef neolib::binary_packet dcc_packet;
	typedef neolib::tcp_packet_stream_server<dcc_packet> dcc_stream_server;
	typedef neolib::i_tcp_packet_stream_server_observer<dcc_packet> dcc_stream_server_observer;
	typedef neolib::packet_stream<dcc_packet, neolib::tcp_protocol> dcc_stream;
	typedef neolib::i_packet_stream_observer<dcc_packet, neolib::tcp_protocol> dcc_stream_observer;

	class dcc_connection : public neolib::observable<dcc_connection_observer>, protected dcc_stream_observer, public neolib::timer
	{
	public:
		// types
		enum type_e { UNKNOWN, CHAT, SEND };
		enum { packetSize = 10240 };
		struct no_stream_server : std::logic_error { no_stream_server() : std::logic_error("irc::dcc_connection::no_stream_server") {} };
		struct no_stream : std::logic_error { no_stream() : std::logic_error("irc::dcc_connection::no_stream") {} };
	private:
		typedef std::auto_ptr<dcc_stream_server> stream_server_pointer;
	public:
		// construction
		dcc_connection(type_e aType, const std::string& aName, const user& aLocalUser, const user& aRemoteUser, dcc_connection_manager& aConnectionManager, neolib::io_thread& aOwnerThread);
		~dcc_connection();

	public:
		// operations
		irc::model& model();
		const irc::model& model() const;
		type_e type() const { return iType; }
		model::id id() const { return iId; }
		const std::string& name() const { return iName; }
		const user& local_user() const { return iLocalUser; }
		const user& remote_user() const { return iRemoteUser; }
		irc::dcc_connection_manager& dcc_connection_manager() const { return iConnectionManager; }
		bool connect(u_long aAddress, u_short aPort);
		bool listen(int aPort);
		void set_stream(dcc_stream::pointer aStream);
		bool has_stream_server() const;
		const dcc_stream_server& stream_server() const;
		dcc_stream_server& stream_server();
		bool has_stream() const;
		const dcc_stream& stream() const;
		dcc_stream& stream();
		bool close();
		bool closing() const { return iClosing; }
		bool connected() const { return stream().connected(); }
		bool is_ready() const { return iReady; }
		void set_ready(bool aReady);
		bool has_error() const { return iError != 0; }
		int error() const { return iError.value(); }
		std::string error_string() const { return iError.message(); }
		void activate();
		void deactivate();
		void reopen();
		bool just_weak_observers();
		virtual void open(const dcc_message& aMessage) {}
		virtual bool find_message(model::id aMessageId, dcc_connection*& aConnection, dcc_message*& aMessage) { return false; }
	protected:
		// implementation
		void disconnect();
		void handle_disconnection();
		// from neolib::observable<dcc_connection_observer>
		virtual void notify_observer(dcc_connection_observer& aObserver, dcc_connection_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from dcc_stream_observer
		virtual void connection_established(dcc_stream& aStream);
		virtual void connection_failure(dcc_stream& aStream, const boost::system::error_code& aError);
		virtual void packet_sent(dcc_stream& aStream, const dcc_packet& aPacket);
		virtual void packet_arrived(dcc_stream& aStream, const dcc_packet& aPacket);
		virtual void transfer_failure(dcc_stream& aStream, const boost::system::error_code& aError);
		virtual void connection_closed(dcc_stream& aStream);
		// from neolib::timer
		virtual void ready();

	protected:
		// attributes
		type_e iType;
		irc::model::id iId;
		std::string iName;
		user iLocalUser;
		user iRemoteUser;
		irc::dcc_connection_manager& iConnectionManager;
		stream_server_pointer iStreamServer;
		dcc_stream::pointer iStream;
		boost::system::error_code iError;
		bool iClosing;
		bool iReady;
	};
}

#endif //IRC_CLIENT_DCC_CONNECTION
