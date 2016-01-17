// connection_manager.cpp
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
#include "connection_manager.hpp"

namespace irc
{
	std::string connection_manager::sConnectingToString;
	std::string connection_manager::sConnectedToString;
	std::string connection_manager::sTimedOutString;
	std::string connection_manager::sReconnectedToString;
	std::string connection_manager::sWaitingForString;
	std::string connection_manager::sDisconnectedString;
	std::string connection_manager::sClientName;
	std::string connection_manager::sClientVersion;
	std::string connection_manager::sClientEnvironment;
	std::string connection_manager::sNoticeBufferName;
	std::string connection_manager::sNoIpAddress;

	connection_manager::connection_manager(irc::model& aModel, neolib::random& aRandom, 
			irc::identities& aIdentities, irc::server_list& aServerList, 
			irc::identd& aIdentd, 
			irc::auto_joins& aAutoJoinList, irc::connection_scripts& aConnectionScripts, irc::ignore_list& aIgnoreList, 
			irc::notify& aNotifyList, irc::auto_mode& aAutoModeList) : 
		neolib::manager_of<connection_manager, connection_manager_observer, connection>(*this, connection_manager_observer::NotifyConnectionAdded, connection_manager_observer::NotifyConnectionRemoved),
		iModel(aModel),
		iRandom(aRandom), iIdentities(aIdentities), iServerList(aServerList), 
		iIdentd(aIdentd),  
		iAutoJoinList(aAutoJoinList), iConnectionScripts(aConnectionScripts),
		iIgnoreList(aIgnoreList), iNotifyList(aNotifyList), iAutoModeList(aAutoModeList), 
		iResolver(aModel.owner_thread()),
		iAutoReconnect(false), iReconnectAnyServer(true), iRetryCount(3), iRetryNetworkDelay(10), iDisconnectTimeout(120),
		iActiveBuffer(0), iFloodPrevention(false), iFloodPreventionDelay(500), iUseNoticeBuffer(false), iAutoWho(false), iAwayUpdate(false), iCreateChannelBufferUpfront(false), iAutoRejoinOnKick(false), iNextConnectionId(0), iNextBufferId(0), iNextMessageId(0)
	{
		iIdentities.add_observer(*this);
	}

	connection_manager::~connection_manager()
	{
		iIdentities.remove_observer(*this);

		erase_objects(iConnections, iConnections.begin(), iConnections.end());
	}

	std::string connection_manager::connecting_message(const server& aServer) const 
	{ 
		std::string ret = sConnectingToString;
		std::string::size_type i = ret.find("%S%");
		if (i != std::string::npos)
			ret.replace(i, 3, aServer.name());
		return ret;
	}

	std::string connection_manager::connected_message(const server& aServer) const 
	{ 
		std::string ret = sConnectedToString;
		std::string::size_type i = ret.find("%S%");
		if (i != std::string::npos)
			ret.replace(i, 3, aServer.name());
		return ret;
	}

	std::string connection_manager::timed_out_message(const server& aServer) const
	{
		std::string ret = sTimedOutString;
		std::string::size_type i = ret.find("%S%");
		if (i != std::string::npos)
			ret.replace(i, 3, aServer.name());
		return ret;
	}

	std::string connection_manager::reconnected_message(const server& aServer) const
	{ 
		std::string ret = sReconnectedToString;
		std::string::size_type i = ret.find("%S%");
		if (i != std::string::npos)
			ret.replace(i, 3, aServer.name());
		return ret;
	}

	std::string connection_manager::waiting_message() const
	{
		std::string ret = sWaitingForString;
		std::string::size_type i = ret.find("%T%");
		if (i != std::string::npos)
			ret.replace(i, 3, neolib::unsigned_integer_to_string<char>(iRetryNetworkDelay));
		return ret;
	}

	std::string connection_manager::disconnected_message(const server& aServer) const 
	{
		std::string ret = sDisconnectedString;
		std::string::size_type i = ret.find("%S%");
		if (i != std::string::npos)
			ret.replace(i, 3, aServer.name());
		return ret;
	}

