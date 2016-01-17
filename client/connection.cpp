// connection.cpp
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
#include <ctime>
#include "connection.hpp"
#include "connection_manager.hpp"
#include "whois.hpp"
#include "who.hpp"
#include "dns.hpp"
#include "ignore.hpp"
#include "notify.hpp"
#include "auto_mode.hpp"
#include "channel_list.hpp"

namespace irc
{
	std::string connection::sConsoleTitle;
	std::string connection::sConsolesTitle;
	std::string connection::sIgnoreAddedMessage;
	std::string connection::sIgnoreUpdatedMessage;
	std::string connection::sIgnoreRemovedMessage;

	connection::reconnect_data::reconnect_data(const connection& aConnection, const neolib::optional<irc::server>& aNewServer) : iCurrentEntry(iEntries.end())
	{
		irc::connection_manager& theConnectionManager = aConnection.connection_manager();
		if (aNewServer.valid())
			iEntries.push_back(entry(*aNewServer, 0));
		if (theConnectionManager.reconnect_any_server())
		{
			for (server_list::iterator i = theConnectionManager.server_list().begin(); i != theConnectionManager.server_list().end(); ++i)
			{
				if (aNewServer.valid() && *i == *aNewServer)
					continue;
				if (i->network() == aConnection.server().network() &&
					i->secure() == aConnection.server().secure())
				{
					bool isCurrentServer = (i->name() == aConnection.server().name());
					iEntries.push_back(entry(*i, isCurrentServer ? 1 : 0));
					if (isCurrentServer && aNewServer.invalid())
						--iCurrentEntry;
				}
			}
			if (iEntries.empty())
			{
				iEntries.push_back(entry(aConnection.server(), 0));
				iCurrentEntry = iEntries.begin();
			}
			else if (iCurrentEntry != iEntries.end())
				++iCurrentEntry;
			if (iCurrentEntry == iEntries.end())
				iCurrentEntry = iEntries.begin();
		}
		else
		{
			iEntries.push_back(entry(aConnection.server(), 1));
			iCurrentEntry = iEntries.begin();
		}
	}

