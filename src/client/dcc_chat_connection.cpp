// dcc_chat_connection.cpp
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
#include <neoirc/client/dcc_chat_connection.hpp>
#include <neoirc/client/dcc_connection_manager.hpp>
#include <neoirc/client/model.hpp>

namespace irc
{
	dcc_chat_connection::dcc_chat_connection(irc::connection* aConnection, origin_e aOrigin, const std::string& aName, const user& aLocalUser, const user& aRemoteUser, irc::dcc_connection_manager& aConnectionManager, neolib::io_task& aIoTask) : 
		dcc_connection(CHAT, aName, aLocalUser, aRemoteUser, aConnectionManager, aIoTask),
		iConnection(aConnection),
		iOrigin(aOrigin), iBufferSize(aConnectionManager.model().buffer_size())
	{
		iConnectionManager.model().neolib::observable<model_observer>::add_observer(*this);
		iConnectionManager.object_created(*this);
		iConnectionManager.model().connection_manager().add_observer(*this);
	}

	dcc_chat_connection::~dcc_chat_connection()
	{
		iConnectionManager.model().neolib::observable<model_observer>::remove_observer(*this);
		iConnectionManager.object_destroyed(*this);
		iConnectionManager.model().connection_manager().remove_observer(*this);
	}

	void dcc_chat_connection::new_message(const std::string& aMessage, bool aNotCommand)
	{
		typedef std::vector<std::pair<std::string::const_iterator, std::string::const_iterator> > lines_t;
		lines_t lines;
		neolib::tokens(aMessage, std::string("\n"), lines, 0, false);
		for (lines_t::iterator i = lines.begin(); i != lines.end(); ++i)
		{
			dcc_message nextMessage(*this, dcc_message::OUTGOING);
			if (i->second != i->first && *(i->second-1) == '\r')
				--i->second;
			nextMessage.content().assign(i->first, i->second);
			new_message(nextMessage);
		}
	}

	void dcc_chat_connection::new_message(const dcc_message& aMessage, bool aNotCommand)
	{
		if (aMessage.direction() == dcc_message::OUTGOING)
		{
			if (!aMessage.content().empty() && aMessage.content()[0] == '/' && !aNotCommand)
			{
				if (dcc_connection_manager().model().do_custom_command(aMessage.content()))
					return;
				else if (connection() != 0)
					connection()->server_buffer().new_message(aMessage.content());
				else if (dcc_connection_manager().model().connection_manager().connections().size() == 1)
					(*dcc_connection_manager().model().connection_manager().connections().begin())->server_buffer().new_message(aMessage.content());
				else 
					neolib::observable<dcc_chat_connection_observer>::notify_observers(dcc_chat_connection_observer::NotifyMessageFailure, aMessage);
				return;
			}
			if (!is_ready())
			{
				neolib::observable<dcc_chat_connection_observer>::notify_observers(dcc_chat_connection_observer::NotifyMessageFailure, aMessage);
				return;
			}
		}
		add_message(aMessage);
		handle_message(aMessage);
	}

	void dcc_chat_connection::open(const dcc_message& aMessage)
	{
		neolib::observable<dcc_chat_connection_observer>::notify_observers(dcc_chat_connection_observer::NotifyOpen, aMessage);
	}

	bool dcc_chat_connection::find_message(model::id aMessageId, dcc_connection*& aConnection, dcc_message*& aMessage)
	{
		for (message_list::iterator i = iMessages.begin(); i != iMessages.end(); ++i)
			if (i->id() == aMessageId)
			{
				aConnection = this;
				aMessage = &*i;
				return true;
			}
		return false;
	}

	void dcc_chat_connection::scrollback(const message_list& aMessages)
	{
		message_list::const_reverse_iterator i = aMessages.rbegin();
		while (iMessages.size() < iBufferSize && i != aMessages.rend())
			iMessages.push_front(*i++);
		neolib::observable<dcc_chat_connection_observer>::notify_observers(dcc_chat_connection_observer::NotifyScrollbacked);
	}

	void dcc_chat_connection::add_message(const dcc_message& aMessage)
	{
		iMessages.push_back(aMessage);
		buffer_size_changed(iBufferSize);
	}

	void dcc_chat_connection::handle_message(const dcc_message& aMessage)
	{
		neolib::observable<dcc_chat_connection_observer>::notify_observers(dcc_chat_connection_observer::NotifyMessage, aMessage);
		if (aMessage.direction() == dcc_message::OUTGOING)
			send_message(aMessage);
	}

	void dcc_chat_connection::send_message(const dcc_message& aMessage)
	{
		dcc_packet thepacket(aMessage.content().data(), aMessage.content().size());
		thepacket.contents().push_back('\r');
		thepacket.contents().push_back('\n');
		stream().send_packet(thepacket);
	}

	void dcc_chat_connection::notify_observer(dcc_chat_connection_observer& aObserver, dcc_chat_connection_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case dcc_chat_connection_observer::NotifyOpen:
			aObserver.dcc_chat_open(*this, *static_cast<const dcc_message*>(aParameter));
			break;
		case dcc_chat_connection_observer::NotifyMessage:
			aObserver.dcc_chat_message(*this, *static_cast<const dcc_message*>(aParameter));
			break;
		case dcc_chat_connection_observer::NotifyMessageFailure:
			aObserver.dcc_chat_message_failure(*this, *static_cast<const dcc_message*>(aParameter));
			break;
		case dcc_chat_connection_observer::NotifyScrollbacked:
			aObserver.dcc_chat_scrollbacked(*this);
			break;
		}
	}

	void dcc_chat_connection::packet_sent(dcc_stream& aStream, const dcc_packet& aPacket)
	{
	}

	void dcc_chat_connection::packet_arrived(dcc_stream& aStream, const dcc_packet& aPacket)
	{
		iPartialLine.insert(iPartialLine.end(), aPacket.contents().begin(), aPacket.contents().end());
		typedef std::pair<dcc_packet::contents_type::const_iterator, dcc_packet::contents_type::const_iterator> line_t;
		typedef std::vector<line_t> lines_t;
		lines_t lines;
		dcc_packet::contents_type delims;
		delims.push_back('\n');
		neolib::tokens(iPartialLine.begin(), iPartialLine.end(), delims.begin(), delims.end(), lines, 0, false);
		for (lines_t::iterator i = lines.begin(); i != lines.end();)
		{
			line_t theLine = *i++;
			if (i == lines.end())
			{
				if (theLine.second == iPartialLine.end())
				{
					iPartialLine.erase(iPartialLine.begin(), iPartialLine.begin() + (theLine.first - iPartialLine.begin()));
					return;
				}
			}
			dcc_message theMessage(*this, dcc_message::INCOMING);
			theMessage.content().assign(theLine.first, theLine.second);
			if (!theMessage.content().empty() && 
				theMessage.content()[theMessage.content().size() - 1] == '\r')
				theMessage.content().erase(theMessage.content().size() - 1, 1);
			new_message(theMessage);
		}
		iPartialLine.clear();
	}

	void dcc_chat_connection::buffer_size_changed(std::size_t aNewBufferSize)
	{
		iBufferSize = aNewBufferSize;
		while (iMessages.size() > iBufferSize)
			iMessages.pop_front();
	}
}