	std::string connection_manager::notice_buffer_name(const server& aServer) const
	{
		std::string ret = sNoticeBufferName;
		std::string::size_type i = ret.find("%S%");
		if (i != std::string::npos)
			ret.replace(i, 3, aServer.name());
		return ret;
	}

	void connection_manager::set_flood_prevention(bool aFloodPrevention)
	{
		iFloodPrevention = aFloodPrevention;
		if (iFloodPrevention && !iFloodPreventor)
			iFloodPreventor = flood_preventor(iModel.owner_thread(), iFloodPreventionDelay, iModel.connection_manager());
		else if (!iFloodPrevention && iFloodPreventor)
			iFloodPreventor.reset();
		if (iFloodPreventor)
			iFloodPreventor->set_duration(iFloodPreventionDelay, true);
		if (!iFloodPrevention)
			bump_flood_buffers();
	}

	void connection_manager::set_flood_prevention_delay(long aFloodPreventionDelay)
	{
		iFloodPreventionDelay = aFloodPreventionDelay;
		set_flood_prevention(iFloodPrevention);
	}

	void connection_manager::add_key(const connection& aConnection, const std::string& aChannelName, const std::string& aChannelKey)
	{
		iKeys[std::make_pair(aConnection.server().network(), irc::make_string(aConnection, aChannelName))] = aChannelKey;
	}

	void connection_manager::remove_key(const connection& aConnection, const std::string& aChannelName)
	{
		key_list::iterator theKey = iKeys.find(std::make_pair(aConnection.server().network(), irc::make_string(aConnection, aChannelName)));
		if (theKey != iKeys.end())
			iKeys.erase(theKey);
	}

	bool connection_manager::has_key(const connection& aConnection, const std::string& aChannelName) const
	{
		key_list::const_iterator theKey = iKeys.find(std::make_pair(aConnection.server().network(), irc::make_string(aConnection, aChannelName)));
		return theKey != iKeys.end();
	}

	const std::string& connection_manager::key(const connection& aConnection, const std::string& aChannelName) const
	{
		key_list::const_iterator theKey = iKeys.find(std::make_pair(aConnection.server().network(), irc::make_string(aConnection, aChannelName)));
		if (theKey != iKeys.end())
			return theKey->second;
		else
		{
			static const std::string nothing;
			return nothing;
		}
	}

