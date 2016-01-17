// whois.cpp
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
#include "whois.hpp"

namespace irc
{
	whois_requester::whois_requester(connection& aConnection) : iConnection(aConnection)
	{
		iConnection.add_observer(*this);
	}

	whois_requester::~whois_requester()
	{
		iConnection.remove_observer(*this);
	}

	whois_requester& whois_requester::operator=(const whois_requester& aOther)
	{
		request_list iRequests = aOther.iRequests;
		replies iReplies = aOther.iReplies;
		return *this;
	}

	void whois_requester::new_request(const std::string& aNickName, buffer& aBuffer)
	{
		new_request(aNickName, &aBuffer);
	}

	void whois_requester::new_request(const std::string& aNickName, requester& aRequester)
	{
		new_request(aNickName, &aRequester);
	}

	void whois_requester::new_request(const std::string& aNickName)
	{
		new_request(aNickName, static_cast<requester*>(0));
	}

	void whois_requester::cancel_request(const std::string& aNickName, requester& aRequester)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
			if (irc::make_string(iConnection, i->first) == aNickName &&
				i->second.is<requester*>() && static_cast<requester*>(i->second) == &aRequester)
				i->second = static_cast<requester*>(0);
	}

	void whois_requester::cancel_request(requester& aRequester)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
			if (i->second.is<requester*>() && static_cast<requester*>(i->second) == &aRequester)
				i->second = static_cast<requester*>(0);
	}

	void whois_requester::new_request(const std::string& aNickName, const requester_type& aRequester)
	{
		iRequests.push_back(request(aNickName, aRequester));

		buffer& theBuffer = aRequester.is<buffer*>() ? *static_cast<buffer*>(aRequester) : iConnection.server_buffer();
		message requestMessage(theBuffer, message::OUTGOING);
		requestMessage.set_command(message::WHOIS);
		requestMessage.parameters().push_back(aNickName);
		theBuffer.new_message(requestMessage);
	}

	void whois_requester::nick_change(const std::string& aOldNickName, const std::string& aNewNickName)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
			if (irc::make_string(iConnection, i->first) == aOldNickName)
				i->first = aNewNickName;
	}

	bool whois_requester::new_message(const message& aMessage)
	{
		if (aMessage.parameters().empty())
			return false;

		bool found = false;

		switch(aMessage.command())
		{
		case message::RPL_WHOISUSER:
		case message::RPL_WHOISCHANNELS:
		case message::RPL_WHOISSERVER:
		case message::RPL_WHOISOPERATOR:
		case message::RPL_WHOISIDLE:
		case message::RPL_WHOISEXTRA:
		case message::RPL_WHOISREGNICK:
		case message::RPL_WHOISADMIN:
		case message::RPL_WHOISSADMIN:
		case message::RPL_WHOISSVCMSG:
		case message::RPL_AWAY:
			for (request_list::iterator i = iRequests.begin(); i != iRequests.end() && !found; ++i)
				if (irc::make_string(iConnection, i->first) == aMessage.parameters()[0])
					found = true;
			if (found)
				iReplies.push_back(aMessage);
			break;
		case message::RPL_ENDOFWHOIS:
			for (request_list::iterator i = iRequests.begin(); !found && i != iRequests.end();)
			{
				if (irc::make_string(iConnection, i->first) == aMessage.parameters()[0])
				{
					found = true;
					iReplies.push_back(aMessage);
					for (replies::iterator j = iReplies.begin(); j != iReplies.end();)
					{
						if (irc::make_string(iConnection, j->parameters()[0]) == aMessage.parameters()[0])
						{
							if (i->second.is<buffer*>())
								static_cast<buffer*>(i->second)->new_message(*j);
							else if (static_cast<requester*>(i->second) != 0)
								static_cast<requester*>(i->second)->whois_result(*j);
							iReplies.erase(j++);
						}
						else
							++j;
					}
					iRequests.erase(i++);
				}
				else
					++i;
			}
			break;
		case message::ERR_NOSUCHNICK:
			for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
			{
				if (irc::make_string(iConnection, i->first) == aMessage.parameters()[0])
				{
					if (i->second.is<buffer*>())
						static_cast<buffer*>(i->second)->new_message(aMessage);
					else if (static_cast<requester*>(i->second) != 0)
						static_cast<requester*>(i->second)->whois_result(aMessage);
					iRequests.erase(i++);
					found = true;
				}
				else
					++i;
			}
			break;
		default:
			break;
		}

		return found;
	}

	void whois_requester::buffer_removed(buffer& aBuffer)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			if (i->second.is<buffer*>() && static_cast<buffer*>(i->second) == &aBuffer)
				iRequests.erase(i++);
			else
				++i;
		}
	}

	void whois_requester::incoming_message(connection& aConnection, const message& aMessage)
	{
		if (aMessage.command() != message::NICK)
			return;
		user oldUser(aMessage.origin(), aConnection);
		user newUser(oldUser);
		if (!aMessage.parameters().empty())
			newUser.nick_name() = aMessage.parameters()[0];
		nick_change(oldUser.nick_name(), newUser.nick_name());
	}

	void whois_requester::connection_disconnected(connection& aConnection)
	{
		iRequests.clear();
	}
}