	connection::reconnect_data::reconnect_data(const reconnect_data& aOther) : iEntries(aOther.iEntries), iCurrentEntry(iEntries.end())
	{
		for (entries::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (*i == *aOther.iCurrentEntry)
			{
				iCurrentEntry = i;
				break;
			}
	}

	unsigned int connection::reconnect_data::next_retry_attempts() const
	{
		return iCurrentEntry->iRetryAttempt;
	}

	bool connection::reconnect_data::is_network() const
	{
		if (iEntries.size() == 1)
			return true;
		entries::const_iterator nextEntry = iCurrentEntry;
		entries::const_iterator previousEntry = nextEntry;
		if (previousEntry == iEntries.begin())
			previousEntry = iEntries.end();
		--previousEntry;
		return nextEntry->iRetryAttempt == previousEntry->iRetryAttempt;
	}

	connection::reconnect_data::entry& connection::reconnect_data::next_entry()
	{
		entry& ret = *iCurrentEntry++;
		if (iCurrentEntry == iEntries.end())
			iCurrentEntry = iEntries.begin();
		return ret;
	}

	bool connection::reconnect_data::operator==(const reconnect_data& aRhs) const 
	{
		return iEntries == aRhs.iEntries;
	}

	connection::connection(irc::model& aModel, neolib::random& aRandom, irc::connection_manager& aConnectionManager, const irc::server& aServer, const irc::identity& aIdentity, const std::string& aPassword) : 
		neolib::manager_of<connection, connection_observer, buffer>(*this, connection_observer::NotifyBufferAdded, connection_observer::NotifyBufferRemoved),
		iModel(aModel),
		iRandom(aRandom), iConnectionManager(aConnectionManager),
		iRetryNetworkWaiter(*this, aModel.owner_thread(), aConnectionManager.retry_network_delay() * 1000),
		iId(aConnectionManager.next_connection_id()),
		iServer(aServer), iIdentity(aIdentity), 
		iNickName(aIdentity.nick_name()), iPassword(aPassword), 
		iUser(aIdentity.nick_name(), casemapping::rfc1459, false),
		iPacketStream(aModel.owner_thread()),
		iResolver(aModel.owner_thread()),
		iGotConnection(false), iConsole(false), iRegistered(false), iPreviouslyRegistered(false), iClosing(false), iQuitting(false), iChangingServer(false),
		iTimeLastMessageSent(0), 
		iCasemapping(casemapping::rfc1459), 
		iWhoisRequester(new whois_requester(*this)),
		iWhoRequester(new who_requester(*this)),
		iDnsRequester(new dns_requester(*this)),
		iChannelList(new irc::channel_list(*this)),
		iPrefixes(std::make_pair(std::string("ov"), std::string("@+"))),
		iChantypes("&#+!"),
		iPinger(aModel.owner_thread(), [this](neolib::timer_callback& aTimer)
		{
			aTimer.again();
			if (!iWaitingForPong)
				send_ping();
			else if (neolib::thread::elapsed_ms() - iPingSent_ms > iConnectionManager.disconnect_timeout() * 1000)
				timeout();
			else if (neolib::thread::elapsed_ms() - iPingSent_ms > 30 * 1000)
				iLatency_ms.reset();
		}, 10000, true),
		iWaitingForPong(false), iPingSent_ms(0ull), iShowPings(false), iHostQuery(false), iHaveLocalAddress(false), iLocalAddress(INADDR_NONE),
		iAwayUpdater(aModel, *this)
	{
		iPacketStream.add_observer(*this);
		iConnectionManager.add_observer(*this);
		iConnectionManager.ignore_list().add_observer(*this);
		iConnectionManager.object_created(*this);
	}

	connection::connection(irc::model& aModel, neolib::random& aRandom, irc::connection_manager& aConnectionManager) : 
		neolib::manager_of<connection, connection_observer, buffer>(*this, connection_observer::NotifyBufferAdded, connection_observer::NotifyBufferRemoved),
		iModel(aModel),
		iRandom(aRandom), iConnectionManager(aConnectionManager),
		iRetryNetworkWaiter(*this, aModel.owner_thread(), aConnectionManager.retry_network_delay() * 1000),
		iId(aConnectionManager.next_connection_id()),
		iIdentity(iModel.default_identity()), 
		iNickName(iIdentity.nick_name()), 
		iUser(iIdentity.nick_name(), casemapping::rfc1459, false),
		iPacketStream(aModel.owner_thread()),
		iResolver(aModel.owner_thread()),
		iGotConnection(false), iConsole(true), iRegistered(false), iPreviouslyRegistered(false), iClosing(false), iQuitting(false), iChangingServer(false),
		iTimeLastMessageSent(0), 
		iCasemapping(casemapping::rfc1459), 
		iWhoisRequester(new whois_requester(*this)),
		iWhoRequester(new who_requester(*this)),
		iDnsRequester(new dns_requester(*this)),
		iChannelList(new irc::channel_list(*this)),
		iPrefixes(std::make_pair(std::string("ov"), std::string("@+"))),
		iChantypes("&#+!"),
		iPinger(aModel.owner_thread(), [this](neolib::timer_callback& aTimer)
		{
			aTimer.again();
			if (!iWaitingForPong)
				send_ping();
			else if (neolib::thread::elapsed_ms() - iPingSent_ms > iConnectionManager.disconnect_timeout() * 1000)
				timeout();
		}, 10000, true),
		iWaitingForPong(false), iPingSent_ms(0ull), iShowPings(false), iHostQuery(false), iHaveLocalAddress(false), iLocalAddress(INADDR_NONE),
		iAwayUpdater(aModel, *this)
	{
		iPacketStream.add_observer(*this);
		iConnectionManager.add_observer(*this);
		iConnectionManager.ignore_list().add_observer(*this);
		iServer.set_network(sConsolesTitle);
		iServer.set_name(sConsoleTitle);
		iConnectionManager.object_created(*this);
		server_buffer(true);
	}

	connection::~connection()
	{
		iClosing = true;
		disconnect();

		iConnectionManager.ignore_list().remove_observer(*this);
		iConnectionManager.remove_observer(*this);

		erase_objects(iChannelBuffers, iChannelBuffers.begin(), iChannelBuffers.end());
		erase_objects(iUserBuffers, iUserBuffers.begin(), iUserBuffers.end());
		erase_object(iNoticeBuffer, iNoticeBuffer.begin());
		erase_object(iServerBuffer, iServerBuffer.begin());

		iConnectionManager.object_destroyed(*this);
	}

	void connection::handle_orphan(buffer& aBuffer)
	{
		if (iClosing || iQuitting)
			return;
		switch(aBuffer.type())
		{
		case buffer::SERVER:
			break;
		case buffer::CHANNEL:
			remove_buffer(aBuffer);
			break;
		case buffer::USER:
			remove_buffer(aBuffer);
			break;
		case buffer::NOTICE:
			remove_buffer(aBuffer);
			break;
		}
	}

	bool connection::connect(bool aManualConnectionRequest)
	{
		if (aManualConnectionRequest && iIdentity.last_server_used() != iServer.key())
		{
			iIdentity.set_last_server_used(iServer.key());
			iModel.identities().update_item(*iModel.identities().find_item(iIdentity), iIdentity);
			write_identity_list(iModel, iModel.default_identity().nick_name());
		}

		if (iServer.password() && iPassword.empty())
		{
			if (!iModel.enter_password(*this))
				return false;
		}

		if (!iPacketStream.open(iServer.address().c_str(), iServer.port(iRandom), iServer.secure()))
		{
			if (!iConnectionManager.auto_reconnect())
				return false;
			else if (!reconnect())
				return false;
			else
				return true;
		}

		if (!iGotConnection)
		{
			notify_observers(connection_observer::NotifyConnectionConnecting);
			message connecting(*this, message::INCOMING);
			connecting.parameters().push_back(iConnectionManager.connecting_message(iServer));
			server_buffer(true).new_message(connecting);
		}

		return true;
	}

	bool connection::connect(const irc::server& aServer, const irc::identity& aIdentity, const std::string& aPassword, bool aManualConnectionRequest)
	{
		iConsole = false;
		iServer = aServer;
		iIdentity = aIdentity;
		iNickName = iIdentity.nick_name();
		iUser = irc::user(iIdentity.nick_name(), casemapping::rfc1459, false);
		iPassword = aPassword;
		server_buffer(true).set_server(iServer);
		server_buffer(true).set_name(iServer.name());
		return connect(aManualConnectionRequest);
	}

	void connection::check_close()
	{
		if (iClosing || iQuitting)
			return;
		if (iChannelBuffers.empty() && iUserBuffers.empty() && (!connected() || iConnectionManager.query_disconnect(*this)))
			close();
	}

	void connection::handle_data()
	{
		std::string::size_type messageEnd = iMessageBuffer.find("\n");
		uint64_t startTime = neolib::thread::elapsed_ms();
		while(messageEnd != std::string::npos)
		{
			uint64_t elapsed = neolib::thread::elapsed_ms() - startTime;
			if (elapsed > 25 && !iModel.owner_thread().do_io(true))
			{
				if (iBackAfterYield && !iBackAfterYield->waiting())
					iBackAfterYield->reset();
				return;
			}
			std::string messagePart = iMessageBuffer.substr(0, messageEnd);
			if (!messagePart.empty() && messagePart[messagePart.size() - 1]  == '\r') 
				messagePart.erase(messagePart.size() - 1, 1);
			iMessageBuffer = iMessageBuffer.substr(messageEnd+1);
			messageEnd = iMessageBuffer.find("\n");
			message newMessage(*this, message::INCOMING);
			newMessage.parse_command(messagePart);
			newMessage.parse_parameters(messagePart, false, true);
			receive_message(newMessage);
		}
	}

	void connection::reset()
	{
		iWaitingForPong = false;
		iPingSent_ms = 0ull; 
		iLatency_ms.reset();
		iGotConnection = false;
		iRegistered = false;
		iAlternateNickNames.reset();
	}

	bool connection::reconnect(const neolib::optional<irc::server>& aNewServer, bool aUserChangeServer)
	{
		reset();

		if (aNewServer)
		{
			if (aNewServer->network() != iServer.network())
			{
				iReconnectData.reset();
				iPassword.clear();
			}
			iServer = *aNewServer;
		}

		if (!iReconnectData)
			iReconnectData = reconnect_data(*this, aNewServer);

		if (iReconnectData->next_retry_attempts() == iConnectionManager.retry_count())
		{
			iReconnectData.reset();
			return false;
		}

		if (!aUserChangeServer && !iRetryNetworkWaiter.in() && iReconnectData->is_network() && iConnectionManager.retry_network_delay() != 0)
		{
			iRetryNetworkWaiter.reset();
			message reason(*this, message::INCOMING);
			reason.parameters().push_back(iConnectionManager.waiting_message());
			if (!iServerBuffer.empty())
				server_buffer().new_message(reason);
			return true;
		}
		if (iRetryNetworkWaiter.enabled())
			iRetryNetworkWaiter.disable();

		reconnect_data::entry& next = iReconnectData->next_entry();
		++next.iRetryAttempt;
		iServer = next.iServer;
		server_buffer(true).set_server(next.iServer);
		server_buffer(true).set_name(next.iServer.name());
		return connect(aUserChangeServer);
	}

	bool connection::close(bool aSendQuit)
	{
		if (aSendQuit && registered() && !iQuitting)
		{
			message quitMessage(*this, message::OUTGOING);
			quitMessage.set_command(message::QUIT);
			send_message(quitMessage);
			return true;
		}
		iConnectionManager.remove_connection(*this);
		return true;
	}

	void connection::disconnect()
	{
		iReconnectData.reset();
		if (iRetryNetworkWaiter.enabled())
			iRetryNetworkWaiter.disable();
		if (iPacketStream.opened())
			iPacketStream.close();
		else if (!iConsole)
			handle_disconnection();
	}

	void connection::handle_disconnection()
	{
		message reason(*this, message::INCOMING);
		if (iPacketStream.has_error())
			reason.parameters().push_back(iModel.error_message(iPacketStream.error_code(), iPacketStream.error()));
		else
			reason.parameters().push_back(iConnectionManager.disconnected_message(iServer));

		iMessageBuffer = "";
		iFloodPreventionBuffer.clear();

		iHostQuery = false;
		iHostName = "";
		iHaveLocalAddress = false;
		iLocalAddress = INADDR_NONE;

		if (!iServerBuffer.empty())
		{
			server_buffer().new_message(reason);
			server_buffer().set_ready(false);
		}
		if (!iNoticeBuffer.empty() && notice_buffer().is_ready())
			notice_buffer().set_ready(false);
		for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
			if ((*i).second->is_ready())
			{
				reason.set_id(next_message_id());
				(*i).second->new_message(reason);
				(*i).second->set_ready(false);
			}
		for (user_buffer_list::iterator i = iUserBuffers.begin(); i != iUserBuffers.end(); ++i)
			if ((*i).second->is_ready())
			{
				reason.set_id(next_message_id());
				(*i).second->new_message(reason);
				(*i).second->set_ready(false);
			}

		if (iPacketStream.opened())
			iPacketStream.close();

		notify_observers(connection_observer::NotifyConnectionDisconnected);
	}

	void connection::change_server(const std::string& aServer)
	{
		iChangingServer = true;
		disconnect();
		iConsole = false;
		reconnect(iConnectionManager.server_from_string(aServer), true);
		iChangingServer = false;
	}

	const buffer* connection::find_buffer(const std::string& aName) const
	{
		if (aName.empty())
			return 0;

		if (aName == "$")
		{
			if (!iServerBuffer.empty())
				return &*iServerBuffer[0];
			else
				return 0;
		}

		if (is_channel(aName))
		{
			channel_buffer_list::const_iterator i = iChannelBuffers.find(irc::make_string(*this, aName));
			if (i != iChannelBuffers.end())
				return &*(*i).second;
			else
				return 0;
		}
		else
		{
			user_buffer_list::const_iterator i = iUserBuffers.find(irc::make_string(*this, irc::user(aName, *this).nick_name()));
			if (i != iUserBuffers.end())
				return &*(*i).second;
			else
				return 0;
		}
	}

	buffer* connection::find_buffer(const std::string& aName)
	{
		return const_cast<buffer*>(const_cast<const connection*>(this)->find_buffer(aName));
	}

	bool connection::buffer_exists(const std::string& aName) const 
	{ 
		return find_buffer(aName) != 0; 
	}

	buffer& connection::buffer_from_name(const std::string& aName, bool aCreate, bool aRemote)
	{
		if (aName.empty())
			throw invalid_buffer_name();

		buffer* theBuffer = find_buffer(aName);
		if (theBuffer != 0)
			return *theBuffer;

		if (is_channel(aName))
		{
			if (aCreate)
			{
				string theName = irc::make_string(*this, aName);
				buffer_ptr theBuffer(new channel_buffer(iModel, *this, channel(aName, iServer)));
				channel_buffer_list::iterator newBuffer = iChannelBuffers.insert(std::make_pair(theName, theBuffer)).first;
				return *(*newBuffer).second;
			}
			else
			{
				if (!iServerBuffer.empty())
					return server_buffer();
				throw no_server_buffer();
			}
		}
		else
		{
			if (aCreate)
			{
				irc::user theUser(aName, *this);
				if (has_user(theUser))
					theUser = user(theUser);
				string theName = irc::make_string(*this, theUser.nick_name());
				buffer_ptr theBuffer(new user_buffer(iModel, *this, aRemote ? user_buffer::REMOTE : user_buffer::LOCAL, theUser));
				user_buffer_list::iterator newBuffer = iUserBuffers.insert(std::make_pair(theName, theBuffer)).first;
				return *((*newBuffer).second);
			}
			else
			{
				if (!iServerBuffer.empty())
					return server_buffer();
				throw no_server_buffer();
			}
		}
	}

	channel_buffer& connection::channel_buffer_from_name(const std::string& aName, bool aCreate)
	{
		if (aName.empty())
			throw invalid_buffer_name();

		buffer* existingBuffer = find_buffer(aName);
		if (existingBuffer != 0)
		{
			if (existingBuffer->type() != buffer::CHANNEL)
				throw not_channel_buffer();
			return static_cast<channel_buffer&>(*existingBuffer);
		}

		if (!aCreate)
			throw channel_buffer_not_found();

		string theName = irc::make_string(*this, aName);
		buffer_ptr theBuffer(new channel_buffer(iModel, *this, channel(aName, iServer)));
		channel_buffer_list::iterator newBuffer = iChannelBuffers.insert(std::make_pair(theName, theBuffer)).first;
		return static_cast<channel_buffer&>(*(*newBuffer).second);
	}

	server_buffer& connection::server_buffer(bool aCreate)
	{
		if (iServerBuffer.empty() && aCreate)
		{
			buffer_ptr newBuffer(new irc::server_buffer(iModel, *this, iServer));
			iServerBuffer.push_back(newBuffer);
		}

		if (!iServerBuffer.empty())
			return static_cast<irc::server_buffer&>(*iServerBuffer[0]);
		throw no_server_buffer();
	}

	notice_buffer& connection::notice_buffer(bool aCreate)
	{
		if (iNoticeBuffer.empty() && aCreate)
		{
			buffer* activeBuffer = iConnectionManager.active_buffer();
			buffer_ptr newBuffer(new irc::notice_buffer(iModel, *this, iServer));
			iNoticeBuffer.push_back(newBuffer);
			if (activeBuffer != 0)
				activeBuffer->activate();
		}

		if (!iNoticeBuffer.empty())
			return static_cast<irc::notice_buffer&>(*iNoticeBuffer[0]);
		throw no_notice_buffer();
	}

	void connection::buffers(buffer_list& aBuffers)
	{
		if (!iServerBuffer.empty())
			aBuffers.push_back(&server_buffer());
		for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
			aBuffers.push_back(&*(*i).second);
		for (user_buffer_list::iterator i = iUserBuffers.begin(); i != iUserBuffers.end(); ++i)
			aBuffers.push_back(&*(*i).second);
		if (!iNoticeBuffer.empty())
			aBuffers.push_back(&notice_buffer());
	}

	bool connection::has_user(const irc::user& aUser) const
	{
		for (channel_buffer_list::const_iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
			if ((*i).second->has_user(aUser.nick_name()))
				return true;
		return false;
	}

	bool connection::has_user(const irc::user& aUser, const buffer& aBufferToExclude) const
	{
		for (channel_buffer_list::const_iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
			if (&*(*i).second != &aBufferToExclude && (*i).second->has_user(aUser.nick_name()))
				return true;
		return false;
	}

	const user& connection::user(const irc::user& aUser) const
	{
		for (channel_buffer_list::const_iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
			if ((*i).second->has_user(aUser.nick_name()))
				return (*i).second->user(aUser.nick_name());
		throw std::logic_error("irc::connection::user");
	}


	void connection::remove_buffer(buffer& aBuffer)
	{
		switch(aBuffer.type())
		{
		case buffer::SERVER:
			if (!iServerBuffer.empty() && &aBuffer == &server_buffer())
				erase_object(iServerBuffer, iServerBuffer.begin());
			else
				throw not_server_buffer();
			break;
		case buffer::NOTICE:
			if (!iNoticeBuffer.empty() && &aBuffer == &notice_buffer())
				erase_object(iNoticeBuffer, iNoticeBuffer.begin());
			else
				throw not_notice_buffer();
			break;
		case buffer::CHANNEL:
			for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
				if (&*(*i).second == &aBuffer)
				{
					for (flood_prevention_buffer::iterator i = iFloodPreventionBuffer.begin(); i != iFloodPreventionBuffer.end();)
					{
						if (i->command() == message::PRIVMSG &&
							i->parameters().size() > 0 &&
							irc::make_string(*this, i->parameters()[0]) == aBuffer.name())
								i = iFloodPreventionBuffer.erase(i);
						else
							++i;
					}
					if (iAwayUpdater.iNextChannel == i)
						++iAwayUpdater.iNextChannel;
					erase_object(iChannelBuffers, i);
					break;
				}
			break;
		case buffer::USER:
			for (user_buffer_list::iterator i = iUserBuffers.begin(); i != iUserBuffers.end(); ++i)
				if (&*((*i).second) == &aBuffer)
				{
					for (flood_prevention_buffer::iterator i = iFloodPreventionBuffer.begin(); i != iFloodPreventionBuffer.end();)
					{
						if (i->command() == message::PRIVMSG &&
							i->parameters().size() > 0 &&
							irc::make_string(*this, i->parameters()[0]) == aBuffer.name())
								i = iFloodPreventionBuffer.erase(i);
						else
							++i;
					}
					erase_object(iUserBuffers, i);
					break;
				}
			break;
		}
		check_close();
	}

	void connection::local_address_warning()
	{
		message localMessage(*this, message::INCOMING);
		localMessage.parameters().push_back(iConnectionManager.sNoIpAddress);
		server_buffer(true).new_message(localMessage);
	}

	u_long connection::local_address() const
	{
		if (iHaveLocalAddress)
			return iLocalAddress;
		else if (!iPacketStream.closed())
		{
			try
			{
				return iPacketStream.connection().local_end_point().address().to_v4().to_ulong();
			}
			catch(...)
			{
				return INADDR_NONE;
			}
		}
		else
			return INADDR_NONE;
	}

	void connection::reset_console()
	{
		disconnect();
		reset();
		iConsole = true;
		iServer = server();
		iServer.set_network(sConsolesTitle);
		iServer.set_name(sConsoleTitle);
		iIdentity = iModel.default_identity();
		iNickName = iIdentity.nick_name();
		iUser = irc::user(iIdentity.nick_name(), casemapping::rfc1459, false);
		iPassword.clear();
	}

	bool connection::send_message(const std::string& aMessage)
	{
		if (iPacketStream.closed())
			return false;
		if (aMessage[aMessage.size()-1] != '\n')
			throw bad_message();
		iPacketStream.send_packet(neolib::string_packet(aMessage.data(), aMessage.size()));
		iTimeLastMessageSent = iModel.owner_thread().elapsed_ms();
		return true;
	}

	bool connection::send_message(buffer& aBuffer, const message& aMessage, bool aFromFilter)
	{
		if (!aFromFilter)
		{
			bool filtered = false;
			iConnectionManager.filter_message(*this, aMessage, filtered);
			if (filtered)
				return true;
		}
		if (!connected() && aMessage.command() != message::QUIT)
		{
			aBuffer.notify_observers(buffer_observer::NotifyMessageFailure, aMessage);
			return false;
		}
		bool sendNow = !iConnectionManager.flood_prevention() || (iFloodPreventionBuffer.empty() && iModel.owner_thread().elapsed_ms() - iTimeLastMessageSent > iConnectionManager.flood_prevention_delay());
		if (aMessage.command() == message::QUIT)
		{
			iQuitting = true;
			notify_observers(connection_observer::NotifyConnectionQuitting);
			if (!registered())
			{
				close();
				return true;
			}
			iAwayUpdater.iNextChannel = iChannelBuffers.end();
			erase_objects(iChannelBuffers, iChannelBuffers.begin(), iChannelBuffers.end());
			erase_objects(iUserBuffers, iUserBuffers.begin(), iUserBuffers.end());
			erase_object(iNoticeBuffer, iNoticeBuffer.begin());
			erase_object(iServerBuffer, iServerBuffer.begin());
			sendNow = true;
		}
		else if (aMessage.command() == message::PART)
		{
			sendNow = true;
		}
		notify_observers(connection_observer::NotifyOutgoingMessage, aMessage);
		if (aMessage.command() == message::JOIN)
		{
			if (aMessage.parameters().size() >= 1)
			{
				std::vector<std::string> channels;
				neolib::tokens(aMessage.parameters()[0], std::string(","), channels);
				std::vector<std::string> keys;
				if (aMessage.parameters().size() >= 2)
					neolib::tokens(aMessage.parameters()[1], std::string(","), keys);
				for (std::size_t i = 0; i != channels.size(); ++i)
				{
					std::string& channelName = channels[i];
					if (is_channel(channelName))
					{
						if (i < keys.size())
							iConnectionManager.add_key(*this, channelName, keys[i]);
						else
							iConnectionManager.remove_key(*this, channelName);
						if (iConnectionManager.create_channel_buffer_upfront())
						{
							bool bufferExists = buffer_exists(channelName);
							if (!bufferExists)
								channel_buffer_from_name(channelName).set_ready(false);
							channel_buffer& theBuffer = channel_buffer_from_name(channelName);
							if (!theBuffer.is_ready())
								theBuffer.joining();
							if (iModel.can_activate_buffer(theBuffer) && !bufferExists)
								theBuffer.activate();
						}
					}
				}
			}
		}
		if (sendNow)
		{
			send_message(aMessage.to_string(iModel.message_strings()));
			if (aMessage.command() == message::AWAY)
				iWhoRequester->new_request(iNickName);
		}
		else
			iFloodPreventionBuffer.push_back(aMessage);
		return true;
	}

	void connection::receive_message(const message& aMessage, bool aFromFilter)
	{
		if (!aFromFilter)
		{
			bool filtered = false;
			iConnectionManager.filter_message(*this, aMessage, filtered);
			if (filtered)
				return;
		}
		notify_observers(connection_observer::NotifyIncomingMessage, aMessage);
		irc::user userMessage(aMessage.origin(), *this);
		if (userMessage.has_user_name())
		{
			buffer_list theBuffers;
			buffers(theBuffers);
			for (buffer_list::iterator i = theBuffers.begin(); i != theBuffers.end(); ++i)
			{
				if ((*i)->has_user(userMessage.nick_name()))
				{
					irc::user& ourUser = (*i)->user(userMessage.nick_name());
					if (!ourUser.has_user_name())
					{
						ourUser = userMessage;
						(*i)->user_new_host_info(ourUser);
					}
				}
			}
		}
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
		case message::RPL_ENDOFWHOIS:
			{
				bool display = true;
				if (!aMessage.parameters().empty())
				{
					irc::user theUser(*this);
					theUser.nick_name() = aMessage.parameters()[0];
					display = !(iHostQuery && irc::make_string(*this, iNickName) == theUser.nick_name());
					switch(aMessage.command())
					{
					case message::RPL_WHOISUSER:
						if (aMessage.parameters().size() >= 5)
						{
							theUser.user_name() = aMessage.parameters()[1];
							theUser.host_name() = aMessage.parameters()[2];
							theUser.full_name() = aMessage.parameters()[4];
							if (iHostQuery && irc::make_string(*this, iNickName) == theUser.nick_name())
							{
								iUser = theUser;
								iHostName = iUser.host_name();
								iResolver.resolve(*this, iHostName, neolib::IPv4);
							}
							buffer_list theBuffers;
							buffers(theBuffers);
							for (buffer_list::iterator i = theBuffers.begin(); i != theBuffers.end(); ++i)
							{
								if ((*i)->has_user(theUser.nick_name()))
								{
									irc::user& ourUser = (*i)->user(theUser.nick_name());
									if (!ourUser.has_user_name() || !ourUser.has_full_name())
									{
										ourUser = theUser;
										(*i)->user_new_host_info(ourUser);
									}
								}
							}
						}
						break;
					case message::RPL_ENDOFWHOIS:
						if (iHostQuery && irc::make_string(*this, iNickName) == theUser.nick_name())
							iHostQuery = false;
						break;
					}
				}
				if (display)
				{
					if (!iWhoisRequester->new_message(aMessage))
					{
						switch(aMessage.command())
						{
						case message::RPL_AWAY:
							if (iConnectionManager.active_buffer() != 0 && 
								&iConnectionManager.active_buffer()->connection() == this)
							{
								iConnectionManager.active_buffer()->new_message(aMessage);
								break;
							}
							// fall through
						default:
							server_buffer().new_message(aMessage);
						}
					}
				}
			}
			break;
		case message::RPL_WHOREPLY:
			if (aMessage.parameters().size() >= 7)
			{
				irc::user theUser(*this);
				theUser.nick_name() = aMessage.parameters()[4];
				theUser.user_name() = aMessage.parameters()[1];
				theUser.host_name() = aMessage.parameters()[2];
				theUser.set_away(aMessage.parameters()[5][0] == 'G');
				neolib::vecarray<std::pair<std::string::const_iterator, std::string::const_iterator>, 2> fullNameBits;
				neolib::tokens(aMessage.parameters()[6], std::string(" "), fullNameBits, 2);
				if (fullNameBits.size() == 2)
					theUser.full_name() = std::string(fullNameBits[1].first, aMessage.parameters()[6].end());
				buffer_list theBuffers;
				buffers(theBuffers);
				for (buffer_list::iterator i = theBuffers.begin(); i != theBuffers.end(); ++i)
				{
					if ((*i)->has_user(theUser.nick_name()))
					{
						irc::user& ourUser = (*i)->user(theUser.nick_name());
						if (!ourUser.has_user_name() || (!ourUser.has_full_name() && !theUser.full_name().empty()))
						{
							ourUser = theUser;
							(*i)->user_new_host_info(ourUser);
						}
						if (ourUser.away() != theUser.away())
						{
							ourUser.set_away(theUser.away());
							(*i)->user_away_status_changed(ourUser);
						}
					}
				}
			}
			// fall through...
		case message::RPL_ENDOFWHO:
			if (!iWhoRequester->new_message(aMessage))
				server_buffer().new_message(aMessage);
			break;
		case message::ERR_NOSUCHNICK:
			if (iWhoisRequester->new_message(aMessage))
				break;
			// fall through...
		case message::ERR_CANNOTSENDTOCHAN:
			if (aMessage.parameters().size() >= 1 && buffer_exists(aMessage.parameters()[0]))
				buffer_from_name(aMessage.parameters()[0]).new_message(aMessage);
			else
				server_buffer().new_message(aMessage);
			break;
		case message::ERR_NOSUCHSERVICE:
			if (aMessage.parameters().size() >= 1 && is_channel(aMessage.parameters()[0]) &&
				buffer_exists(aMessage.parameters()[0]))
				buffer_from_name(aMessage.parameters()[0]).new_message(aMessage);
			else
				server_buffer().new_message(aMessage);
			break;
		case message::ERR_NONICKNAMEGIVEN:
		case message::ERR_ERRONEUSNICKNAME:
		case message::ERR_NICKNAMEINUSE:
		case message::ERR_NICKCOLLISION:
		case message::ERR_UNAVAILRESOURCE:
		case message::ERR_RESTRICTED:
			server_buffer().new_message(aMessage);
			if (!iRegistered)
			{
				if (!iAlternateNickNames)
				{
					iAlternateNickNames = iIdentity.alternate_nick_names();
					iAlternateNickNames->insert(iAlternateNickNames->begin(), iIdentity.nick_name());
					for (identity::alternate_nick_names_t::iterator i = iAlternateNickNames->begin(); i != iAlternateNickNames->end(); ++i)
					{
						if (*i == nick_name())
						{
							iAlternateNickNames->erase(i);
							break;
						}
					}
				}
				if (!iAlternateNickNames->empty())
				{
					set_nick_name(*iAlternateNickNames->begin());
					iAlternateNickNames->erase(iAlternateNickNames->begin());
				}
				else
					iConnectionManager.query_nickname(*this);
			}
			break;
		case message::RPL_WELCOME:
		case message::RPL_YOURESERVICE:
			iNickName = irc::user(aMessage.target(), *this).nick_name();
			iUser.nick_name() = iNickName;
			if (!iRegistered)
			{
				iAlternateNickNames.reset();
				iRegistered = true;
				server_buffer().set_ready(true);
				for (user_buffer_list::iterator i = iUserBuffers.begin(); i != iUserBuffers.end(); ++i)
				{
					(*(*i).second).set_ready(true);
					if (iPreviouslyRegistered)
					{
						message reconnected(*this, message::INCOMING);
						reconnected.parameters().push_back(iConnectionManager.reconnected_message(iServer));
						(*(*i).second).new_message(reconnected);
					}
				}
				if (!iNoticeBuffer.empty())
					notice_buffer().set_ready(true);
				std::vector<message> rejoins;
				for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
				{
					channel_buffer& theBuffer = static_cast<channel_buffer&>(*(*i).second);
					if (iPreviouslyRegistered)
					{
						message reconnected(*this, message::INCOMING);
						reconnected.parameters().push_back(iConnectionManager.reconnected_message(iServer));
						theBuffer.new_message(reconnected);
					}
					message joinMessage(*this, message::OUTGOING);
					joinMessage.set_command(message::JOIN);
					joinMessage.parameters().push_back(theBuffer.name());
					if (iConnectionManager.has_key(*this, theBuffer.name()))
						joinMessage.parameters().push_back(iConnectionManager.key(*this, theBuffer.name()));
					rejoins.push_back(joinMessage);
				}
				query_host();
				notify_observers(connection_observer::NotifyConnectionRegistered);
				for (std::vector<message>::const_iterator i = rejoins.begin(); i != rejoins.end(); ++i)
					send_message(*i);
				iPreviouslyRegistered = true;
			}
			server_buffer().new_message(aMessage);
			break;
		case message::RPL_ISUPPORT:
			for (message::parameters_t::const_iterator i = aMessage.parameters().begin(); i != aMessage.parameters().end(); ++i)
			{
				neolib::vecarray<neolib::ci_string, 2> param;
				neolib::tokens(neolib::make_ci_string(*i), neolib::ci_string("="), param, 2);
				if (param.size() == 2 && param[0] == "CASEMAPPING")
				{
					if (param[1] == "ascii")
						iCasemapping = casemapping::ascii;
					else if (param[1] == "rfc1459")
						iCasemapping = casemapping::rfc1459;
					else if (param[1] == "strict-rfc1459")
						iCasemapping = casemapping::strict_rfc1459;
					iUser.casemapping() = iCasemapping;
				}
				if (param.size() == 2 && param[0] == "PREFIX")
				{
					iPrefixes.first = "";
					iPrefixes.second = "";
					neolib::vecarray<std::string, 2> bits;
					neolib::tokens(neolib::make_string(param[1]), std::string("()"), bits, 2);
					if (bits.size() == 2)
					{
						iPrefixes.first = bits[0];
						iPrefixes.second = bits[1];
					}
				}
				if (param.size() == 2 && param[0] == "CHANTYPES")
				{
					iChantypes = neolib::make_string(param[1]);
					buffer_list theBuffers;
					buffers(theBuffers);
					for (buffer_list::iterator i = theBuffers.begin(); i != theBuffers.end(); ++i)
						(*i)->update_chantype_messages();
				}
			}
			server_buffer().new_message(aMessage);
			break;
		case message::INVITE:
			if (!iConnectionManager.ignore_list().ignored(*this, iServer, irc::user(aMessage.origin(), *this)))
			{
				server_buffer().new_message(aMessage);
			}
			break;
		case message::NOTICE:
			if (!iConnectionManager.ignore_list().ignored(*this, iServer, irc::user(aMessage.origin(), *this)))
			{
				if (iConnectionManager.use_notice_buffer())
				{
					notice_buffer().new_message(aMessage);
				}
				else if (!aMessage.parameters().empty() && is_channel(aMessage.parameters()[0]) &&
					buffer_exists(aMessage.parameters()[0]))
				{
					buffer_from_name(aMessage.parameters()[0]).new_message(aMessage);
				}
				else if (iConnectionManager.active_buffer() != 0 &&
					&iConnectionManager.active_buffer()->connection() == this)
				{
					iConnectionManager.active_buffer()->new_message(aMessage);
				}
				else
				{
					if (server_buffer().just_weak_observers())
					{
						buffer_list theBuffers;
						buffers(theBuffers);
						for (buffer_list::iterator i = theBuffers.begin(); i != theBuffers.end(); ++i)
							(*i)->new_message(aMessage);
					}
					else
						server_buffer().new_message(aMessage);
				}
			}
			break;
		case message::PRIVMSG:
			if (!iConnectionManager.ignore_list().ignored(*this, iServer, irc::user(aMessage.origin(), *this)))
			{
				if (neolib::to_upper(aMessage.content()) == "\001VERSION\001")
				{
					server_buffer().new_message(aMessage);
					message response(*this, message::OUTGOING);
					response.set_command(message::NOTICE);
					response.parameters().push_back(irc::user(aMessage.origin(), *this).nick_name());
					std::string version = "\001VERSION %CN%:%CV%:%CE%\001";
					std::string::size_type i = version.find("%CN%");
					if (i != std::string::npos)
						version.replace(i, 4, connection_manager::sClientName);
					i = version.find("%CV%");
					if (i != std::string::npos)
						version.replace(i, 4, connection_manager::sClientVersion);
					i = version.find("%CE%");
					if (i != std::string::npos)
						version.replace(i, 4, connection_manager::sClientEnvironment);
					response.parameters().push_back(version);
					send_message(response);
					return;
				}
				else if (neolib::to_upper(aMessage.content()) == "\001FINGER\001")
				{
					server_buffer().new_message(aMessage);
					/* TODO */
					return;
				}
				else if (neolib::to_upper(aMessage.content()) == "\001SOURCE\001")
				{
					server_buffer().new_message(aMessage);
					/* TODO */
					return;
				}
				else if (neolib::to_upper(aMessage.content()) == "\001USERINFO\001")
				{
					server_buffer().new_message(aMessage);
					/* TODO */
					return;
				}
				else if (neolib::to_upper(aMessage.content()) == "\001CLIENTINFO\001")
				{
					server_buffer().new_message(aMessage);
					message response(*this, message::OUTGOING);
					response.set_command(message::NOTICE);
					response.parameters().push_back(irc::user(aMessage.origin(), *this).nick_name());
					response.parameters().push_back("\001CLIENTINFO VERSION TIME PING CLIENTINFO\001");
					send_message(response);
					return;
				}
				else if (neolib::to_upper(aMessage.content()).find("\001PING ") == 0)
				{
					server_buffer().new_message(aMessage);
					message response(*this, message::OUTGOING);
					response.set_command(message::NOTICE);
					response.parameters() = aMessage.parameters();
					response.parameters()[0] = irc::user(aMessage.origin(), *this).nick_name();
					send_message(response);
					return;
				}
				else if (neolib::to_upper(aMessage.content()).find("\001ERRMSG ") == 0)
				{
					server_buffer().new_message(aMessage);
					/* TODO */
					return;
				}
				else if (neolib::to_upper(aMessage.content()) == "\001TIME\001")
				{
					server_buffer().new_message(aMessage);
					message response(*this, message::OUTGOING);
					response.set_command(message::NOTICE);
					response.parameters().push_back(irc::user(aMessage.origin(), *this).nick_name());
					std::string result = "\001TIME %T%\001";
					std::string::size_type i = result.find("%T%");
					if (i != std::string::npos)
					{
						std::tm *tmNow;
						std::time_t timeNow;
						std::time(&timeNow);
						tmNow = std::localtime(&timeNow);
						std::string strNow(asctime(tmNow));
						if (strNow[strNow.size() - 1] == '\n')
							strNow.erase(strNow.end() - 1);
						result.replace(i, 3, strNow);
					}
					response.parameters().push_back(result);
					send_message(response);
					return;
				}
				else if (neolib::to_upper(aMessage.content()).find("\001DCC") == 0)
				{
					server_buffer().new_message(aMessage);
					// Handled elsewhere
					return;
				}
				else if (aMessage.is_ctcp())
				{
					server_buffer().new_message(aMessage);
					return;
				}
				else if (!aMessage.parameters().empty())
				{
					const std::string& target = aMessage.parameters()[0];
					if (is_channel(target))
					{
						if (buffer_exists(target))
							buffer_from_name(target).new_message(aMessage);
					}
					else
					{
						buffer* currentActive = iConnectionManager.active_buffer();
						buffer_from_name(aMessage.origin(), true, true);
						if (currentActive != 0)
							currentActive->activate();
						buffer_from_name(aMessage.origin(), true, true).new_message(aMessage);
					}
				}
			}
			break;
		case message::PING:
			{
				if (iShowPings)
					server_buffer().new_message(aMessage);
				message response(*this, message::OUTGOING);
				response.set_command(message::PONG);
				response.parameters() = aMessage.parameters();
				if (response.parameters().size() == 2)
					std::swap(response.parameters()[0], response.parameters()[1]);
				send_message(response);
			}
			break;
		case message::PONG:
			if (iShowPings)
				server_buffer().new_message(aMessage);
			if (iPingSent_ms != 0ull)
			{
				iLatency_ms = neolib::thread::elapsed_ms() - iPingSent_ms;
				iPingSent_ms = 0ull;
			}
			break;
		case message::TOPIC:
		case message::RPL_TOPIC:
		case message::RPL_TOPICAUTHOR:
			if (!aMessage.parameters().empty())
			{
				const std::string& theChannel = aMessage.parameters()[0];
				if (buffer_exists(theChannel))
					buffer_from_name(theChannel).new_message(aMessage);
			}
			break;
		case message::RPL_NAMREPLY:
		case message::RPL_ENDOFNAMES:
			if (!aMessage.parameters().empty())
			{
				std::string channel;
				if (aMessage.parameters()[0][0] == '=' || 
					aMessage.parameters()[0][0] == '*' ||
					aMessage.parameters()[0][0] == '@')
				{
					if (aMessage.parameters()[0].size() == 1)
					{
						if (aMessage.parameters().size() > 1)
							channel = aMessage.parameters()[1];
					}
					else
						channel = aMessage.parameters()[0].substr(1);
				}
				else
					channel = aMessage.parameters()[0];
				if (!channel.empty())
				{
					buffer_from_name(channel, false).new_message(aMessage);
					if ((iConnectionManager.auto_who() || iConnectionManager.away_update()) && 
						aMessage.command() == message::RPL_ENDOFNAMES)
						iWhoRequester->new_request(channel);
				}
			}
			break;
		case message::QUIT:
			for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
				if ((*i).second->has_user(aMessage.origin()))
					(*i).second->new_message(aMessage);
			if (buffer_exists(aMessage.origin()))
				buffer_from_name(aMessage.origin()).new_message(aMessage);
			if (irc::make_string(*this, irc::user(aMessage.origin(), *this).nick_name()) == nick_name())
				close();
			break;
		case message::NICK:
			{
				irc::user oldUser(aMessage.origin(), *this);
				irc::user newUser(oldUser);
				if (!aMessage.parameters().empty())
					newUser.nick_name() = aMessage.parameters()[0];
				iConnectionManager.ignore_list().update_user(*this, iServer.key(), oldUser, newUser);
				iConnectionManager.notify_list().update_user(*this, iServer.key(), oldUser, newUser);
				iConnectionManager.auto_mode_list().update_user(*this, iServer.key(), oldUser, newUser);
				if (oldUser.nick_name() == irc::make_string(*this, iNickName))
				{
					broadcast(aMessage);
					iNickName = newUser.nick_name();
					iUser.nick_name() = iNickName;
				}
				else
				{
					for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
						if ((*i).second->has_user(aMessage.origin()))
							(*i).second->new_message(aMessage);
					if (buffer_exists(aMessage.origin()))
						buffer_from_name(aMessage.origin()).new_message(aMessage);
				}
				if (buffer_exists(aMessage.origin()))
				{
					user_buffer* userBuffer = &static_cast<user_buffer&>(buffer_from_name(aMessage.origin()));
					user_buffer_list::iterator i;
					for (i = iUserBuffers.begin(); i != iUserBuffers.end(); ++i)
						if (&*((*i).second) == userBuffer)
							break;
					if (i != iUserBuffers.end())
					{
						string newName = irc::make_string(*this, newUser.nick_name());
						buffer_ptr tmp(i->second);
						iUserBuffers.insert(std::make_pair(newName, i->second));
						erase_object(iUserBuffers, i);
					}
				}
			}
			break;
		case message::JOIN:
			{
				std::string target;
				for (message::parameters_t::const_iterator i = aMessage.parameters().begin(); i != aMessage.parameters().end(); ++i)
					if (is_channel(*i))
					{
						target = *i;
						break;
					}
				if (!target.empty())
				{
					bool selfJoin = (irc::make_string(*this, irc::user(aMessage.origin(), *this).nick_name()) == nick_name());
					if (buffer_exists(target) || selfJoin)
					{
						if (selfJoin && !buffer_exists(target))
						{
							buffer* activeBuffer = iModel.connection_manager().active_buffer();
							if (activeBuffer != 0 && (activeBuffer->type() != buffer::CHANNEL && activeBuffer->type() != buffer::USER))
							{
								if (iModel.can_activate_buffer(buffer_from_name(target)))
									buffer_from_name(target).activate();
							}
						}
						buffer_from_name(target).set_ready(true);
						buffer_from_name(target).new_message(aMessage);
						if ((!selfJoin && iConnectionManager.auto_who()) || iConnectionManager.away_update())
							iWhoRequester->new_request(static_cast<channel_buffer&>(buffer_from_name(target)), 
								irc::user(aMessage.origin(), *this).nick_name());
					}
				}
			}
			break;
		case message::ERR_NOSUCHCHANNEL:
		case message::ERR_CHANNELISFULL:
		case message::ERR_INVITEONLYCHAN:
		case message::ERR_BANNEDFROMCHAN:
		case message::ERR_BADCHANNELKEY:
			server_buffer().new_message(aMessage);
			if (aMessage.parameters().size() >= 1 && is_channel(aMessage.parameters()[0]) &&
				buffer_exists(aMessage.parameters()[0]))
				remove_buffer(buffer_from_name(aMessage.parameters()[0]));			
			break;
		case message::ERR_NOCHANMODES:
			server_buffer().new_message(aMessage);
			if (aMessage.parameters().size() >= 1 && is_channel(aMessage.parameters()[0]) &&
				buffer_exists(aMessage.parameters()[0]) && !buffer_from_name(aMessage.parameters()[0]).is_ready())
				remove_buffer(buffer_from_name(aMessage.parameters()[0]));			
			break;
		case message::PART:
			{
				std::string target;
				for (message::parameters_t::const_iterator i = aMessage.parameters().begin(); i != aMessage.parameters().end(); ++i)
					if (is_channel(*i))
					{
						target = *i;
						break;
					}
				if (!target.empty())
				{
					irc::user theUser(aMessage.origin(), *this);
					string theNickName = irc::make_string(*this, theUser.nick_name());
					if (theNickName == nick_name())
					{
						if (buffer_exists(target))
							remove_buffer(buffer_from_name(target));
					}
					else if (buffer_exists(target))
						buffer_from_name(target).new_message(aMessage);
				}
			}
			break;
		case message::KICK:
			if (aMessage.parameters().size() >= 2)
			{
				std::string theChannel = aMessage.parameters()[0];
				std::string theUser = aMessage.parameters()[1];
				bool self = irc::make_string(*this, irc::user(theUser, *this).nick_name()) == nick_name();
				if (buffer_exists(theChannel))
				{
					if (self)
					{
						server_buffer().new_message(aMessage);
						if (!iConnectionManager.auto_rejoin_on_kick())
							remove_buffer(buffer_from_name(theChannel));
						else
						{
							channel_buffer& theBuffer = channel_buffer_from_name(theChannel);
							theBuffer.new_message(aMessage);
							message rejoinMessage(*this, message::OUTGOING);
							rejoinMessage.set_command(message::JOIN);	
							rejoinMessage.parameters().push_back(theChannel);
							if (iConnectionManager.has_key(*this, theChannel))
								rejoinMessage.parameters().push_back(iConnectionManager.key(*this, theChannel));
							send_message(rejoinMessage);
						}
					}
					else
						buffer_from_name(theChannel).new_message(aMessage);
				}
			}
			else
				server_buffer().new_message(aMessage);
			break;
		case message::MODE:
			if (aMessage.parameters().size() <= 1)
				break;
			if (is_channel(aMessage.parameters()[0]))
			{
				if (buffer_exists(aMessage.parameters()[0]))
					buffer_from_name(aMessage.parameters()[0]).new_message(aMessage);
			}
			else
			{
				for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
					if ((*i).second->has_user(aMessage.parameters()[0]))
						(*i).second->new_message(aMessage);
				if (buffer_exists(aMessage.parameters()[0]))
					buffer_from_name(aMessage.parameters()[0]).new_message(aMessage);
			}
			break;
		case message::RPL_LISTSTART:
		case message::RPL_LIST:
		case message::RPL_LISTEND:
		case message::RPL_BANLIST:
		case message::RPL_ENDOFBANLIST:
		case message::RPL_EXCEPTLIST:
		case message::RPL_ENDOFEXCEPTLIST:
		case message::RPL_INVITELIST:
		case message::RPL_ENDOFINVITELIST:
			// Handled elsewhere
			break;
		case message::RPL_CHANNELMODEIS:
		case message::RPL_CREATIONTIME:
			if (is_channel(aMessage.parameters()[0]) &&
				buffer_exists(aMessage.parameters()[0]))
					buffer_from_name(aMessage.parameters()[0]).new_message(aMessage);
			break;
		default:
			server_buffer().new_message(aMessage);
			break;
		}
	}

	model::id connection::next_buffer_id()
	{
		return iConnectionManager.next_buffer_id();
	}

	model::id connection::next_message_id()
	{
		return iConnectionManager.next_message_id();
	}

	void connection::broadcast(const message& aMessage)
	{
		server_buffer().new_message(aMessage);
		if (!iNoticeBuffer.empty())
			notice_buffer().new_message(aMessage);
		for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
			(*i).second->new_message(aMessage);
		for (user_buffer_list::iterator i = iUserBuffers.begin(); i != iUserBuffers.end(); ++i)
			(*i).second->new_message(aMessage);
	}

	void connection::set_nick_name(const std::string& aNickName, bool aSetLocal)
	{
		if (aSetLocal)
		{
			iNickName = aNickName;
			iUser.nick_name() = iNickName;
		}
		message regMessage(*this, message::OUTGOING);
		regMessage.set_command(message::NICK);
		regMessage.parameters().clear();
		regMessage.parameters().push_back(aNickName);
		send_message(regMessage);
	}

	bool connection::is_prefix(char aPrefix) const
	{
		return iPrefixes.second.find(aPrefix) != std::string::npos;
	}

	bool connection::is_prefix_mode(char aMode) const
	{
		return iPrefixes.first.find(aMode) != std::string::npos;
	}

	char connection::mode_from_prefix(char aPrefix) const
	{
		std::string::size_type p = iPrefixes.second.find(aPrefix);
		if (p == std::string::npos)
			throw mode_prefix_not_found();
		if (p < iPrefixes.first.size())
			return iPrefixes.first[p];
		return 0;		
	}

	char connection::best_prefix(const std::string& aModes) const
	{
		std::string::size_type best = std::string::npos;
		char bestPrefix = '?';
		for (std::string::size_type i = 0; i != aModes.size(); ++i)
		{
			std::string::size_type j = iPrefixes.first.find(aModes[i]);
			if (best == std::string::npos || j < best)
			{
				best = j;
				if (best < iPrefixes.second.size())
					bestPrefix = iPrefixes.second[best];
			}
		}
		return bestPrefix;
	}

	unsigned int connection::mode_compare_value(const std::string& aModes) const
	{
		std::string::size_type best = std::string::npos;
		for (std::string::size_type i = 0; i != aModes.size(); ++i)
		{
			std::string::size_type j = iPrefixes.first.find(aModes[i]);
			if (j != std::string::npos && (best == std::string::npos || j < best))
				best = j;
		}
		return best != std::string::npos ? static_cast<unsigned int>(best) : 0xFFFFFFFF;
	}

	bool connection::add_command_timer(const std::string& aName, double aInterval, const neolib::optional<std::size_t>& aRepeat, const std::string& aCommand)
	{
		command_timer_list::iterator theTimer = iCommandTimers.find(aName);
		bool addedNotReplaced = true;
		if (theTimer == iCommandTimers.end())	
			theTimer = iCommandTimers.insert(std::make_pair(aName, command_timer(*this, aInterval, aRepeat, aCommand))).first;
		else
		{
			theTimer->second = command_timer(*this, aInterval, aRepeat, aCommand);
			addedNotReplaced = false;
		}
		return addedNotReplaced;
	}

	bool connection::delete_command_timer(const std::string& aName)
	{
		command_timer_list::iterator i = iCommandTimers.find(aName);
		if (i != iCommandTimers.end())	
		{
			iCommandTimers.erase(i);
			return true;
		}
		else
			return false;
	}

	void connection::add_observer(connection_observer& aObserver)
	{
		neolib::observable<connection_observer>::add_observer(aObserver);
		if (!iServerBuffer.empty())
			neolib::observable<connection_observer>::notify_observer(aObserver, connection_observer::NotifyBufferAdded, server_buffer());
		if (!iNoticeBuffer.empty())
			neolib::observable<connection_observer>::notify_observer(aObserver, connection_observer::NotifyBufferAdded, notice_buffer());
		for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end();)
			neolib::observable<connection_observer>::notify_observer(aObserver, connection_observer::NotifyBufferAdded, *(*i++).second);
		for (user_buffer_list::iterator i = iUserBuffers.begin(); i != iUserBuffers.end();)
			neolib::observable<connection_observer>::notify_observer(aObserver, connection_observer::NotifyBufferAdded, *(*i++).second);
	}

	void connection::send_ping()
	{
		if (!connected() || !registered())
			return;
		iWaitingForPong = true;
		message newMessage(*this, message::OUTGOING);
		newMessage.set_command(message::PING);
		newMessage.set_origin("");
		newMessage.parameters().push_back(neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(time(NULL))));
		send_message(newMessage);
		iPingSent_ms = neolib::thread::elapsed_ms();
	}

	void connection::timeout()
	{
		message connected(*this, message::INCOMING);
		connected.parameters().push_back(iConnectionManager.timed_out_message(iServer));
		server_buffer(true).new_message(connected);
		iWaitingForPong = false;
		iPingSent_ms = 0ull;
		iLatency_ms.reset();
		if (!iPacketStream.closed())
			iPacketStream.close();
	}

	void connection::bump_flood_buffer()
	{
		while(!iFloodPreventionBuffer.empty())
		{
			message nextMessage = iFloodPreventionBuffer.front();
			iFloodPreventionBuffer.pop_front();
			send_message(nextMessage.to_string(iModel.message_strings()));
			if (nextMessage.command() == message::AWAY)
				iWhoRequester->new_request(iNickName);
			if (iConnectionManager.flood_prevention())
				break; // if flood prevention has not been disabled we don't need to bump until empty
		} 
	}

	bool connection::any_buffers() const
	{
		return !iServerBuffer.empty() || !iChannelBuffers.empty() || !iUserBuffers.empty();
	}

	bool connection::find_message(model::id aMessageId, buffer*& aBuffer, message*& aMessage)
	{
		if (!iServerBuffer.empty())
			if (server_buffer().find_message(aMessageId, aBuffer, aMessage))
				return true;
		if (!iNoticeBuffer.empty())
			if (notice_buffer().find_message(aMessageId, aBuffer, aMessage))
				return true;
		for (channel_buffer_list::iterator i = iChannelBuffers.begin(); i != iChannelBuffers.end(); ++i)
			if ((*i).second->find_message(aMessageId, aBuffer, aMessage))
				return true;
		for (user_buffer_list::iterator i = iUserBuffers.begin(); i != iUserBuffers.end(); ++i)
			if ((*i).second->find_message(aMessageId, aBuffer, aMessage))
				return true;
		return false;
	}

	void connection::query_host()
	{
		if (iHostName.empty())
		{
			iHostQuery = true;
			message requestMessage(*this, message::OUTGOING);
			requestMessage.set_command(message::WHOIS);
			requestMessage.parameters().push_back(iNickName);
			send_message(requestMessage);
		}
	}

	void connection::notify_observer(connection_observer& aObserver, connection_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case connection_observer::NotifyConnectionConnecting:
			aObserver.connection_connecting(*this);
			break;
		case connection_observer::NotifyConnectionRegistered:
			aObserver.connection_registered(*this);
			break;
		case connection_observer::NotifyBufferAdded:
			aObserver.buffer_added(*const_cast<buffer*>(static_cast<const buffer*>(aParameter)));
			break;
		case connection_observer::NotifyBufferRemoved:
			aObserver.buffer_removed(*const_cast<buffer*>(static_cast<const buffer*>(aParameter)));
			break;
		case connection_observer::NotifyIncomingMessage:
			aObserver.incoming_message(*this, *const_cast<message*>(static_cast<const message*>(aParameter)));
			break;
		case connection_observer::NotifyOutgoingMessage:
			aObserver.outgoing_message(*this, *const_cast<message*>(static_cast<const message*>(aParameter)));
			break;
		case connection_observer::NotifyConnectionQuitting:
			aObserver.connection_quitting(*this);
			break;
		case connection_observer::NotifyConnectionDisconnected:
			aObserver.connection_disconnected(*this);
			break;
		case connection_observer::NotifyConnectionGiveup:
			aObserver.connection_giveup(*this);
			break;
		}
	}

