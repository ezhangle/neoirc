// dcc_connection_manager.cpp
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
#include <fstream>
#include <neolib/file.hpp>
#include <neoirc/client/dcc_connection_manager.hpp>
#include <neoirc/client/connection_manager.hpp>
#include <neoirc/client/connection.hpp>
#include <neoirc/client/message.hpp>

namespace irc
{
	std::string dcc_connection_manager::sConnectingToString;
	std::string dcc_connection_manager::sConnectedToString;
	std::string dcc_connection_manager::sDisconnectedString;

	dcc_connection_manager::dcc_connection_manager(irc::model& aModel) : 
		neolib::manager_of<dcc_connection_manager, dcc_connection_manager_observer, dcc_connection>(*this, dcc_connection_manager_observer::NotifyConnectionAdded, dcc_connection_manager_observer::NotifyConnectionRemoved),
		iModel(aModel), iAutomaticIpAddress(true), iDccBasePort(4200), iDccFastSend(false), iNextConnectionId(0), iActiveConnection(0)
	{
	}

	dcc_connection_manager::~dcc_connection_manager()
	{
		erase_objects(iConnections, iConnections.begin(), iConnections.end());
	}

	bool dcc_connection_manager::send_file(connection& aConnection, const user& aUser, const std::string& aFilePath, const std::string& aFileName)
	{
		std::fstream file(aFilePath.c_str(), std::ios::in|std::ios::binary);
		if (!file)
			return false;
		unsigned long filesize = static_cast<unsigned long>(neolib::large_file_size(aFilePath));

		iConnections.push_back(dcc_connection_ptr(new dcc_send_connection(aConnection, dcc_send_connection::Listen, aFileName, user(aConnection.nick_name()), aUser, *this, iModel.io_task())));
		if (!iConnections.back()->listen(next_port()))
		{
			iConnections.pop_back();
			return false;
		}
		iConnections.back()->stream_server().add_observer(*this);
		iSendFileRequests.push_back(dcc_send_file_request(iConnections.back()->stream_server().local_port(), aFilePath));
		
		u_long address = boost::asio::ip::address::from_string(iIpAddress).to_v4().to_ulong();

		if (iAutomaticIpAddress)
		{
			if (!aConnection.have_local_address())
				aConnection.local_address_warning();
			address = aConnection.local_address();
		}

		message newMessage(aConnection, message::OUTGOING);
		newMessage.set_command(message::PRIVMSG);
		newMessage.parameters().push_back(aUser.nick_name());
		newMessage.parameters().push_back("\001DCC SEND "); 
		newMessage.parameters().back() += aFileName + " ";
		newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(address) + " ";
		newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(iConnections.back()->stream_server().local_port()) + " ";
		newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(filesize) + "\001";
		aConnection.send_message(newMessage);

		return true;
	}

	bool dcc_connection_manager::receive_file(connection& aConnection, const user& aUser, u_long aAddress, u_short aPort, const std::string& aFilePath, const std::string& aFileName, unsigned long aFileSize, neolib::optional<unsigned long> aResumeFileSize)
	{
		{
			std::ofstream file(neolib::create_file(aFilePath).c_str(), std::ios::out|std::ios::binary|(aResumeFileSize ? std::ios::app : 0));
			if (!file)
				return false;
		}
		iConnections.push_back(dcc_connection_ptr(new dcc_send_connection(aConnection, dcc_send_connection::Download, aFileName, user(aConnection.nick_name()), aUser, *this, iModel.io_task())));
		if (!aResumeFileSize)
			static_cast<dcc_send_connection&>(*iConnections.back()).receive_file(aAddress, aPort, aFilePath, aFileSize);
		else
			static_cast<dcc_send_connection&>(*iConnections.back()).resume_file(aAddress, aPort, aFilePath, aFileName, aFileSize, *aResumeFileSize);
		return true;
	}

	bool dcc_connection_manager::request_chat(connection& aConnection, const user& aUser)
	{
		iConnections.push_back(dcc_connection_ptr(new dcc_chat_connection(&aConnection, dcc_chat_connection::Listen, aUser.nick_name(), user(aConnection.nick_name()), aUser, *this, iModel.io_task())));
		if (!iConnections.back()->listen(next_port()))
		{
			iConnections.pop_back();
			return false;
		}
		iConnections.back()->stream_server().add_observer(*this);
		iChatRequests.push_back(dcc_chat_request(iConnections.back()->stream_server().local_port(), aUser));

		u_long address = boost::asio::ip::address::from_string(iIpAddress).to_v4().to_ulong();;

		if (iAutomaticIpAddress)
		{
			if (!aConnection.have_local_address())
				aConnection.local_address_warning();
			address = aConnection.local_address();
		}

		message newMessage(aConnection, message::OUTGOING);
		newMessage.set_command(message::PRIVMSG);
		newMessage.parameters().push_back(aUser.nick_name());
		newMessage.parameters().push_back("\001DCC CHAT "); 
		newMessage.parameters().back() += "chat ";
		newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(address) + " ";
		newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(iConnections.back()->stream_server().local_port()) + "\001";
		aConnection.send_message(newMessage);

		return true;
	}

	bool dcc_connection_manager::accept_chat(connection& aConnection, const user& aUser, u_long aAddress, u_short aPort)
	{
		iConnections.push_back(dcc_connection_ptr(new dcc_chat_connection(&aConnection, dcc_chat_connection::Remote, aUser.nick_name(), user(aConnection.nick_name()), aUser, *this, iModel.io_task())));
		if (!iConnections.back()->connect(aAddress, aPort))
		{
			iConnections.pop_back();
			return false;
		}
		return true;
	}

