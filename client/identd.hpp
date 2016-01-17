// identd.h
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

#ifndef IRC_CLIENT_IDENTD
#define IRC_CLIENT_IDENTD

#include <neolib/tcp_packet_stream_server.hpp>
#include <memory>
#include "connection_manager.hpp"
#include "connection.hpp"

namespace irc
{
	class identd : private neolib::tcp_string_packet_stream_server_observer, private neolib::i_tcp_string_packet_stream_observer
	{
		// types
	public:
		enum type_e { Nickname, Email, Userid };
	private:
		typedef std::auto_ptr<neolib::tcp_string_packet_stream_server> server_pointer;

		// construction
	public:
		identd(model& aModel);
		~identd();

		// operations
	public:
		void enable(bool aEnable);
		bool enabled() const { return iEnabled; }
		void set_type(type_e aType) { iType = aType; }
		type_e type() const { return iType; }
		void set_userid(const std::string& aUserid) { iUserid = aUserid; }
		const std::string& userid() const { return iUserid; }

		// implementation
	private:
		// from neolib::tcp_string_packet_stream_server_observer
		virtual void packet_stream_added(neolib::tcp_string_packet_stream_server& aServer, neolib::tcp_string_packet_stream& aStream);
		virtual void packet_stream_removed(neolib::tcp_string_packet_stream_server& aServer, neolib::tcp_string_packet_stream& aStream);
		virtual void failed_to_accept_packet_stream(neolib::tcp_string_packet_stream_server& aServer, const boost::system::error_code& aError) {}
		// from neolib::i_tcp_string_packet_stream_observer
		virtual void connection_established(neolib::tcp_string_packet_stream& aStream) {}
		virtual void connection_failure(neolib::tcp_string_packet_stream& aStream, const boost::system::error_code& aError) {}
		virtual void packet_sent(neolib::tcp_string_packet_stream& aStream, const neolib::string_packet& aPacket) {}
		virtual void packet_arrived(neolib::tcp_string_packet_stream& aStream, const neolib::string_packet& aPacket);
		virtual void transfer_failure(neolib::tcp_string_packet_stream& aStream, const boost::system::error_code& aError) {}
		virtual void connection_closed(neolib::tcp_string_packet_stream& aStream) {}

		// attributes
	private:
		model& iModel;
		bool iEnabled;
		type_e iType;
		std::string iUserid;
		server_pointer iServer;
		class client : public neolib::timer 
		{
		public:
			client(neolib::io_thread& aOwnerThread, neolib::tcp_string_packet_stream& aStream) : neolib::timer(aOwnerThread, 60*1000), iStream(aStream) 
			{ 
			}
		private:
			virtual void ready() 
			{ 
				iStream.close(); 
			}
		private:
			neolib::tcp_string_packet_stream& iStream;
		};
		typedef std::map<neolib::tcp_string_packet_stream*, client> clients;
		clients iClients;
	};
}

#endif //IRC_CLIENT_IDENTD
