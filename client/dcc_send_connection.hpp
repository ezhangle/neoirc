// dcc_send_connection.h
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

#ifndef IRC_CLIENT_DCC_SEND_CONNECTION
#define IRC_CLIENT_DCC_SEND_CONNECTION

#include <array>
#include <neolib/file.hpp>
#include "dcc_connection.hpp"
#include "connection.hpp"

namespace irc
{
	class dcc_send_connection_observer
	{
		friend class dcc_send_connection;
	private:
		virtual void dcc_transfer_started(dcc_send_connection& aConnection) = 0;
		virtual void dcc_transfer_progress(dcc_send_connection& aConnection) = 0;
		virtual void dcc_transfer_access_denied(dcc_send_connection& aConnection, bool& aRetry) = 0;
	public:
		enum notify_type { NotifyTransferStarted, NotifyTransferProgress, NotifyTransferAccessDenied };
	};

	class dcc_send_connection : public dcc_connection, public neolib::observable<dcc_send_connection_observer>, private connection_observer
	{
	public:
		// types
		enum send_type_e { Listen, Download, Upload };
		struct resume_data_t
		{
			u_long iAddress;
			u_short iPort;
			std::string iFileName;
			unsigned long iResumeFileSize;
			resume_data_t(u_long aAddress, u_short aPort, const std::string& aFileName, unsigned long aResumeFileSize) : 
				iAddress(aAddress), iPort(aPort), iFileName(aFileName), iResumeFileSize(aResumeFileSize) {}
			bool operator==(const resume_data_t& aRhs) const { return iAddress == aRhs.iAddress && iPort == aRhs.iPort && iFileName == aRhs.iFileName && iResumeFileSize == aRhs.iResumeFileSize; }
		};
		struct no_connection : std::logic_error { no_connection() : std::logic_error("irc::dcc_send_connection::no_connection") {} };
	public:
		// construction
		dcc_send_connection(connection& aConnection, send_type_e aSendType, const std::string& aName, const user& aLocalUser, const user& aRemoteUser, irc::dcc_connection_manager& aConnectionManager, neolib::io_thread& aOwnerThread);
		~dcc_send_connection();

	public:
		// operations
		bool has_connection() const { return iConnection != 0; }
		irc::connection& connection() { if (iConnection != 0) return *iConnection; throw no_connection(); }
		send_type_e send_type() const { return iSendType; }
		void send_file(const std::string& aFilePath);
		void receive_file(u_long aAddress, u_short aPort, const std::string& aFilePath, unsigned long aFileSize);
		void resume_file(u_long aAddress, u_short aPort, const std::string& aFilePath, const std::string& aFileName, unsigned long aFileSize, unsigned long aResumeFileSize);
		bool open_file();
		const std::string& file_path() const { return iFilePath; }
		unsigned long file_size() const { return iFileSize; }
		unsigned long bytes_transferred() const { return iBytesTransferred; }
		bool has_resume_data() const { return iResumeData.valid(); }
		neolib::optional<resume_data_t>& resume_data() { return iResumeData; }
		unsigned long speed(bool aAverage) const 
		{ 
			if (!aAverage)
				return iSpeedSamples[(iSpeedCounter-1)%kSpeedSamples];
			double total = 0.0;
			for (std::size_t i = 0; i < kSpeedSamples; ++i)
				total += static_cast<double>(iSpeedSamples[i]);
			return static_cast<unsigned long>(total / kSpeedSamples);
		}
		bool complete() const 
		{ 
			return iBytesTransferred == iFileSize && 
				(iSendType != Upload || (iAckReceived == 0 && ntohl(iAck) == iBytesTransferred)); 
		}

	private:
		// implementation
		void send_next_packet();
		// from neolib::observable<dcc_send_connection_observer>
		virtual void notify_observer(dcc_send_connection_observer& aObserver, dcc_send_connection_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from dcc_connection
		virtual void packet_sent(dcc_stream& aStream, const dcc_packet& aPacket);
		virtual void packet_arrived(dcc_stream& aStream, const dcc_packet& aPacket);
		// from connection_observer
		virtual void connection_connecting(irc::connection& aConnection) {}
		virtual void connection_registered(irc::connection& aConnection) {}
		virtual void buffer_added(buffer& aBuffer) {}
		virtual void buffer_removed(buffer& aBuffer) {}
		virtual void incoming_message(irc::connection& aConnection, const message& aMessage);
		virtual void outgoing_message(irc::connection& aConnection, const message& aMessage) {}
		virtual void connection_quitting(irc::connection& aConnection) {}
		virtual void connection_disconnected(irc::connection& aConnection);
		virtual void connection_giveup(irc::connection& aConnection) {}

	private:
		// attributes
		irc::connection* iConnection;
		send_type_e iSendType;
		std::string iFilePath;
		neolib::simple_file iFile;
		unsigned long iFileSize;
		unsigned long iBytesTransferred;
		unsigned long iAck;
		std::size_t iAckReceived;
		static const std::size_t kSpeedSamples = 10;
		typedef std::tr1::array<unsigned long, kSpeedSamples> speed_samples;
		speed_samples iSpeedSamples;
		std::size_t iSpeedCounter;
		unsigned long iLastBytesTransferred;
		struct speed_gun : private neolib::timer
		{
			dcc_send_connection& iOwner;
			speed_gun(dcc_send_connection& aOwner, neolib::io_thread& aOwnerThread) : neolib::timer(aOwnerThread, 1000), iOwner(aOwner) 
			{ 
			}
			virtual void ready() 
			{ 
				again();
				if (iOwner.iSpeedCounter != 0)
					iOwner.iSpeedSamples[(iOwner.iSpeedCounter++)%kSpeedSamples] = iOwner.iBytesTransferred - iOwner.iLastBytesTransferred;
				else
					for (std::size_t i = 0; i < kSpeedSamples; ++i)
						iOwner.iSpeedSamples[(iOwner.iSpeedCounter++)%kSpeedSamples] = iOwner.iBytesTransferred - iOwner.iLastBytesTransferred;
				iOwner.iLastBytesTransferred = iOwner.iBytesTransferred;
			}
		} iSpeedGun;
		neolib::optional<resume_data_t> iResumeData;

	public:
	};
}

#endif //IRC_CLIENT_DCC_SEND_CONNECTION