	void connection::object_created(buffer& aObject)
	{
		if (aObject.type() == buffer::SERVER)
			iBackAfterYield = std::shared_ptr<back_after_yield>(new back_after_yield(*this, iModel.owner_thread()));
		neolib::manager_of<connection, connection_observer, buffer>::object_created(aObject);
	}

	void connection::object_destroyed(buffer& aObject)
	{
		if (aObject.type() == buffer::SERVER)
			iBackAfterYield.reset();
		neolib::manager_of<connection, connection_observer, buffer>::object_destroyed(aObject);
	}

	void connection::connection_established(neolib::tcp_string_packet_stream& aStream)
	{
		iGotConnection = true;
		iReconnectData.reset();

		message connected(*this, message::INCOMING);
		connected.parameters().push_back(iConnectionManager.connected_message(iServer));
		server_buffer(true).new_message(connected);

		if (!iPassword.empty())
		{
			message passMessage(*this, message::OUTGOING);
			passMessage.set_command(message::PASS);
			passMessage.parameters().push_back(iPassword);
			send_message(passMessage);
		}

		message nickMessage(*this, message::OUTGOING);
		nickMessage.set_command(message::NICK);
		nickMessage.parameters().clear();
		nickMessage.parameters().push_back(nick_name());
		send_message(nickMessage);

		message userMessage(*this, message::OUTGOING);
		userMessage.set_command(message::USER);
		std::string userName = iIdentity.email_address();
		std::string::size_type at = userName.find('@');
		if (at != std::string::npos)
			userName = userName.substr(0, at);
		if (userName.empty())
			userName = nick_name();
		userMessage.parameters().clear();
		userMessage.parameters().push_back(userName);
		userMessage.parameters().push_back(iIdentity.invisible() ? "8" : "0");
		userMessage.parameters().push_back("*");
		userMessage.parameters().push_back(iIdentity.full_name());
		if (userMessage.parameters().back().empty())
			userMessage.parameters().back() = nick_name();
		send_message(userMessage);
	}

