// who.cpp
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
#include <neoirc/client/who.hpp>

namespace irc
{
	who_requester::who_requester(connection& aConnection) : 
		neolib::timer(aConnection.connection_manager().model().io_task(), 5000), 
		iConnection(aConnection)
	{
		iConnection.add_observer(*this);
	}

	who_requester::~who_requester()
	{
		iConnection.remove_observer(*this);
	}

	who_requester& who_requester::operator=(const who_requester& aOther)
	{
		request_list iRequests = aOther.iRequests;
		return *this;
	}

	void who_requester::new_request(const std::string& aMask, buffer& aBuffer)
	{
		new_request(aMask, &aBuffer);
	}

	void who_requester::new_request(const std::string& aMask, requester& aRequester)
	{
		new_request(aMask, &aRequester);
	}

	void who_requester::new_request(const std::string& aMask)
	{
		new_request(aMask, static_cast<requester*>(0));
	}

	void who_requester::new_request(channel_buffer& aBuffer, const std::string& aNickName)
	{
		new_request(aNickName, join_request(aBuffer));
	}

	void who_requester::cancel_request(const std::string& aMask, requester& aRequester)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
			if (irc::make_string(iConnection, i->iMask) == aMask &&
				i->iRequestType.is<requester*>() && static_cast<requester*>(i->iRequestType) == &aRequester)
				i->iRequestType = static_cast<requester*>(0);
	}

	void who_requester::cancel_request(requester& aRequester)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
			if (i->iRequestType.is<requester*>() && static_cast<requester*>(i->iRequestType) == &aRequester)
				i->iRequestType = static_cast<requester*>(0);
	}

	void who_requester::new_request(const std::string& aMask, const request_type& aRequestType)
	{
		request theRequest(aMask, aRequestType);
		
		for (request_list::const_iterator i = iRequests.begin(); i != iRequests.end(); ++i)
			if (i->iMask == theRequest.iMask && i->iRequestType == theRequest.iRequestType)
				return;

		iRequests.push_back(theRequest);

		if (!aRequestType.is<join_request>())
		{
			buffer& theBuffer = aRequestType.is<buffer*>() ? *static_cast<buffer*>(aRequestType) : iConnection.server_buffer();
			message requestMessage(theBuffer, message::OUTGOING);
			requestMessage.set_command(message::WHO);
			requestMessage.parameters().push_back(aMask);
			theBuffer.new_message(requestMessage);
			iRequests.back().iSent = true;
		}
	}

	bool who_requester::new_message(const message& aMessage)
	{
		neolib::optional<reply> theReply;

		switch(aMessage.command())
		{
		case message::RPL_WHOREPLY:
			if (aMessage.parameters().size() < 5)
				return false;
			for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
				if (i->iSent && 
					((irc::make_string(iConnection, i->iMask) == aMessage.parameters()[0]) ||
					(irc::make_string(iConnection, i->iMask) == aMessage.parameters()[4])))
				{
					theReply = reply(i, aMessage);
					break;
				}
			break;
		case message::RPL_ENDOFWHO:
			if (aMessage.parameters().empty())
				return false;
			else
			{
				for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
					if (i->iSent && irc::make_string(iConnection, i->iMask) == aMessage.parameters()[0])
					{
						theReply = reply(i, aMessage);
						break;
					}
			}
			break;
		default:
			break;
		}

		if (theReply)
		{
			if (theReply->first->iRequestType.is<buffer*>())
				static_cast<buffer*>(theReply->first->iRequestType)->new_message(theReply->second);
			else if (theReply->first->iRequestType.is<requester*>() &&
				static_cast<requester*>(theReply->first->iRequestType) != 0)
				static_cast<requester*>(theReply->first->iRequestType)->who_result(theReply->second);
			if (aMessage.command() == message::RPL_ENDOFWHO)
				iRequests.erase(theReply->first);
		}

		return theReply;
	}

	void who_requester::ready()
	{
		typedef std::vector<request_list::iterator> join_requests;
		typedef std::map<channel_buffer*, join_requests> join_request_map;
		join_request_map joinRequests;
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end(); ++i)
		{
			if (i->iRequestType.is<join_request>() && !i->iSent)
				joinRequests[static_cast<join_request&>(i->iRequestType).iChannel].push_back(i);
		}
		for (join_request_map::iterator i = joinRequests.begin(); i != joinRequests.end(); ++i)
		{
			if (i->second.size() < kPossibleNetSplit)
			{
				for (join_requests::iterator j = i->second.begin(); j != i->second.end(); ++j)
				{
					buffer& theBuffer = iConnection.server_buffer();
					message requestMessage(theBuffer, message::OUTGOING);
					requestMessage.set_command(message::WHO);
					requestMessage.parameters().push_back((*j)->iMask);
					theBuffer.new_message(requestMessage);
					(*j)->iSent = true;
				}
			}
			else
			{
				std::string channelName = i->first->name();
				for (join_requests::iterator j = i->second.begin(); j != i->second.end(); ++j)
					iRequests.erase(*j);
				new_request(channelName);
			}
		}
		again();
	}

	void who_requester::buffer_removed(buffer& aBuffer)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			if (i->iRequestType.is<buffer*>() && static_cast<buffer*>(i->iRequestType) == &aBuffer)
				iRequests.erase(i++);
			else if (aBuffer.type() == buffer::CHANNEL && i->iRequestType.is<join_request>() && static_cast<join_request&>(i->iRequestType).iChannel == static_cast<channel_buffer*>(&aBuffer))
				iRequests.erase(i++);
			else
				++i;
		}
	}

	void who_requester::connection_disconnected(connection& aConnection)
	{
		iRequests.clear();
	}
}