// channel_list.cpp
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
#include <neoirc/client/channel_list.hpp>

namespace irc
{
	channel_list::channel_list(connection& aConnection) : iParent(aConnection), iState(StateInit)
	{
		iParent.add_observer(*this);
	}

	channel_list::~channel_list()
	{
		iParent.remove_observer(*this);
	}

	void channel_list::list()
	{
		if (iState == StateGetting)
			return;
		message listMessage(iParent, message::OUTGOING);
		listMessage.set_command(message::LIST);
		iParent.send_message(listMessage);
	}

	void channel_list::notify_observer(channel_list_observer& aObserver, channel_list_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case channel_list_observer::NotifyListStart:
			aObserver.channel_list_start(*this);
			break;
		case channel_list_observer::NotifyListEntry:
			aObserver.channel_list_new_entry(*this, *static_cast<const channel_list_entry*>(aParameter));
			break;
		case channel_list_observer::NotifyListEnd:
			aObserver.channel_list_end(*this);
			break;
		}
	}

	void channel_list::incoming_message(connection& aConnection, const message& aMessage)
	{
		switch(aMessage.command())
		{
		case message::RPL_LIST:
			if (iState != StateGetting)
			{
				iState = StateGetting;
				iEntries.clear();
				notify_observers(channel_list_observer::NotifyListStart);
			}
			if (aMessage.parameters().size() < 2)
				break;
			iEntries.push_back(channel_list_entry());
			iEntries.back().iChannelName = aMessage.parameters()[0];
			iEntries.back().iUserCount = neolib::string_to_integer(aMessage.parameters()[1]);
			iEntries.back().iChannelTopic = aMessage.content();
			notify_observers(channel_list_observer::NotifyListEntry, iEntries.back());
			break;
		case message::RPL_LISTEND:
			iState = StateGot;
			notify_observers(channel_list_observer::NotifyListEnd);
			break;
		default:
			break;
		}
	}

	void channel_list::connection_quitting(connection& aConnection)
	{
		iEntries.clear();
		iState = StateInit;
		notify_observers(channel_list_observer::NotifyListStart);
		notify_observers(channel_list_observer::NotifyListEnd);
	}

	void channel_list::connection_disconnected(connection& aConnection)
	{
		iEntries.clear();
		iState = StateInit;
		notify_observers(channel_list_observer::NotifyListStart);
		notify_observers(channel_list_observer::NotifyListEnd);
	}
}