// dcc_connection.cpp
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
#include <neoirc/client/dcc_connection.hpp>
#include <neoirc/client/dcc_connection_manager.hpp>
#include <neoirc/client/model.hpp>

namespace irc
{
	dcc_connection::dcc_connection(type_e aType, const std::string& aName, const user& aLocalUser, const user& aRemoteUser, irc::dcc_connection_manager& aConnectionManager, neolib::io_task& aIoTask) : 
		neolib::timer(aIoTask, 5*60*1000, false), iType(aType), iName(aName), iLocalUser(aLocalUser), iRemoteUser(aRemoteUser), iId(aConnectionManager.next_connection_id()),
		iConnectionManager(aConnectionManager), 
		iClosing(false), iReady(false)
	{
	}

	dcc_connection::~dcc_connection()
	{
		iClosing = true;
		disconnect();
		deactivate();
		notify_observers(dcc_connection_observer::NotifyClosing);
	}

	model& dcc_connection::model()
	{
		return iConnectionManager.model();
	}

	const model& dcc_connection::model() const
	{
		return iConnectionManager.model();
	}

	bool dcc_connection::connect(u_long aAddress, u_short aPort)
	{
		std::string address = 
			neolib::unsigned_integer_to_string<char>((aAddress >> 24) % 0xFF) + "." + 
			neolib::unsigned_integer_to_string<char>((aAddress >> 16) % 0xFF) + "." + 
			neolib::unsigned_integer_to_string<char>((aAddress >> 8) % 0xFF) + "." + 
			neolib::unsigned_integer_to_string<char>(aAddress % 0xFF);
		iStream = dcc_stream::pointer(new dcc_stream(iConnectionManager.model().io_task(), address, aPort));

		if (type() == CHAT)
		{
			dcc_chat_connection& theChat = static_cast<dcc_chat_connection&>(*this);
			if (theChat.origin() == dcc_chat_connection::Remote)
			{
				dcc_message connecting(theChat, dcc_message::INCOMING, dcc_message::STATUS);
				connecting.content() = iConnectionManager.connecting_to_message();
				theChat.new_message(connecting);
			}
		}

		return true;
	}

	bool dcc_connection::listen(int aPort)
	{
		iStreamServer = stream_server_pointer(new dcc_stream_server(iConnectionManager.model().io_task(), aPort));
		neolib::timer::reset();
		return true;
	}

	void dcc_connection::set_stream(dcc_stream::pointer aStream)
	{
		iStream = std::move(aStream);
		iStream->add_observer(*this);
	}

	bool dcc_connection::has_stream_server() const
	{
		return iStreamServer.get() != 0;
	}

	const dcc_stream_server& dcc_connection::stream_server() const
	{
		if (!has_stream_server())
			throw no_stream_server();
		return *iStreamServer;
	}

	dcc_stream_server& dcc_connection::stream_server()
	{
		if (!has_stream_server())
			throw no_stream_server();
		return *iStreamServer;
	}

	bool dcc_connection::has_stream() const
	{
		return iStream.get() != 0;
	}

	const dcc_stream& dcc_connection::stream() const
	{
		if (!has_stream())
			throw no_stream();
		return *iStream;
	}

	dcc_stream& dcc_connection::stream()
	{
		if (!has_stream())
			throw no_stream();
		return *iStream;
	}

	bool dcc_connection::close()
	{
		if (iClosing)
			return true;
		iConnectionManager.remove_dcc_connection(*this);
		return true;
	}

	void dcc_connection::set_ready(bool aReady)
	{
		if (iReady == aReady)
			return;
		iReady = aReady;
		notify_observers(dcc_connection_observer::NotifyReadyChange);
	}

	void dcc_connection::disconnect()
	{
		handle_disconnection();
	}

	void dcc_connection::handle_disconnection()
	{
		notify_observers(dcc_connection_observer::NotifyConnectionDisconnected);

		if (iType == CHAT && !iClosing)
		{
			dcc_chat_connection& theChat = static_cast<dcc_chat_connection&>(*this);
			if (theChat.origin() != dcc_chat_connection::Listen)
			{
				dcc_message reason(theChat, dcc_message::INCOMING, dcc_message::STATUS);
				if (iError.value() != 0)
					reason.content() = iConnectionManager.model().error_message(iError.value(), iError.message());
				else
					reason.content() = iConnectionManager.disconnected_message();
				theChat.new_message(reason);
			}
		}

		set_ready(false);

		if (iType == SEND)
			close();
	}

	void dcc_connection::activate()
	{
		if (iClosing)
			return;
		if (iConnectionManager.active_connection() == this)
			return;
		iConnectionManager.set_active_connection(this);
		notify_observers(dcc_connection_observer::NotifyActivate);
	}

	void dcc_connection::deactivate()
	{
		if (iConnectionManager.active_connection() != this)
			return;
		iConnectionManager.set_active_connection(0);
	}

	void dcc_connection::reopen()
	{
		iConnectionManager.model().reopen_connection(*this);
	}

	bool dcc_connection::just_weak_observers()
	{
		bool justWeakObservers = true;
		for (neolib::observable<dcc_connection_observer>::observer_list::iterator i = iObservers.begin(); justWeakObservers && i != iObservers.end(); ++i)
		{
			bool isWeakObserver = false;
			notify_observer(**i, dcc_connection_observer::NotifyIsWeakObserver, &isWeakObserver);
			if (!isWeakObserver)
				justWeakObservers = false;
		}
		return justWeakObservers;
	}

	void dcc_connection::notify_observer(dcc_connection_observer& aObserver, dcc_connection_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case dcc_connection_observer::NotifyActivate:
			aObserver.dcc_connection_activate(*this);
			break;
		case dcc_connection_observer::NotifyReadyChange:
			aObserver.dcc_connection_ready_changed(*this);
			break;
		case dcc_connection_observer::NotifyConnectionDisconnected:
			aObserver.dcc_connection_disconnected(*this);
			break;
		case dcc_connection_observer::NotifyClosing:
			aObserver.dcc_connection_closing(*this);
			break;
		case dcc_connection_observer::NotifyIsWeakObserver:
			*static_cast<bool*>(const_cast<void*>(aParameter)) = aObserver.is_weak_observer(*this);
			break;
		}
	}

	void dcc_connection::connection_established(dcc_stream& aStream)
	{
		if (type() == CHAT)
		{
			dcc_chat_connection& theChat = static_cast<dcc_chat_connection&>(*this);
			if (theChat.origin() != dcc_chat_connection::Listen)
			{
				dcc_message connected(theChat, dcc_message::INCOMING, dcc_message::STATUS);
				connected.content() = iConnectionManager.connected_to_message();
				theChat.new_message(connected);
			}
		}
		set_ready(true);
	}

	void dcc_connection::connection_failure(dcc_stream& aStream, const boost::system::error_code& aError)
	{
		iError = aError;
		handle_disconnection();
	}

	void dcc_connection::packet_sent(dcc_stream& aStream, const dcc_packet& aPacket)
	{
	}

	void dcc_connection::packet_arrived(dcc_stream& aStream, const dcc_packet& aPacket)
	{
	}

	void dcc_connection::transfer_failure(dcc_stream& aStream, const boost::system::error_code& aError)
	{
		iError = aError;
		aStream.close();
	}

	void dcc_connection::connection_closed(dcc_stream& aStream)
	{
		handle_disconnection();
	}

	void dcc_connection::ready()
	{
		close();
	}
}