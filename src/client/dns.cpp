// dns.cpp
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

#include <neolib/neolib.hpp>
#include <neoirc/client/connection_manager.hpp>
#include <neoirc/client/dns.hpp>

namespace irc
{
	dns_requester::dns_requester(connection& aConnection) : whois_requester::requester(aConnection.whois()), iResolver(aConnection.connection_manager().resolver()), iConnection(aConnection)
	{
		iConnection.add_observer(*this);
	}

	dns_requester::~dns_requester()
	{
		iConnection.remove_observer(*this);
		iResolver.remove_requester(*this);
	}

	dns_requester& dns_requester::operator=(const dns_requester& aOther)
	{
		request_list iRequests = aOther.iRequests;
		return *this;
	}

	void dns_requester::new_request(const std::string& aNickName, buffer& aBuffer)
	{
		if (aBuffer.has_user(aNickName))
		{
			const user& theUser = aBuffer.user(aNickName);
			iRequests.push_back(request(theUser, &aBuffer));
			if (theUser.has_host_name())
				iResolver.resolve(*this, theUser.host_name());
			else
				iConnection.whois().new_request(aNickName, *this);
		}
		else
		{
			iRequests.push_back(request(user(aNickName), &aBuffer));
			iConnection.whois().new_request(aNickName, *this);
		}
	}

	void dns_requester::nick_change(const std::string& aOldNickName, const std::string& aNewNickName)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
			if (irc::make_string(iConnection, i->first.nick_name()) == aOldNickName)
				i->first.nick_name() = aNewNickName;
	}

	void dns_requester::whois_result(const message& aMessage)
	{
		if (aMessage.parameters().empty())
			return;

		switch(aMessage.command())
		{
		case message::RPL_WHOISUSER:
		case message::ERR_NOSUCHNICK:
			for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
			{
				if (irc::make_string(iConnection, i->first.nick_name()) == aMessage.parameters()[0])
				{
					if (aMessage.command() == message::RPL_WHOISUSER)
					{
						if (aMessage.parameters().size() >= 3)
						{
							i->first.user_name() = aMessage.parameters()[1];
							i->first.host_name() = aMessage.parameters()[2];
							iResolver.resolve(*this, i->first.host_name());
						}
						++i;
					}
					else
					{
						i->second->new_message(aMessage);
						iRequests.erase(i++);
					}
				}
				else
					++i;
			}
			break;
		default:
			break;
		}
	}

	void dns_requester::host_resolved(const std::string& aHostName, neolib::tcp_resolver::iterator aHost)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			if (i->first.host_name() == aHostName)
			{
				std::string result = i->first.nick_name() + ": hostname=" + aHost->host_name() + ", IP address=" + aHost->endpoint().address().to_string();
				message newMessage(*i->second, message::INCOMING);
				newMessage.set_command("");
				newMessage.parameters().clear();
				newMessage.parameters().push_back(result);
				i->second->new_message(newMessage);
				iRequests.erase(i++);
			}
			else
				++i;
		}
	}

	void dns_requester::buffer_removed(buffer& aBuffer)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			if (i->second == &aBuffer)
				iRequests.erase(i++);
			else
				++i;
		}
	}

	void dns_requester::incoming_message(connection& aConnection, const message& aMessage)
	{
		if (aMessage.command() != message::NICK)
			return;
		user oldUser(aMessage.origin(), aConnection);
		user newUser(oldUser);
		if (!aMessage.parameters().empty())
			newUser.nick_name() = aMessage.parameters()[0];
		nick_change(oldUser.nick_name(), newUser.nick_name());
	}
}