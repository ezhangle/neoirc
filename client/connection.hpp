// connection.h
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

#ifndef IRC_CLIENT_CONNECTION
#define IRC_CLIENT_CONNECTION

#include <neolib/manager_of.hpp>
#include <neolib/optional.hpp>
#include <neolib/packet_stream.hpp>
#include "server.hpp"
#include "identity.hpp"
#include "connection_manager_observer.hpp"
#include "buffer.hpp"
#include "server_buffer.hpp"
#include "channel_buffer.hpp"
#include "user_buffer.hpp"
#include "notice_buffer.hpp"
#include "ignore.hpp"

namespace irc
{
	class whois_requester;
	class who_requester;
	class dns_requester;
	class channel_list;
	class message;

	class connection_observer
	{
		friend class connection;
	private:
		virtual void connection_connecting(connection& aConnection) = 0;
		virtual void connection_registered(connection& aConnection) = 0;
		virtual void buffer_added(buffer& aBuffer) = 0;
		virtual void buffer_removed(buffer& aBuffer) = 0;
		virtual void incoming_message(connection& aConnection, const message& aMessage) = 0;
		virtual void outgoing_message(connection& aConnection, const message& aMessage) = 0;
		virtual void connection_quitting(connection& aConnection) = 0;
		virtual void connection_disconnected(connection& aConnection) = 0;
		virtual void connection_giveup(connection& aConnection) = 0;
	public:
		enum notify_type { NotifyConnectionConnecting, NotifyConnectionRegistered, NotifyBufferAdded, NotifyBufferRemoved, NotifyIncomingMessage, NotifyOutgoingMessage, NotifyConnectionQuitting, NotifyConnectionDisconnected, NotifyConnectionGiveup };
	};

	class connection : public neolib::observable<connection_observer>, public neolib::manager_of<connection, connection_observer, buffer>, private neolib::i_tcp_string_packet_stream_observer, private connection_manager_observer, private ignore_list_observer, private neolib::tcp_resolver::requester
	{
		// types
	private:
		friend class buffer;
		friend class connection_manager;
		struct reconnect_data
		{
			reconnect_data(const connection& aConnection, const neolib::optional<irc::server>& aNewServer);
			reconnect_data(const reconnect_data& aOther);
			struct entry
			{
				irc::server iServer;
				unsigned int iRetryAttempt;
				entry(const irc::server& aServer, unsigned int aRetryAttempt) : iServer(aServer), iRetryAttempt(aRetryAttempt) {}
				bool operator==(const entry& aOther) const { return iServer == aOther.iServer && iRetryAttempt == aOther.iRetryAttempt; }
			};
			unsigned int next_retry_attempts() const;
			bool is_network() const;
			entry& next_entry();
			bool operator==(const reconnect_data& aRhs) const;
			typedef std::list<entry> entries;
			entries iEntries;
			entries::iterator iCurrentEntry;
		};
	public:
		typedef std::vector<buffer*> buffer_list;
		typedef std::shared_ptr<buffer> buffer_ptr;
		typedef neolib::vecarray<buffer_ptr, 1> server_buffer_list;
		typedef neolib::vecarray<buffer_ptr, 1> notice_buffer_list;
		typedef std::map<irc::string, buffer_ptr> mapped_buffer_list;
		typedef mapped_buffer_list channel_buffer_list;
		typedef mapped_buffer_list user_buffer_list;
		class command_timer : public neolib::timer
		{
		public:
			typedef std::map<std::string, command_timer> list;
		public:
			command_timer(connection& aParent, double aInterval, const neolib::optional<std::size_t>& aRepeat, const std::string& aCommand) : 
			  neolib::timer(aParent.iModel.owner_thread(), static_cast<unsigned long>(aInterval * 1000)), iParent(aParent), iRepeat(aRepeat), iCommand(aCommand) {}
			command_timer(const command_timer& aOther) : neolib::timer(aOther), iParent(aOther.iParent), iRepeat(aOther.iRepeat), iCommand(aOther.iCommand) {}
			command_timer& operator=(const command_timer& aOther) 
			{
				neolib::timer::operator=(aOther);
				iRepeat = aOther.iRepeat;
				iCommand = aOther.iCommand;
				return *this;
			}
		public:
			const neolib::optional<std::size_t>& repeat() const { return iRepeat; }
			const std::string& command() const { return iCommand; }
		private:
			virtual void ready()
			{
				if (iParent.registered() && !iParent.iServerBuffer.empty())
					iParent.server_buffer().new_message(iCommand);
				again();
				if (iRepeat && --(*iRepeat) == 0)
					for (list::iterator i = iParent.iCommandTimers.begin(); i != iParent.iCommandTimers.end(); ++i)
						if (&i->second == this)
						{
							iParent.iCommandTimers.erase(i);
							return;
						}
			}
		private:
			connection& iParent;
			neolib::optional<std::size_t> iRepeat;
			std::string iCommand;
		};
		typedef command_timer::list command_timer_list;
		struct error {};

