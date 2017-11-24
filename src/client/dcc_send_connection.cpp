// dcc_send_connection.cpp
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
#include <errno.h>
#include <neolib/file.hpp>
#include <neoirc/client/dcc_send_connection.hpp>
#include <neoirc/client/dcc_connection_manager.hpp>
#include <neoirc/client/model.hpp>

namespace irc
{
	dcc_send_connection::dcc_send_connection(irc::connection& aConnection, send_type_e aSendType, const std::string& aName, const user& aLocalUser, const user& aRemoteUser, irc::dcc_connection_manager& aConnectionManager, neolib::io_task& aIoTask) : 
		iConnection(&aConnection), dcc_connection(SEND, aName, aLocalUser, aRemoteUser, aConnectionManager, aConnectionManager.model().io_task()),
		iSendType(aSendType), iFileSize(0), iBytesTransferred(0), iAck(0), iAckReceived(0), iSpeedSamples(), iSpeedCounter(0), iLastBytesTransferred(0),
		iSpeedGun(*this, aConnectionManager.model().io_task())
	{
		iConnection->add_observer(*this);
		iConnectionManager.object_created(*this);
	}

	dcc_send_connection::~dcc_send_connection()
	{
		iConnectionManager.object_destroyed(*this);
		if (iConnection != 0)
		{
			iConnection->remove_observer(*this);
			iConnection = 0;
		}
	}

	void dcc_send_connection::send_next_packet()
	{
		if (iFileSize == 0)
		{
			close();
			return;
		}
		if (iBytesTransferred == iFileSize)
		{
			if (iAckReceived == 0 && ntohl(iAck) == iBytesTransferred)
				close();
			return;
		}
		std::vector<char> buffer;
		buffer.resize(iFileSize - iBytesTransferred < packetSize ? iFileSize - iBytesTransferred : packetSize);
		if (!iFile.valid())
		{
			close();
			return;
		}
		neolib::large_file_seek(iFile, iBytesTransferred, SEEK_SET);
		if (fread(&buffer[0], 1, buffer.size(), iFile) != buffer.size())
		{
			close();
			return;
		}
		stream().send_packet(dcc_packet(&buffer[0], buffer.size()));
		iBytesTransferred += buffer.size();
		neolib::observable<dcc_send_connection_observer>::notify_observers(dcc_send_connection_observer::NotifyTransferProgress);
	}

	void dcc_send_connection::send_file(const std::string& aFilePath)
	{
		iFile.close();
		if (has_resume_data())
		{
			iBytesTransferred = resume_data()->iResumeFileSize;
			iLastBytesTransferred = iBytesTransferred;
		}
		iFilePath = aFilePath;
		iFileSize = static_cast<unsigned long>(neolib::large_file_size(aFilePath));
		neolib::observable<dcc_send_connection_observer>::notify_observers(dcc_send_connection_observer::NotifyTransferStarted);
		if (!open_file())
			return;
		send_next_packet();
	}

	void dcc_send_connection::receive_file(u_long aAddress, u_short aPort, const std::string& aFilePath, unsigned long aFileSize)
	{
		iFile.close();
		if (!connect(aAddress, aPort))
		{
			close();
			return;
		}
		iFilePath = aFilePath;
		iFileSize = aFileSize;
		neolib::observable<dcc_send_connection_observer>::notify_observers(dcc_send_connection_observer::NotifyTransferStarted);
		if (!open_file())
			return;
	}

	void dcc_send_connection::resume_file(u_long aAddress, u_short aPort, const std::string& aFilePath, const std::string& aFileName, unsigned long aFileSize, unsigned long aResumeFileSize)
	{
		iFile.close();
		iFilePath = aFilePath;
		iFileSize = aFileSize;
		iResumeData = resume_data_t(aAddress, aPort, aFileName, aResumeFileSize);
		message newMessage(*iConnection, message::OUTGOING);
		newMessage.set_command(message::PRIVMSG);
		newMessage.parameters().push_back(iRemoteUser.nick_name());
		newMessage.parameters().push_back("\001DCC RESUME "); 
		newMessage.parameters().back() += aFileName + " ";
		newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(aPort) + " ";
		newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(aResumeFileSize) + "\001";
		iConnection->send_message(newMessage);
	}

	bool dcc_send_connection::open_file()
	{
		iFile.close();
		iFile = (iSendType == Upload ? neolib::simple_file(iFilePath, "rb") : neolib::simple_file(neolib::create_file(iFilePath), "ab"));
		if (!iFile.valid())
		{
			if (iSendType == Download && iFile.error() == EACCES)
			{
				bool tryAgain = false;
				neolib::observable<dcc_send_connection_observer>::notify_observers(dcc_send_connection_observer::NotifyTransferAccessDenied, tryAgain);
			}
			close();
			return false;
		}
		return true;
	}

