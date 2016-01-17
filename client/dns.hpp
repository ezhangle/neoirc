// dns.h
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

#ifndef IRC_CLIENT_DNS
#define IRC_CLIENT_DNS

#include <neolib/resolver.hpp>
#include "connection.hpp"
#include "whois.hpp"
#include "buffer.hpp"
#include "message.hpp"

namespace irc
{
	class dns_requester : private connection_observer, private whois_requester::requester, private neolib::tcp_resolver::requester
	{
		// types
	public:

		// construction
	public:
		dns_requester(connection& aConnection);
		virtual ~dns_requester();
		dns_requester& operator=(const dns_requester& aOther);

		// operations
	public:
		void new_request(const std::string& aNickName, buffer& aBuffer);
		void nick_change(const std::string& aOldNickName, const std::string& aNewNickName);

		// implementation
	private:
		// from connection_observer
		virtual void connection_connecting(connection& aConnection) {}
		virtual void connection_registered(connection& aConnection) {}
		virtual void buffer_added(buffer& aBuffer) {}
		virtual void buffer_removed(buffer& aBuffer);
		virtual void incoming_message(connection& aConnection, const message& aMessage);
		virtual void outgoing_message(connection& aConnection, const message& aMessage) {}
		virtual void connection_quitting(connection& aConnection) {}
		virtual void connection_disconnected(connection& aConnection) {}
		virtual void connection_giveup(connection& aConnection) {}
		// from whois_requester::requester
		virtual void whois_result(const message& aMessage);
		// from neolib::tcp_resolver::requester
		virtual void host_resolved(const std::string& aHostName, neolib::tcp_resolver::iterator aHost);
		virtual void host_not_resolved(const std::string& aHostName, const boost::system::error_code& aError) {}

		// attributes
	private:
		neolib::tcp_resolver& iResolver;
		connection& iConnection;
		typedef std::pair<user, buffer*> request;
		typedef std::list<request> request_list;
		request_list iRequests;
	};
}

#endif //IRC_CLIENT_DNS
