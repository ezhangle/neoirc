// channel_buffer.h
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

#ifndef IRC_CLIENT_CHANNEL_BUFFER
#define IRC_CLIENT_CHANNEL_BUFFER

#include <neolib/mutable_set.hpp>
#include <neoirc/client/buffer.hpp>
#include <neoirc/client/channel.hpp>
#include <neoirc/client/channel_user.hpp>

namespace irc
{
	class connection;
	class channel_buffer;

	typedef neolib::mutable_multiset<channel_user> channel_user_list;
	// multiset as a user may change the case of their nickname letter(s)
	// resulting in old and new keys comparing the same but we want 2 different
	// iterators for the update notification.

	class channel_buffer_observer
	{
		friend class channel_buffer;
	private:
		virtual void joining_channel(channel_buffer& aBuffer) = 0;
		virtual void user_added(channel_buffer& aBuffer, channel_user_list::iterator aUser) = 0;
		virtual void user_updated(channel_buffer& aBuffer, channel_user_list::iterator aOldUser, channel_user_list::iterator aNewUser) = 0;
		virtual void user_removed(channel_buffer& aBuffer, channel_user_list::iterator aUser) = 0;
		virtual void user_host_info(channel_buffer& aBuffer, channel_user_list::iterator aUser) = 0;
		virtual void user_away_status(channel_buffer& aBuffer, channel_user_list::iterator aUser) = 0;
		virtual void user_list_updating(channel_buffer& aBuffer) = 0;
		virtual void user_list_updated(channel_buffer& aBuffer) = 0;
	public:
		enum notify_type { NotifyJoiningChannel, NotifyUserAdded, NotifyUserUpdated, NotifyUserRemoved, NotifyUserHostInfo, NotifyUserAwayStatus, NotifyUserListUpdating, NotifyUserListUpdated };
	};

	class channel_buffer : public buffer, public neolib::observable<channel_buffer_observer>
	{
		// types
	public:
		typedef channel_user_list list;

	public:
		struct user_not_found : std::logic_error { user_not_found() : std::logic_error("channel_buffer::user_not_found") {} };

		// construction
	public:
		channel_buffer(irc::model& aModel, irc::connection& aConnection, const channel& aChannel);
		~channel_buffer();

		// operations
	public:
		list& users() { return iUsers; }
		const list& users() const { return iUsers; }
		bool updating_user_list() const { return iUpdatingUserList; }
		const std::string& topic() const { return iTopic; }
		const std::string& mode() const { return iMode; }
		time_t creation_time() const { return iCreationTime; }
		bool is_operator() const;
		list::iterator find_user(const std::string& aUser);
		list::const_iterator find_user(const std::string& aUser) const;
		virtual bool has_user(const std::string& aUser) const;
		virtual const irc::user& user(const std::string& aUser) const;
		virtual irc::user& user(const std::string& aUser);
		virtual bool has_user(const irc::user& aUser) const;
		virtual const irc::user& user(const irc::user& aUser) const;
		virtual irc::user& user(const irc::user& aUser);
		virtual std::vector<const irc::user*> find_users(const std::string& aSearchString) const;
		void joining();
	public:
		virtual void add_observer(channel_buffer_observer& aObserver);

		// implementation
	private:
		void update_title();
		// from buffer
		virtual bool on_close();
		virtual void on_set_ready();
		virtual bool add_message(const message& aMessage);
		virtual void handle_message(const message& aMessage);
		virtual void user_new_host_info(const irc::user& aUser);
		virtual void user_away_status_changed(const irc::user& aUser);
		// from neolib::observable<channel_buffer_observer>
		virtual void notify_observer(channel_buffer_observer& aObserver, channel_buffer_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);

		// attributes
	private:
		channel iChannel;
		std::string iTopic;
		std::string iMode;
		time_t iCreationTime;
		list iUsers;
		bool iNewNamesList;
		bool iUpdatingUserList;
	};
}

#endif //IRC_CLIENT_CHANNEL_BUFFER