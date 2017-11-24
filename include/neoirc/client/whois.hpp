// whois.h
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

#ifndef IRC_CLIENT_WHOIS
#define IRC_CLIENT_WHOIS

#include <neolib/variant.hpp>
#include <neoirc/client/connection.hpp>
#include <neoirc/client/buffer.hpp>
#include <neoirc/client/message.hpp>

namespace irc
{
	class whois_requester : private connection_observer
	{
	public:
		// types
		struct requester
		{
			requester(whois_requester& aParent) : iParent(aParent) {}
			~requester() { iParent.cancel_request(*this); }
			virtual void whois_result(const message& aMessage) = 0;
			whois_requester& iParent;
		};

	public:
		// construction
		whois_requester(connection& aConnection);
		virtual ~whois_requester();
		whois_requester& operator=(const whois_requester& aOther);

	public:
		// operations
		void new_request(const std::string& aNickName, buffer& aBuffer);
		void new_request(const std::string& aNickName, requester& aRequester);
		void new_request(const std::string& aNickName);
		void cancel_request(const std::string& aNickName, requester& aRequester);
		void cancel_request(requester& aRequester);
		void nick_change(const std::string& aOldNickName, const std::string& aNewNickName);
		bool new_message(const message& aMessage);

	private:
		// implementation
		typedef neolib::variant<buffer*, requester*> requester_type;
		void new_request(const std::string& aNickName, const requester_type& aRequester);
		// from connection_observer
		virtual void connection_connecting(connection& aConnection) {}
		virtual void connection_registered(connection& aConnection) {}
		virtual void buffer_added(buffer& aBuffer) {}
		virtual void buffer_removed(buffer& aBuffer);
		virtual void incoming_message(connection& aConnection, const message& aMessage);
		virtual void outgoing_message(connection& aConnection, const message& aMessage) {}
		virtual void connection_quitting(connection& aConnection) {}
		virtual void connection_disconnected(connection& aConnection);
		virtual void connection_giveup(connection& aConnection) {}

	private:
		// attributes
		connection& iConnection;
		typedef std::pair<std::string, requester_type> request;
		typedef std::list<request> request_list;
		request_list iRequests;
		typedef std::list<message> replies;
		replies iReplies;
	};
}

#endif //IRC_CLIENT_WHOIS
