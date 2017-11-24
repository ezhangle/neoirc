// connection_manager.h
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

#ifndef IRC_CLIENT_CONNECTION_MANAGER
#define IRC_CLIENT_CONNECTION_MANAGER

#include <neolib/resolver.hpp>
#include <neoirc/client/connection.hpp>
#include <neoirc/client/server.hpp>
#include <neoirc/client/identity.hpp>
#include <neoirc/client/connection_manager_observer.hpp>

namespace irc
{
	class auto_joins;
	class connection_scripts;
	class ignore_list;
	class notify;
	class auto_mode;
	class identd;

	class buffer;
	class message;

	class connection_manager : public neolib::observable<connection_manager_observer>, public neolib::manager_of<connection_manager, connection_manager_observer, connection>, private identities_observer
	{
		friend class connection;
		// types
	public:
		typedef std::shared_ptr<connection> connection_ptr;
		typedef std::list<connection_ptr> connection_list;
		typedef std::map<std::pair<std::string, string>, std::string> key_list;
		struct error {};
		class flood_preventor : public neolib::timer
		{
		public:
			flood_preventor(neolib::io_task& aIoTask, unsigned long aFloodPreventionDelay, connection_manager& aConnectionManager) :
				neolib::timer(aIoTask, aFloodPreventionDelay, true), iConnectionManager(aConnectionManager)
			{
			}
			flood_preventor(const flood_preventor& aOther) : neolib::timer(aOther), iConnectionManager(aOther.iConnectionManager)
			{
			}
			flood_preventor& operator=(const flood_preventor& aOther)
			{
				neolib::timer::operator=(aOther);
				return *this;
			}
			bool operator==(const flood_preventor& aRhs) const { return false; }
		private:
			// implementation
			virtual void ready()
			{
				again();
				iConnectionManager.bump_flood_buffers();
			}
		private:
			// attributes
			connection_manager& iConnectionManager;
		};

		// construction
	public:
		connection_manager(irc::model& aModel, neolib::random& aRandom, irc::identities& aIdentities, irc::server_list& aServerList, irc::identd& aIdentd, irc::auto_joins& aAutoJoinList, irc::connection_scripts& aConnectionScripts, irc::ignore_list& aIgnoreList, irc::notify& aNotifyList, irc::auto_mode& aAutoModeList);
		~connection_manager();

