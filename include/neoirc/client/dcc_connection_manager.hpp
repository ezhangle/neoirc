// dcc_connection_manager.h
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

#ifndef IRC_CLIENT_DCC_CONNECTION_MANAGER
#define IRC_CLIENT_DCC_CONNECTION_MANAGER

#include <neolib/timer.hpp>
#include <neolib/tcp_packet_stream_server.hpp>
#include <neoirc/client/dcc_send_connection.hpp>
#include <neoirc/client/dcc_chat_connection.hpp>

namespace irc
{
	class model;
	class connection;

	struct dcc_send_file_request
	{
		dcc_send_file_request(u_short aPort, const std::string& aPathName) : iPort(aPort), iPathName(aPathName) {}
		u_short iPort;
		std::string iPathName;
	};

	struct dcc_chat_request
	{
		dcc_chat_request(u_short aPort, const user& aUser) : iPort(aPort), iUser(aUser) {}
		u_short iPort;
		user iUser;
	};

	typedef std::vector<dcc_send_file_request> dcc_send_file_requests;
	typedef std::vector<dcc_chat_request> dcc_chat_requests;

	class dcc_connection_manager_observer
	{
		friend class dcc_connection_manager;
	private:
		virtual void dcc_connection_added(dcc_connection& aConnection) = 0;
		virtual void dcc_connection_removed(dcc_connection& aConnection) = 0;
		virtual void dcc_download_failure(dcc_connection& aConnection, const std::string& aErrorString) = 0;
	public:
		enum notify_type { NotifyConnectionAdded, NotifyConnectionRemoved, NotifyDownloadFailure };
	};

	typedef neolib::i_tcp_packet_stream_server_observer<dcc_packet> dcc_server_observer;
	typedef neolib::tcp_packet_stream_server<dcc_packet> dcc_server;

	class dcc_connection_manager : public neolib::observable<dcc_connection_manager_observer>, public neolib::manager_of<dcc_connection_manager, dcc_connection_manager_observer, dcc_connection>, private dcc_server_observer
	{
		friend class dcc_connection;
		friend class dcc_send_connection;
		friend class dcc_chat_connection;
	public:
		// types
		typedef std::shared_ptr<dcc_connection> dcc_connection_ptr;
		typedef std::list<dcc_connection_ptr> dcc_connection_list;

	public:
		// construction
		dcc_connection_manager(model& aModel);
		~dcc_connection_manager();

	public:
		// operations
		irc::model& model() { return iModel; }
		const irc::model& model() const { return iModel; }
		void set_ip_address(const std::string& aIpAddress) { iIpAddress = aIpAddress; }
		void set_automatic_ip_address(bool aAutomaticIpAddress) { iAutomaticIpAddress = aAutomaticIpAddress; }
		void set_dcc_base_port(u_short aDccBasePort) { iDccBasePort = aDccBasePort; }
		void set_dcc_fast_send(bool aDccFastSend) { iDccFastSend = aDccFastSend; }
		bool dcc_fast_send() const { return iDccFastSend; }
		bool send_file(connection& aConnection, const user& aUser, const std::string& aFilePath, const std::string& aFileName);
		bool receive_file(connection& aConnection, const user& aUser, u_long aAddress, u_short aPort, const std::string& aFilePath, const std::string& aFileName, unsigned long aFileSize, neolib::optional<unsigned long> aResumeFileSize = neolib::optional<unsigned long>());
		bool request_chat(connection& aConnection, const user& aUser);
		bool accept_chat(connection& aConnection, const user& aUser, u_long aAddress, u_short aPort);
		void remove_dcc_connection(dcc_connection& aConnection);
		dcc_connection_list& dcc_connections() { return iConnections; }
		const dcc_connection_list& dcc_connections() const { return iConnections; }
		model::id next_connection_id() { return iNextConnectionId++; }
		model::id next_message_id();
		dcc_connection* active_connection() const { return iActiveConnection; }
		void set_active_connection(dcc_connection* aActiveConnection) { iActiveConnection = aActiveConnection; }
		bool find_message(model::id aMessageId, dcc_connection*& aConnection, dcc_message*& aMessage);
		bool any_chats() const;
		static dcc_connection_ptr value(dcc_connection_list::iterator aIter) { return *aIter; }
		std::string connecting_to_message() const;
		std::string connected_to_message() const;
		std::string disconnected_message() const;
	private:
		u_short next_port();
		// implementation
		// from neolib::observable<dcc_connection_manager_observer>
		virtual void notify_observer(dcc_connection_manager_observer& aObserver, dcc_connection_manager_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from dcc_server_observer
		virtual void packet_stream_added(dcc_server& aServer, dcc_stream& aStream);
		virtual void packet_stream_removed(dcc_server& aServer, dcc_stream& aStream);
		virtual void failed_to_accept_packet_stream(dcc_server& aServer, const boost::system::error_code& aError);

	private:
		// attributes
		irc::model& iModel;
		std::string iIpAddress;
		bool iAutomaticIpAddress;
		u_short iDccBasePort;
		bool iDccFastSend;
		dcc_connection_list iConnections;
		dcc_send_file_requests iSendFileRequests;
		dcc_chat_requests iChatRequests;
		model::id iNextConnectionId;
		dcc_connection* iActiveConnection;

	public:
		static std::string sConnectingToString;
		static std::string sConnectedToString;
		static std::string sDisconnectedString;
	};
}

#endif //IRC_CLIENT_DCC_CONNECTION_MANAGER