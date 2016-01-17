// channel_list.h
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

#ifndef IRC_CLIENT_CHANNEL_LIST
#define IRC_CLIENT_CHANNEL_LIST

#include "connection.hpp"

namespace irc
{
	struct channel_list_entry
	{
		std::string iChannelName;
		std::size_t iUserCount;
		std::string iChannelTopic;
		channel_list_entry() : iUserCount(0) {}
	};

	class channel_list;

	class channel_list_observer
	{
		friend class channel_list;
	private:
		virtual void channel_list_start(const channel_list& aList) = 0;
		virtual void channel_list_new_entry(const channel_list& aList, const channel_list_entry& aEntry) = 0;
		virtual void channel_list_end(const channel_list& aList) = 0;
	public:
		enum notify_type { NotifyListStart, NotifyListEntry, NotifyListEnd };
	};

	class channel_list : public neolib::observable<channel_list_observer>, private connection_observer
	{
		// types
	public:
		typedef std::deque<channel_list_entry> container_type;
		typedef container_type::const_iterator const_iterator;
		typedef container_type::iterator iterator;
	private:
		enum state_e { StateInit, StateGetting, StateGot };

		// construction
	public:
		channel_list(connection& aConnection);
		~channel_list();

		// operations
	public:
		void list();
		container_type& entries() { return iEntries; }
		const container_type& entries() const { return iEntries; }
		bool no_list() const { return iState == StateInit; }
		bool getting_list() const { return iState == StateGetting; }
		bool got_list() const { return iState == StateGot; }

		// implementation
	private:
		// from neolib::observable<channel_list_observer>
		virtual void notify_observer(channel_list_observer& aObserver, channel_list_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from connection_observer
		virtual void connection_connecting(connection& aConnection) {}
		virtual void connection_registered(connection& aConnection) {}
		virtual void buffer_added(buffer& aBuffer) {}
		virtual void buffer_removed(buffer& aBuffer) {}
		virtual void incoming_message(connection& aConnection, const message& aMessage);
		virtual void outgoing_message(connection& aConnection, const message& aMessage) {}
		virtual void connection_quitting(connection& aConnection);
		virtual void connection_disconnected(connection& aConnection);
		virtual void connection_giveup(connection& aConnection) {}

		// attributes
	private:
		connection& iParent;
		container_type iEntries;
		state_e iState;
	};
}

#endif //IRC_CLIENT_CHANNEL_LIST