	void connection::connection_failure(neolib::tcp_string_packet_stream& aStream, const boost::system::error_code& aError)
	{
		connection_closed(aStream);
	}

	void connection::packet_sent(neolib::tcp_string_packet_stream& aStream, const neolib::string_packet& aPacket)
	{
		/* do nothing */
	}

	void connection::packet_arrived(neolib::tcp_string_packet_stream& aStream, const neolib::string_packet& aPacket)
	{
		if (iQuitting)
			return;

		iWaitingForPong = false;

		iMessageBuffer.append(static_cast<const char*>(aPacket.data()), aPacket.length());
		iMessageBuffer.append("\r\n", 2);

		handle_data();
	}

	void connection::transfer_failure(neolib::tcp_string_packet_stream& aStream, const boost::system::error_code& aError)
	{
		/* do nothing */
	}

	void connection::connection_closed(neolib::tcp_string_packet_stream& aStream)
	{
		handle_disconnection();
		reset();

		if (iQuitting)
		{
			close();
			return;
		}

		if (iChangingServer)
			return;

		bool giveup = true;
		if (!iClosing && iConnectionManager.auto_reconnect())
		{
			giveup = !reconnect();
		}
		if (giveup)
		{
			notify_observers(connection_observer::NotifyConnectionGiveup);
			iAwayUpdater.iNextChannel = iChannelBuffers.end();
			erase_objects(iChannelBuffers, iChannelBuffers.begin(), iChannelBuffers.end());
			erase_objects(iUserBuffers, iUserBuffers.begin(), iUserBuffers.end());
			erase_object(iNoticeBuffer, iNoticeBuffer.begin());
		}
	}