		// exceptions
	public:
		struct invalid_buffer_name : std::logic_error { invalid_buffer_name() : std::logic_error("irc::connection::invalid_buffer_name") {} };
		struct no_server_buffer : std::logic_error { no_server_buffer() : std::logic_error("irc::connection::no_server_buffer") {} };
		struct no_notice_buffer : std::logic_error { no_notice_buffer() : std::logic_error("irc::connection::no_notice_buffer") {} };
		struct not_server_buffer : std::logic_error { not_server_buffer() : std::logic_error("irc::connection::not_server_buffer") {} };
		struct not_notice_buffer : std::logic_error { not_notice_buffer() : std::logic_error("irc::connection::not_notice_buffer") {} };
		struct not_channel_buffer : std::logic_error { not_channel_buffer() : std::logic_error("irc::connection::not_channel_buffer") {} };
		struct channel_buffer_not_found : std::logic_error { channel_buffer_not_found() : std::logic_error("irc::connection::channel_buffer_not_found") {} };
		struct bad_message : std::logic_error { bad_message() : std::logic_error("irc::connection::bad_message") {} };
		struct mode_prefix_not_found : std::logic_error { mode_prefix_not_found() : std::logic_error("irc::connection::mode_prefix_not_found") {} };

		// construction
	public:
		connection(irc::model& aModel, neolib::random& aRandom, irc::connection_manager& aConnectionManager, const irc::server& aServer, const irc::identity& aIdentity, const std::string& aPassword = std::string());
		connection(irc::model& aModel, neolib::random& aRandom, irc::connection_manager& aConnectionManager);
		~connection();