	void connection_manager::bump_flood_buffers()
	{
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); ++i)
			(*i)->bump_flood_buffer();
	}

	bool connection_manager::any_buffers() const
	{
		for (connection_list::const_iterator i = iConnections.begin(); i != iConnections.end(); ++i)
			if ((*i)->any_buffers())
				return true;
		return false;
	}

	bool connection_manager::find_message(model::id aMessageId, buffer*& aBuffer, message*& aMessage)
	{
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); ++i)
			if ((*i)->find_message(aMessageId, aBuffer, aMessage))
				return true;
		return false;
	}

	namespace
	{
		class buffer_collection : private connection_manager_observer, private connection_observer
		{
		public:
			struct no_next_buffer : std::logic_error { no_next_buffer() : std::logic_error("irc::::buffer_collection::no_next_buffer") {} };
		public:
			buffer_collection(connection& aConnection) : iConnectionManager(aConnection.connection_manager()), iConnection(aConnection), iConnectionDestroyed(false)
			{
				iConnectionManager.add_observer(*this);
				iConnection.add_observer(*this);
				iConnection.buffers(iBuffers);
			}
			~buffer_collection()
			{
				iConnectionManager.remove_observer(*this);
				if (!iConnectionDestroyed)
					iConnection.remove_observer(*this);
			}
			bool empty() const { return iBuffers.empty(); }
			buffer& next()
			{
				if (empty())
					throw no_next_buffer();
				buffer& ret = **(iBuffers.begin());
				iBuffers.erase(iBuffers.begin());
				return ret;
			}
		private:
			// from connection_observer
			virtual void connection_connecting(connection& aConnection) {}
			virtual void connection_registered(connection& aConnection) {}
			virtual void buffer_added(buffer& aBuffer) {}
			virtual void buffer_removed(buffer& aBuffer)
			{
				buffers_t::iterator i = std::find(iBuffers.begin(), iBuffers.end(), &aBuffer);
				if (i != iBuffers.end())
					iBuffers.erase(i);
			}
			virtual void incoming_message(connection& aConnection, const message& aMessage) {}
			virtual void outgoing_message(connection& aConnection, const message& aMessage) {}
			virtual void connection_quitting(connection& aConnection) {}
			virtual void connection_disconnected(connection& aConnection) {}
			virtual void connection_giveup(connection& aConnection) {}
			// from connection_manager_observer
			virtual void connection_added(connection& aConnection) {}
			virtual void connection_removed(connection& aConnection)
			{
				if (&aConnection == &iConnection)
					iConnectionDestroyed = true;
			}
			virtual void filter_message(connection& aConnection, const message& aMessage, bool& aFiltered) {}
			virtual bool query_disconnect(const connection& aConnection) { return false; }
			virtual void query_nickname(connection& aConnection) {}
			virtual void disconnect_timeout_changed() {}
			virtual void retry_network_delay_changed() {}
			virtual void buffer_activated(buffer& aActiveBuffer) {}
			virtual void buffer_deactivated(buffer& aDeactivatedBuffer) {}
		private:
			connection_manager& iConnectionManager;
			connection& iConnection;
			bool iConnectionDestroyed;
			typedef std::vector<buffer*> buffers_t;
			buffers_t iBuffers;
		};
	}

	bool connection_manager::new_message_all(message& aMessage)
	{
		bool handled = false;
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); )
		{
			connection& theConnection = (**i++);
			aMessage.set_origin(theConnection.user().msgto_form());
			buffer_collection buffers(theConnection);
			while(!buffers.empty())
			{
				buffer& theBuffer = buffers.next();
				switch(aMessage.command())
				{
				case message::PRIVMSG:
					if (theBuffer.type() == buffer::CHANNEL ||
						theBuffer.type() == buffer::USER)
					{
						if (aMessage.parameters().size() >= 1)
						{
							aMessage.parameters()[0] =theBuffer.name();
							theBuffer.new_message(aMessage);
							handled = true;
							aMessage.set_id(next_message_id());
						}
					}
					break;
				case message::PART:
					if (theBuffer.type() == buffer::CHANNEL)
					{
						if (aMessage.parameters().size() >= 1 &&
							theBuffer.is_channel(aMessage.parameters()[0]))
							aMessage.parameters().erase(aMessage.parameters().begin());
						theBuffer.insert_channel_parameter(aMessage);
						theBuffer.new_message(aMessage);
						handled = true;
						aMessage.set_id(next_message_id());
					}
					break;
				case message::UNKNOWN:
					if (aMessage.command_string().empty() && aMessage.direction() == message::INCOMING)
					{
						// should be an ECHO message
						theBuffer.new_message(aMessage);
						handled = true;
						aMessage.set_id(next_message_id());
						break;
					}
					// fall through...
				default:
					if (theBuffer.type() == buffer::SERVER)
					{
						theBuffer.new_message(aMessage);
						handled = true;
						aMessage.set_id(next_message_id());
					}
				}
			}
		}
		return handled;
	}

	void connection_manager::clear_all()
	{
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); )
		{
			connection& theConnection = (**i++);
			buffer_collection buffers(theConnection);
			while(!buffers.empty())
			{
				buffer& theBuffer = buffers.next();
				theBuffer.clear();
			}
		}
	}

	connection* connection_manager::find_connection(u_short aLocalPort, u_short aRemotePort)
	{
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); )
		{
			connection& theConnection = (**i++);
			if (theConnection.connected() &&
				theConnection.stream().connection().local_port() == aLocalPort &&
				theConnection.stream().connection().remote_port() == aRemotePort)
				return &theConnection;
		}
		return 0;
	}

	void connection_manager::add_observer(connection_manager_observer& aObserver)
	{
		neolib::observable<connection_manager_observer>::add_observer(aObserver);
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); )
		{
			connection& theConnection = (**i++);
			neolib::observable<connection_manager_observer>::notify_observer(aObserver, connection_manager_observer::NotifyConnectionAdded, theConnection);
		}
	}

	void connection_manager::set_retry_network_delay(unsigned int aRetryNetworkDelay) 
	{ 
		if (iRetryNetworkDelay == aRetryNetworkDelay)
			return;
		iRetryNetworkDelay = aRetryNetworkDelay; 	
		notify_observers(connection_manager_observer::NotifyRetryNetworkDelayChanged);
	}

	void connection_manager::set_disconnect_timeout(unsigned int aDisconnectTimeout)
	{
		if (iDisconnectTimeout == aDisconnectTimeout)
			return;
		iDisconnectTimeout = aDisconnectTimeout;
		notify_observers(connection_manager_observer::NotifyDisconnectTimeoutChanged);
	}

	void connection_manager::set_active_buffer(buffer* aActiveBuffer)
	{
		if (iActiveBuffer == aActiveBuffer)
			return;
		buffer* oldActiveBuffer = iActiveBuffer;
		iActiveBuffer = aActiveBuffer;
		if (iActiveBuffer != 0)
			notify_observers(connection_manager_observer::NotifyBufferActivated, iActiveBuffer);
		if (oldActiveBuffer != 0)
			notify_observers(connection_manager_observer::NotifyBufferDeactivated, oldActiveBuffer);
	}

	bool connection_manager::query_disconnect(const connection& aConnection)
	{
		for (neolib::observable<connection_manager_observer>::observer_list::iterator i = iObservers.begin(); i != iObservers.end(); ++i)
			if ((*i)->query_disconnect(aConnection))
				return true;
		return false;
	}

	void connection_manager::query_nickname(connection& aConnection)
	{
		for (neolib::observable<connection_manager_observer>::observer_list::iterator i = iObservers.begin(); i != iObservers.end(); ++i)
			(*i)->query_nickname(aConnection);
	}

	neolib::optional<server> connection_manager::server_from_string(const std::string& aServer)
	{
		neolib::vecarray<std::string, 2> parts;
		neolib::tokens(aServer, std::string(":"), parts, 2);
		if (parts.empty())
			return neolib::optional<server>();
		std::string network;
		for (server_list::iterator i = iServerList.begin(); i != iServerList.end(); ++i)
		{
			if (i->address() == parts[0])
			{
				network = i->network();
				server theServer = *i;
				if (parts.size() == 2)
				{
					int port = neolib::string_to_integer(parts[1]);
					server::port_list portList;
					portList.push_back(server::port_range(port, port));
					theServer.set_ports(portList);
					if ((parts[1][0] == '+' && !theServer.secure()) ||
						(parts[1][0] != '+' && theServer.secure()))
						continue;
				}
				return theServer;
			}
		}
		server theServer;
		theServer.set_address(parts[0]);
		theServer.set_name(parts[0]);
		theServer.set_network(network.empty() ? parts[0] : network);
		server::port_list portList;
		if (parts.size() == 2)
		{
			int port = neolib::string_to_integer(parts[1]);
			portList.push_back(server::port_range(port, port));
			if (parts[1][0] == '+')
			{
				theServer.set_secure(true);
				theServer.set_name(theServer.name() + " (secure)");
			}
		}
		else
			portList.push_back(server::port_range(6667, 6667));
		theServer.set_ports(portList);
		iServerList.push_back(theServer);
		iServerList.sort();
		write_server_list(iModel);
		return theServer;
	}

	connection* connection_manager::add_connection(const std::string& aServer, const identity& aIdentity, const std::string& aPassword, bool aManualConnectionRequest)
	{
		neolib::optional<server> theServer = server_from_string(aServer);
		if (theServer)
			return add_connection(*theServer, aIdentity, aPassword, aManualConnectionRequest);
		else
			return 0;
	}

	connection* connection_manager::add_connection(const server& aServer, const identity& aIdentity, const std::string& aPassword, bool aManualConnectionRequest)
	{
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); ++i)
		{
			if ((*i)->is_console())
			{
				if ((*i)->connect(aServer, aIdentity, aPassword, aManualConnectionRequest))
					return &**i;
				else
				{
					(*i)->reset_console();
					return 0;
				}
			}
		}
		connection_ptr newConnection(new connection(iModel, iRandom, *this, aServer, aIdentity, aPassword));
		iConnections.push_back(newConnection);
		if (!iConnections.back()->connect(aManualConnectionRequest))
		{
			iConnections.pop_back();
			return 0;
		}
		return &*iConnections.back();
	}

	connection* connection_manager::add_connection()
	{
		connection_ptr newConnection(new connection(iModel, iRandom, *this));
		iConnections.push_back(newConnection);
		newConnection->server_buffer().activate();
		return &*iConnections.back();
	}

	void connection_manager::remove_connection(connection& aConnection)
	{
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); ++i)
			if (&**i == &aConnection)
			{
				erase_object(iConnections, i);
				break;
			}
	}

	namespace
	{
		struct filter_message_item
		{
			connection& iConnection;
			const message& iMessage;
			bool& iFiltered;
			filter_message_item(connection& aConnection, const message& aMessage, bool& aFiltered) :
				iConnection(aConnection), iMessage(aMessage), iFiltered(aFiltered) {}	
		};
	}

	void connection_manager::filter_message(connection& aConnection, const message& aMessage, bool& aFiltered)
	{
		notify_observers(connection_manager_observer::NotifyFilterMessage, filter_message_item(aConnection, aMessage, aFiltered));
	}

	void connection_manager::notify_observer(connection_manager_observer& aObserver, connection_manager_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		connection& theConnection = *const_cast<connection*>(static_cast<const connection*>(aParameter));
		switch(aType)
		{
		case connection_manager_observer::NotifyConnectionAdded:
			aObserver.connection_added(theConnection);
			break;
		case connection_manager_observer::NotifyConnectionRemoved:
			aObserver.connection_removed(theConnection);
			break;
		case connection_manager_observer::NotifyFilterMessage:
			{
				const filter_message_item& item = *static_cast<const filter_message_item*>(aParameter);
				aObserver.filter_message(item.iConnection, item.iMessage, item.iFiltered);
			}
			break;
		case connection_manager_observer::NotifyDisconnectTimeoutChanged:
			aObserver.disconnect_timeout_changed();
			break;
		case connection_manager_observer::NotifyRetryNetworkDelayChanged:
			aObserver.retry_network_delay_changed();
			break;
		case connection_manager_observer::NotifyBufferActivated:
			aObserver.buffer_activated(**static_cast<buffer**>(const_cast<void*>(aParameter)));
			break;
		case connection_manager_observer::NotifyBufferDeactivated:
			aObserver.buffer_deactivated(**static_cast<buffer**>(const_cast<void*>(aParameter)));
			break;
		}
	}

	void connection_manager::identity_added(const identity& aEntry)
	{
	}

	void connection_manager::identity_updated(const identity& aEntry, const identity& aOldEntry)
	{
		for (connection_list::iterator i = iConnections.begin(); i != iConnections.end(); ++i)
			if ((*i)->identity() == aOldEntry)
			{
				(*i)->identity() = aEntry; 
				if ((*i)->nick_name() == aOldEntry.nick_name() &&
					(*i)->nick_name() != aEntry.nick_name())
					(*i)->set_nick_name(aEntry.nick_name(), false);
			}
	}

	void connection_manager::identity_removed(const identity& aEntry)
	{
	}
}