	void connection::disconnect_timeout_changed()
	{
		iPinger.set_duration(iConnectionManager.disconnect_timeout() / 2 * 1000, true);
		iPinger.reset();
	}

	void connection::retry_network_delay_changed()
	{
		unsigned long delay = iConnectionManager.retry_network_delay() * 1000;
		iRetryNetworkWaiter.set_duration(delay, true);
		iRetryNetworkWaiter.reset();
		if (delay == 0)
		{
			if (iRetryNetworkWaiter.enabled())
				reconnect();
		}
		else
		{
			if (iRetryNetworkWaiter.enabled())
			{
				message reason(*this, message::INCOMING);
				reason.parameters().push_back(iConnectionManager.waiting_message());
				if (!iServerBuffer.empty())
					server_buffer().new_message(reason);
			}
		}
	}

	void connection::ignore_added(const ignore_entry& aEntry)
	{
		if (iServer == aEntry.server())
		{
			message newMessage(*this, message::INCOMING);
			newMessage.set_command("");
			newMessage.parameters().push_back(sIgnoreAddedMessage);
			std::string::size_type i = newMessage.parameters().back().find("%U%");
			if (i != std::string::npos)
				newMessage.parameters().back().replace(i, 3, aEntry.user().ignore_mask());
			server_buffer().new_message(newMessage);
		}
	}

