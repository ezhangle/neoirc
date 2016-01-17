// identd.cpp
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
#include "identd.hpp"

namespace irc
{
	identd::identd(model& aModel) : 
		iModel(aModel), iEnabled(false), iType(Nickname)
	{
	}

	identd::~identd()
	{
		for (clients::iterator i = iClients.begin(); i != iClients.end(); ++i)
			i->first->close();
	}

	void identd::enable(bool aEnable)
	{
		if (iEnabled == aEnable)
			return;
		iEnabled = aEnable;
		if (iEnabled)
		{
			server_pointer newServer(new neolib::tcp_string_packet_stream_server(iModel.owner_thread(), 113));
			iServer = newServer;
			iServer->add_observer(*this);
		}
		else
		{
			iServer.reset();
		}
	}

	void identd::packet_stream_added(neolib::tcp_string_packet_stream_server& aServer, neolib::tcp_string_packet_stream& aStream)
	{
		iClients.insert(std::make_pair(&aStream, client(iModel.owner_thread(), aStream)));
		aStream.add_observer(*this);
	}

	void identd::packet_stream_removed(neolib::tcp_string_packet_stream_server& aServer, neolib::tcp_string_packet_stream& aStream)
	{
		clients::iterator client = iClients.find(&aStream);
		if (client != iClients.end())
			iClients.erase(client);
	}

	void identd::packet_arrived(neolib::tcp_string_packet_stream& aStream, const neolib::string_packet& aPacket)
	{
		clients::iterator theClient = iClients.find(&aStream);
		if (theClient == iClients.end())
			return;
		
		theClient->second.reset();

		std::string response;
		neolib::vecarray<std::string, 2> parts;
		neolib::tokens(aPacket.contents(), std::string(","), parts, 2);
		if (parts.size() == 2)
		{
			neolib::vecarray<std::string, 1> localPart;
			neolib::vecarray<std::string, 1> remotePart;
			neolib::tokens(parts[0], std::string(" \r\n"), localPart, 1);
			neolib::tokens(parts[1], std::string(" \r\n"), remotePart, 1);
			unsigned long localPort = 0;
			unsigned long remotePort = 0;
			if (!localPart[0].empty())
				localPort = neolib::string_to_unsigned_integer(localPart[0]);
			if (!remotePart[0].empty())
				remotePort = neolib::string_to_unsigned_integer(remotePart[0]);
			if (localPort >= 1 && localPort <= 65535 && remotePort >= 1 && remotePort <= 65535)
			{
				connection* theConnection = iModel.connection_manager().find_connection(static_cast<u_short>(localPort), static_cast<u_short>(remotePort));
				if (theConnection != 0)
				{
					response = neolib::unsigned_integer_to_string<char>(localPort) + "," + neolib::unsigned_integer_to_string<char>(remotePort);
					response += ":USERID:WIN32:";
					switch(type())
					{
					case Nickname:
						response += theConnection->nick_name();
						break;
					case Email:
						{
							std::string userName = theConnection->identity().email_address();
							std::string::size_type at = userName.find('@');
							if (at != std::string::npos)
								userName = userName.substr(0, at);
							if (userName.empty())
								userName = theConnection->nick_name();
							response += userName;
						}
						break;
					case Userid:
						response += userid();
						break;
					}
					response += "\r\n";
				}
				else
				{
					response = neolib::unsigned_integer_to_string<char>(localPort) + "," + neolib::unsigned_integer_to_string<char>(remotePort);
					response += ":ERROR:NO-USER\r\n";
				}
			}
			else
			{
				response = aPacket.contents();
				response += ":ERROR:INVALID-PORT\r\n";
			}
		}
		else
		{
			response = aPacket.contents();
			response += ":ERROR:UNKNOWN-ERROR\r\n";
		}
		aStream.send_packet(neolib::string_packet(response));
	}
}