		// operations
	public:
		irc::model& model() { return iModel; }
		const irc::model& model() const { return iModel; }
		irc::server_list& server_list() { return iServerList; }
		const irc::server_list& server_list() const { return iServerList; }
		irc::identd& identd() { return iIdentd; }
		const irc::identd& identd() const { return iIdentd; }
		irc::auto_joins& auto_joins() { return iAutoJoinList; }
		const irc::auto_joins& auto_joins() const { return iAutoJoinList; }
		irc::connection_scripts& connection_scripts() { return iConnectionScripts; }
		const irc::connection_scripts& connection_scripts() const { return iConnectionScripts; }
		irc::ignore_list& ignore_list() { return iIgnoreList; }
		const irc::ignore_list& ignore_list() const { return iIgnoreList; }
		notify& notify_list() { return iNotifyList; }
		const notify& notify_list() const { return iNotifyList; }
		auto_mode& auto_mode_list() { return iAutoModeList; }
		const auto_mode& auto_mode_list() const { return iAutoModeList; }
		neolib::tcp_resolver& resolver() { return iResolver; }
		const neolib::tcp_resolver& resolver() const { return iResolver; }
		neolib::optional<server> server_from_string(const std::string& aServer);
		connection* add_connection(const std::string& aServer, const identity& aIdentity, const std::string& aPassword = std::string(), bool aManualConnectionRequest = false);
		connection* add_connection(const server& aServer, const identity& aIdentity, const std::string& aPassword = std::string(), bool aManualConnectionRequest = false);
		connection* add_connection();
		void remove_connection(connection& aConnection);
		connection_list& connections() { return iConnections; }
		const connection_list& connections() const { return iConnections; }
		bool auto_reconnect() const { return iAutoReconnect; }
		void set_auto_reconnect(bool aAutoReconnect) { iAutoReconnect = aAutoReconnect; }
		bool reconnect_any_server() const { return iReconnectAnyServer; }
		void set_reconnect_any_server(bool aReconnectAnyServer) { iReconnectAnyServer = aReconnectAnyServer; } 
		unsigned int retry_count() const { return iRetryCount; }
		void set_retry_count(unsigned int aRetryCount) { iRetryCount = aRetryCount; }
		unsigned int retry_network_delay() const { return iRetryNetworkDelay; }
		void set_retry_network_delay(unsigned int aRetryNetworkDelay);
		unsigned int disconnect_timeout() const { return iDisconnectTimeout; }
		void set_disconnect_timeout(unsigned int aDisconnectTimeout);
		buffer* active_buffer() const { return iActiveBuffer; }
		void set_active_buffer(buffer* aActiveBuffer);
		bool query_disconnect(const connection& aConnection);
		void query_nickname(connection& aConnection);
		std::string connecting_message(const server& aServer) const;
		std::string connected_message(const server& aServer) const;
		std::string timed_out_message(const server& aServer) const;
		std::string reconnected_message(const server& aServer) const;
		std::string waiting_message() const;
		std::string disconnected_message(const server& aServer) const;
		std::string notice_buffer_name(const server& aServer) const;
		void set_flood_prevention(bool aFloodPrevention);
		void set_flood_prevention_delay(long aFloodPreventionDelay);
		bool flood_prevention() const { return iFloodPrevention; }
		unsigned long flood_prevention_delay() const { return iFloodPreventionDelay; }
		void set_use_notice_buffer(bool aUseNoticeBuffer) { iUseNoticeBuffer = aUseNoticeBuffer; }
		bool use_notice_buffer() const { return iUseNoticeBuffer; }
		void set_auto_who(bool aAutoWho) { iAutoWho = aAutoWho; }
		bool auto_who() const { return iAutoWho; }
		void set_away_update(bool aAwayUpdate) { iAwayUpdate = aAwayUpdate; }
		bool away_update() const { return iAwayUpdate; }
		void set_create_channel_buffer_upfront(bool aCreateChannelBufferUpfront) { iCreateChannelBufferUpfront = aCreateChannelBufferUpfront; }
		bool create_channel_buffer_upfront() const { return iCreateChannelBufferUpfront; }
		void set_auto_rejoin_on_kick(bool aAutoRejoinOnKick) { iAutoRejoinOnKick = aAutoRejoinOnKick; }
		bool auto_rejoin_on_kick() const { return iAutoRejoinOnKick; }
		void add_key(const connection& aConnection, const std::string& aChannelName, const std::string& aChannelKey);
		void remove_key(const connection& aConnection, const std::string& aChannelName);
		bool has_key(const connection& aConnection, const std::string& aChannelName) const;
		const std::string& key(const connection& aConnection, const std::string& aChannelName) const;
		void bump_flood_buffers();
		model::id next_connection_id() { return iNextConnectionId++; }
		model::id next_buffer_id() { return iNextBufferId++; }
		model::id next_message_id() { return iNextMessageId++; }
		bool any_buffers() const;
		bool find_message(model::id aMessageId, buffer*& aBuffer, message*& aMessage);
		bool new_message_all(message& aMessage);
		void clear_all();
		static connection_ptr value(connection_list::iterator aIter) { return *aIter; }
		connection* find_connection(u_short aLocalPort, u_short aRemotePort);
		virtual void add_observer(connection_manager_observer& aObserver);
		// implementation
	private:
		void filter_message(connection& aConnection, const message& aMessage, bool& aFiltered);
		// from neolib::observable<connection_manager_observer>
		virtual void notify_observer(connection_manager_observer& aObserver, connection_manager_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from identities_observer
		virtual void identity_added(const identity& aEntry);
		virtual void identity_updated(const identity& aEntry, const identity& aOldEntry);
		virtual void identity_removed(const identity& aEntry);

		// attributes
	private:
		irc::model& iModel;
		neolib::random& iRandom;
		irc::identities& iIdentities;
		irc::server_list& iServerList;
		irc::identd& iIdentd;
		irc::auto_joins& iAutoJoinList;
		irc::connection_scripts& iConnectionScripts;
		irc::ignore_list& iIgnoreList;
		irc::notify& iNotifyList;
		irc::auto_mode& iAutoModeList;
		neolib::tcp_resolver iResolver;
		connection_list iConnections;
		bool iAutoReconnect;
		bool iReconnectAnyServer;
		unsigned int iRetryCount;
		unsigned int iRetryNetworkDelay;
		unsigned int iDisconnectTimeout;
		buffer* iActiveBuffer;
		bool iFloodPrevention;
		neolib::optional<flood_preventor> iFloodPreventor;
		unsigned long iFloodPreventionDelay;
		bool iUseNoticeBuffer;
		bool iAutoWho;
		bool iAwayUpdate;
		bool iCreateChannelBufferUpfront;
		bool iAutoRejoinOnKick;
		key_list iKeys;
		model::id iNextConnectionId;
		model::id iNextBufferId;
		model::id iNextMessageId;
	public:
		static std::string sConnectingToString;
		static std::string sConnectedToString;
		static std::string sTimedOutString;
		static std::string sReconnectedToString;
		static std::string sWaitingForString;
		static std::string sDisconnectedString;
		static std::string sClientName;
		static std::string sClientVersion;
		static std::string sClientEnvironment;
		static std::string sNoticeBufferName;
		static std::string sNoIpAddress;
	};
}

#endif //IRC_CLIENT_CONNECTION_MANAGER