		// operations
	public:
		irc::connection_manager& connection_manager() const { return iConnectionManager; }
		model::id id() const { return iId; }
		irc::casemapping::type casemapping() const { return iCasemapping; }
		operator irc::casemapping::type() const { return iCasemapping; }
		const std::string& chantypes() const { return iChantypes; }
		bool is_channel(const std::string& aName) const { return !aName.empty() && aName.find_first_of(iChantypes) == 0; }
		neolib::random& random() const { return iRandom; }
		void set_password(const std::string& aPassword) { iPassword = aPassword; }
		bool has_password() const { return !iPassword.empty(); }
		bool connect(bool aManualConnectionRequest = false);
		bool connect(const irc::server& aServer, const irc::identity& aIdentity, const std::string& aPassword, bool aManualConnectionRequest = false);
		bool will_auto_reconnect() const;
		void check_close();
		bool close(bool aSendQuit = false);
		void change_server(const std::string& aServer);
		neolib::tcp_string_packet_stream& stream() { return iPacketStream; }
		bool connected() const { return iPacketStream.connected(); }
		bool have_local_address() const { return iHaveLocalAddress; }
		void local_address_warning();
		u_long local_address() const;
		bool is_console() const { return iConsole; }
		void reset_console();
		bool registered() const { return iRegistered; }
		bool quitting() const { return iQuitting; }
		bool send_message(const std::string& aMessage);
		bool send_message(const message& aMessage, bool aFromFilter = false) { return send_message(server_buffer(), aMessage, aFromFilter); }
		bool send_message(buffer& aBuffer, const message& aMessage, bool aFromFilter = false);
		void receive_message(const message& aMessage, bool aFromFilter = false);
		model::id next_buffer_id();
		model::id next_message_id();
		void broadcast(const message& aMessage);
		const irc::server& server() const { return iServer; }
		const irc::identity& identity() const { return iIdentity; }
		irc::identity& identity() { return iIdentity; }
		const std::string& nick_name() const { return iNickName; }
		void set_nick_name(const std::string& aNickName, bool aSetLocal = true);
		const irc::user& user() const { return iUser; }
		buffer& create_buffer(const std::string& aName) { return buffer_from_name(aName); }
		channel_buffer& create_channel_buffer(const std::string& aName) { return channel_buffer_from_name(aName); }
		const buffer* find_buffer(const std::string& aName) const;
		buffer* find_buffer(const std::string& aName);
		buffer& buffer_from_name(const std::string& aName, bool aCreate = true, bool aRemote = false);
		channel_buffer& channel_buffer_from_name(const std::string& aName, bool aCreate = true);
		bool buffer_exists(const std::string& aName) const;
		irc::server_buffer& server_buffer(bool aCreate = false);
		irc::notice_buffer& notice_buffer(bool aCreate = true);
		channel_buffer_list& channel_buffers() { return iChannelBuffers; }
		user_buffer_list& user_buffers() { return iUserBuffers; }
		void buffers(buffer_list& aBuffers);
		bool has_user(const irc::user& aUser) const;
		bool has_user(const irc::user& aUser, const buffer& aBufferToExclude) const;
		const irc::user& user(const irc::user& aUser) const;
		whois_requester& whois() { return *iWhoisRequester; }
		who_requester& who() { return *iWhoRequester; }
		dns_requester& dns() { return *iDnsRequester; }
		irc::channel_list& channel_list() { return *iChannelList; }
		const std::pair<std::string, std::string>& prefixes() const { return iPrefixes; }
		bool is_prefix(char aPrefix) const;
		bool is_prefix_mode(char aMode) const;
		char mode_from_prefix(char aPrefix) const;
		char best_prefix(const std::string& aModes) const;
		unsigned int mode_compare_value(const std::string& aModes) const;
		void show_pings() { iShowPings = true; }
		void hide_pings() { iShowPings = false; }
		bool latency_available() const { return iLatency_ms.is_initialized(); }
		uint64_t latency() const { return *iLatency_ms; }
		bool add_command_timer(const std::string& aName, double aInterval, const neolib::optional<std::size_t>& aRepeat, const std::string& aCommand);
		bool delete_command_timer(const std::string& aName);
		const command_timer_list& command_timers() const { return iCommandTimers; }
		virtual void add_observer(connection_observer& aObserver);
		static buffer_ptr value(server_buffer_list::iterator aIter) { return *aIter; }
		static buffer_ptr value(mapped_buffer_list::iterator aIter) { return aIter->second; }
	private:
		// implementation
		void handle_data();
		void reset();
		void disconnect();
		void handle_disconnection();
		bool reconnect(const neolib::optional<irc::server>& aNewServer = neolib::optional<irc::server>(), bool aUserChangeServer = false);
		void send_ping();
		void timeout();
		void bump_flood_buffer();
		bool any_buffers() const;
		bool find_message(model::id aMessageId, buffer*& aBuffer, message*& aMessage);
		void query_host();
		void remove_buffer(buffer& aBuffer);
		void handle_orphan(buffer& aBuffer);
		// from neolib::observable<connection_observer>
		virtual void notify_observer(connection_observer& aObserver, connection_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from neolib::manager_of<connection, connection_observer, buffer>
	public:
		virtual void object_created(buffer& aObject);
		virtual void object_destroyed(buffer& aObject);
	private:
		// from neolib::i_tcp_string_packet_stream_observer
		virtual void connection_established(neolib::tcp_string_packet_stream& aStream);
		virtual void connection_failure(neolib::tcp_string_packet_stream& aStream, const boost::system::error_code& aError);
		virtual void packet_sent(neolib::tcp_string_packet_stream& aStream, const neolib::string_packet& aPacket);
		virtual void packet_arrived(neolib::tcp_string_packet_stream& aStream, const neolib::string_packet& aPacket);
		virtual void transfer_failure(neolib::tcp_string_packet_stream& aStream, const boost::system::error_code& aError);
		virtual void connection_closed(neolib::tcp_string_packet_stream& aStream);
		// from connection_manager_observer
		virtual void connection_added(connection& aConnection) {}
		virtual void connection_removed(connection& aConnection) {}
		virtual void filter_message(connection& aConnection, const message& aMessage, bool& aFiltered) {}
		virtual bool query_disconnect(const connection& aConnection) { return false; }
		virtual void query_nickname(connection& aConnection) {}
		virtual void disconnect_timeout_changed();
		virtual void retry_network_delay_changed();
		virtual void buffer_activated(buffer& aActiveBuffer) {}
		virtual void buffer_deactivated(buffer& aDeactivatedBuffer) {}
		// from ignore_list_observer
		virtual void ignore_added(const ignore_entry& aEntry);
		virtual void ignore_updated(const ignore_entry& aEntry);
		virtual void ignore_removed(const ignore_entry& aEntry);
		// from neolib::tcp_resolver
		virtual void host_resolved(const std::string& aHostName, neolib::tcp_resolver::iterator aHost);
		virtual void host_not_resolved(const std::string& aHostName, const boost::system::error_code& aError);

	private:
		// attributes
		model& iModel;
		neolib::random& iRandom;
		irc::connection_manager& iConnectionManager;
		class back_after_yield : public neolib::timer
		{
			// construction
		public:
			back_after_yield(connection& aParent, neolib::io_thread& aOwnerThread) : 
				neolib::timer(aOwnerThread, 0), 
				iParent(aParent) 
			{ 
			}
			// implementation
		private:
			virtual void ready()
			{
				if (!iParent.iMessageBuffer.empty())
					iParent.handle_data();
			}
			// attributes
		private:
			connection& iParent;
		};
		std::shared_ptr<back_after_yield> iBackAfterYield;
		class retry_network_waiter : public neolib::timer
		{
		public:
			// construction
			retry_network_waiter(connection& aConnection, neolib::io_thread& aOwnerThread, unsigned long aInterval) : 
			  neolib::timer(aOwnerThread, aInterval, false), iConnection(aConnection), iIn(false) {}
		public:
			bool in() const { return iIn; }
		private:
			// from neolib::timer
			virtual void ready() { iIn = true; iConnection.reconnect(); iIn = false; }
		private:
			// attributes
			connection& iConnection;
			bool iIn;
		} iRetryNetworkWaiter;
		model::id iId;
		irc::server iServer;
		irc::identity iIdentity;
		std::string iNickName;
		std::string iPassword;
		irc::user iUser;
		neolib::tcp_string_packet_stream iPacketStream;
		neolib::tcp_resolver iResolver;
		bool iGotConnection;
		bool iConsole;
		bool iRegistered;
		bool iPreviouslyRegistered;
		bool iClosing;
		bool iQuitting;
		bool iChangingServer;
		std::string iMessageBuffer;
		server_buffer_list iServerBuffer;
		notice_buffer_list iNoticeBuffer;
		channel_buffer_list iChannelBuffers;
		user_buffer_list iUserBuffers;
		std::auto_ptr<whois_requester> iWhoisRequester;
		std::auto_ptr<who_requester> iWhoRequester;
		std::auto_ptr<dns_requester> iDnsRequester;
		std::auto_ptr<irc::channel_list> iChannelList;
		uint64_t iTimeLastMessageSent;
		typedef std::deque<message> flood_prevention_buffer;
		flood_prevention_buffer iFloodPreventionBuffer;
		neolib::optional<reconnect_data> iReconnectData;
		neolib::optional<identity::alternate_nick_names_t> iAlternateNickNames;
		casemapping::type iCasemapping;
		std::pair<std::string, std::string> iPrefixes;
		std::string iChantypes;
		neolib::timer_callback iPinger;
		bool iWaitingForPong;
		uint64_t iPingSent_ms;
		boost::optional<uint64_t> iLatency_ms;
		bool iShowPings;
		bool iHostQuery;
		std::string iHostName;
		bool iHaveLocalAddress;
		u_long iLocalAddress;
		struct away_updater : neolib::timer
		{
			away_updater(model& aModel, connection& aParent) : 
				neolib::timer(aModel.owner_thread(), 30 * 1000), 
				iParent(aParent), 
				iNextChannel(aParent.iChannelBuffers.begin())
			{
			}
			virtual void ready();
			connection& iParent;
			channel_buffer_list::iterator iNextChannel;
		} iAwayUpdater;
		command_timer_list iCommandTimers;
	public:
		static std::string sConsolesTitle;
		static std::string sConsoleTitle;
		static std::string sIgnoreAddedMessage;
		static std::string sIgnoreUpdatedMessage;
		static std::string sIgnoreRemovedMessage;
	};

	inline irc::string make_string(const connection& aConnection, const std::string& s)
	{
		return make_string(aConnection.casemapping(), s);
	}
	inline irc::wstring make_string(const connection& aConnection, const std::wstring& s)
	{
		return make_string(aConnection.casemapping(), s);
	}
}

#endif //IRC_CLIENT_CONNECTION