	void dcc_connection_manager::remove_dcc_connection(dcc_connection& aConnection)
	{
		for (dcc_connection_list::iterator i = iConnections.begin(); i != iConnections.end(); ++i)
		{
			if (&**i == &aConnection)
			{
				if (aConnection.has_error() && aConnection.type() == dcc_connection::SEND)
					notify_observers(dcc_connection_manager_observer::NotifyDownloadFailure, aConnection, iModel.error_message(aConnection.error(), aConnection.error_string()));
				erase_object(iConnections, i);
				break;
			}
		}
	}

	model::id dcc_connection_manager::next_message_id()
	{
		return iModel.connection_manager().next_message_id();
	}

	bool dcc_connection_manager::find_message(model::id aMessageId, dcc_connection*& aConnection, dcc_message*& aMessage)
	{
		for (dcc_connection_list::iterator i = iConnections.begin(); i != iConnections.end(); ++i)
			if ((*i)->find_message(aMessageId, aConnection, aMessage))
				return true;
		return false;
	}

	bool dcc_connection_manager::any_chats() const
	{
		for (dcc_connection_list::const_iterator i = iConnections.begin(); i != iConnections.end(); ++i)
			if ((*i)->type() == dcc_connection::CHAT)
				return true;
		return false;
	}

	std::string dcc_connection_manager::connecting_to_message() const
	{
		return sConnectingToString;
	}

	std::string dcc_connection_manager::connected_to_message() const
	{
		return sConnectedToString;
	}

	std::string dcc_connection_manager::disconnected_message() const
	{
		return sDisconnectedString;
	}

	u_short dcc_connection_manager::next_port()
	{
		static u_short sNextPort = iDccBasePort;
		if (iConnections.size() == 1)
			sNextPort = iDccBasePort;
		else
			++sNextPort;
		return sNextPort;
	}

	void dcc_connection_manager::notify_observer(dcc_connection_manager_observer& aObserver, dcc_connection_manager_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		dcc_connection& theConnection = *const_cast<dcc_connection*>(reinterpret_cast<const dcc_connection*>(aParameter));
		switch(aType)
		{
		case dcc_connection_manager_observer::NotifyConnectionAdded:
			aObserver.dcc_connection_added(theConnection);
			break;
		case dcc_connection_manager_observer::NotifyConnectionRemoved:
			aObserver.dcc_connection_removed(theConnection);
			break;
		case dcc_connection_manager_observer::NotifyDownloadFailure:
			aObserver.dcc_download_failure(theConnection, *static_cast<const std::string*>(aParameter2));
			break;
		}
	}

	void dcc_connection_manager::packet_stream_added(dcc_server& aServer, dcc_stream& aStream)
	{
		for (dcc_connection_list::iterator c = iConnections.begin(); c != iConnections.end(); ++c)
		{
			dcc_connection& existingConnection = **c;
			if (existingConnection.has_stream_server() && &existingConnection.stream_server() == &aServer)
			{
				if (existingConnection.type() == dcc_connection::SEND)
				{
					dcc_send_connection& theSendListener = static_cast<dcc_send_connection&>(existingConnection);
					for (dcc_send_file_requests::iterator r = iSendFileRequests.begin(); r != iSendFileRequests.end(); ++r)
						if (r->iPort == theSendListener.stream_server().local_port())
						{
							iConnections.push_back(dcc_connection_ptr(new dcc_send_connection(theSendListener.connection(), dcc_send_connection::Upload, existingConnection.name(), existingConnection.local_user(), existingConnection.remote_user(), *this, iModel.io_task())));
							dcc_send_connection& theConnection = static_cast<dcc_send_connection&>(*iConnections.back());
							theConnection.set_stream(aServer.take_ownership(aStream));
							if (theSendListener.has_resume_data())
								theConnection.resume_data() = theSendListener.resume_data();
							static_cast<dcc_send_connection&>(*iConnections.back()).send_file(r->iPathName);
							iSendFileRequests.erase(r);
							break;
						}
				}
				else if (existingConnection.type() == dcc_connection::CHAT)
				{
					for (dcc_chat_requests::iterator r = iChatRequests.begin(); r != iChatRequests.end(); ++r)
						if (r->iPort == existingConnection.stream_server().local_port())
						{
							iConnections.push_back(dcc_connection_ptr(new dcc_chat_connection(static_cast<dcc_chat_connection&>(existingConnection).connection(), dcc_chat_connection::Local, existingConnection.name(), existingConnection.local_user(), existingConnection.remote_user(), *this, iModel.io_task())));
							iConnections.back()->set_stream(aServer.take_ownership(aStream));
							iChatRequests.erase(r);
							break;
						}
				}
				existingConnection.close();
				break;
			}
		}
	}

	void dcc_connection_manager::packet_stream_removed(dcc_server& aServer, dcc_stream& aStream)
	{
		for (dcc_connection_list::iterator c = iConnections.begin(); c != iConnections.end(); ++c)
		{
			dcc_connection& existingConnection = **c;
			if (&existingConnection.stream() == &aStream)
			{
				iConnections.erase(c);
				return;
			}
		}
	}

	void dcc_connection_manager::failed_to_accept_packet_stream(dcc_server& aServer, const boost::system::error_code& aError)
	{
	}
}