	void dcc_send_connection::notify_observer(dcc_send_connection_observer& aObserver, dcc_send_connection_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case dcc_send_connection_observer::NotifyTransferStarted:
			aObserver.dcc_transfer_started(*this);
			break;
		case dcc_send_connection_observer::NotifyTransferProgress:
			aObserver.dcc_transfer_progress(*this);
			break;
		case dcc_send_connection_observer::NotifyTransferAccessDenied:
			aObserver.dcc_transfer_access_denied(*this, const_cast<bool&>(*static_cast<const bool*>(aParameter)));
			break;
		}
	}

	void dcc_send_connection::packet_sent(dcc_stream& aStream, const dcc_packet& aPacket)
	{
		if (iSendType == Upload && iConnectionManager.dcc_fast_send())
			send_next_packet();
	}

	void dcc_send_connection::packet_arrived(dcc_stream& aStream, const dcc_packet& aPacket)
	{
		if (iSendType == Download)
		{
			if (!iFile.valid() || fwrite(static_cast<const char*>(aPacket.data()), 1, aPacket.length(), iFile) != aPacket.length())
			{
				close();
				return;
			}
			iBytesTransferred += aPacket.length();
			unsigned long ack = htonl(iBytesTransferred);
			stream().send_packet(dcc_packet(reinterpret_cast<void*>(&ack), sizeof(ack)));
			neolib::observable<dcc_send_connection_observer>::notify_observers(dcc_send_connection_observer::NotifyTransferProgress);
		}
		else if (iSendType == Upload)
		{
			char* ack = reinterpret_cast<char*>(&iAck);
			const char* data = static_cast<const char*>(aPacket.data());
			for (std::size_t i = 0; i < aPacket.length(); ++i, iAckReceived = (iAckReceived + 1) % 4)
			{
				*(ack + iAckReceived) = *(data + i);
			}
			if (iAckReceived == 0 && ntohl(iAck) == iBytesTransferred)
				send_next_packet();
		}
	}

	void dcc_send_connection::incoming_message(irc::connection& aConnection, const message& aMessage)
	{
		if (&aConnection != iConnection)
			return;
		if (aMessage.command() != message::PRIVMSG)
			return;
		typedef std::vector<std::pair<std::string::const_iterator, std::string::const_iterator> > bits_t;
		bits_t bits;
		neolib::tokens(aMessage.content(), std::string("\001 "), bits);
		if (neolib::to_upper(neolib::to_string(bits[0])) != "DCC")
			return;
		if (iSendType == Download && neolib::to_upper(neolib::to_string(bits[1])) == "ACCEPT" && bits.size() >= 5)
		{
			if (*bits[2].first == '\"')
			{
				for (bits_t::size_type i = 3; i < bits.size(); ++i)
					if (*(bits[i].second - 1) == '\"')
					{
						bits[2].second = bits[i].second;
						bits.erase(bits.begin() + 3, bits.begin() + i + 1);
						break;
					}
			}
			std::string fileName = neolib::to_string(bits[2]);
			if (fileName == iResumeData->iFileName && 
				neolib::string_to_unsigned_integer(neolib::to_string(bits[3])) == iResumeData->iPort &&
				neolib::string_to_unsigned_integer(neolib::to_string(bits[4])) == iResumeData->iResumeFileSize)
			{
				u_long address = iResumeData->iAddress;
				u_short port = iResumeData->iPort;
				iBytesTransferred = iResumeData->iResumeFileSize;
				iLastBytesTransferred = iBytesTransferred;
				if (!connect(address, port))
				{
					close();
					return;
				}
				neolib::observable<dcc_send_connection_observer>::notify_observers(dcc_send_connection_observer::NotifyTransferStarted);
				if (!open_file())
					return;
			}
		}
		else if (iSendType == Listen && neolib::to_upper(neolib::to_string(bits[1])) == "RESUME" && bits.size() >= 5)
		{
			if (*bits[2].first == '\"')
			{
				for (bits_t::size_type i = 3; i < bits.size(); ++i)
					if (*(bits[i].second - 1) == '\"')
					{
						bits[2].second = bits[i].second;
						bits.erase(bits.begin() + 3, bits.begin() + i + 1);
						break;
					}
			}
			std::string fileName = neolib::to_string(bits[2]);
			if (fileName == iName && 
				neolib::string_to_unsigned_integer(neolib::to_string(bits[3])) == stream_server().local_port())
			{
				iResumeData = resume_data_t(0, stream_server().local_port(), name(), neolib::string_to_unsigned_integer(neolib::to_string(bits[4])));
				message newMessage(*iConnection, message::OUTGOING);
				newMessage.set_command(message::PRIVMSG);
				newMessage.parameters().push_back(iRemoteUser.nick_name());
				newMessage.parameters().push_back("\001DCC ACCEPT "); 
				newMessage.parameters().back() += iResumeData->iFileName + " ";
				newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(iResumeData->iPort) + " ";
				newMessage.parameters().back() += neolib::unsigned_integer_to_string<char>(iResumeData->iResumeFileSize) + "\001";
				iConnection->send_message(newMessage);
			}
		}
	}

	void dcc_send_connection::connection_disconnected(irc::connection& aConnection)
	{
		if (&aConnection != iConnection)
			return;
		iConnection->remove_observer(*this);
		iConnection = 0;
		if (iSendType == Listen || (has_stream() && stream().closed()))
			close();
	}
}