	void connection::ignore_updated(const ignore_entry& aEntry)
	{
		// do nothing and stay silent (we are ignoring after all)
	}

	void connection::ignore_removed(const ignore_entry& aEntry)
	{
		if (iServer == aEntry.server())
		{
			message newMessage(*this, message::INCOMING);
			newMessage.set_command("");
			newMessage.parameters().push_back(sIgnoreRemovedMessage);
			std::string::size_type i = newMessage.parameters().back().find("%U%");
			if (i != std::string::npos)
				newMessage.parameters().back().replace(i, 3, aEntry.user().ignore_mask());
			server_buffer().new_message(newMessage);
		}
	}

	void connection::host_resolved(const std::string& aHostName, neolib::tcp_resolver::iterator aHost)
	{
		if (iHostName == aHostName)
		{
			u_long address = INADDR_NONE;
			try
			{
				address = aHost->endpoint().address().to_v4().to_ulong();
			}
			catch(...)
			{
				// do nothing if IPv6.
			}
			iHaveLocalAddress = (address != INADDR_NONE);
			iLocalAddress = address;
		}
	}

	void connection::host_not_resolved(const std::string& aHostName, const boost::system::error_code& aError)
	{
	}

	void connection::away_updater::ready()
	{
		reset();
		if (iNextChannel == iParent.iChannelBuffers.end())
			iNextChannel = iParent.iChannelBuffers.begin();
		if (iNextChannel == iParent.iChannelBuffers.end())
			return;
		if (iParent.connection_manager().away_update())
		{
			if (iNextChannel->second->is_ready())
				iParent.iWhoRequester->new_request(iNextChannel->second->name());
			++iNextChannel;
		}
	}
}