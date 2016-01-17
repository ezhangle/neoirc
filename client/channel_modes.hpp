// channel_modes.h
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

#ifndef IRC_CLIENT_CHANNEL_MODES
#define IRC_CLIENT_CHANNEL_MODES

#include <array>
#include <ctime>
#include <clocale>
#include "connection.hpp"
#include "channel_buffer.hpp"

namespace irc
{
	class channel_modes;

	struct channel_modes_entry
	{
		typedef neolib::optional<time_t> date;
		channel_modes_entry(const string& aUser, const string& aSetBy, const date& aDate) : 
			iUser(aUser), iSetBy(aSetBy), iDate(aDate) {}
		std::string date_as_string() const 
		{
			if (!iDate)
				return "";
			else
			{
				std::time_t ttTime = *iDate;
				std::tm* tmTime = std::localtime(&ttTime);
				std::tr1::array<char, 256> strTime;
				std::setlocale(LC_TIME, "");
				std::strftime(&strTime[0], strTime.size(), "%a %b %#d %H:%M:%S %Y", tmTime);
				return std::string(&strTime[0]);
			}
		}
		string iUser;
		string iSetBy;
		date iDate;
	};

	class channel_modes_observer
	{
		friend class channel_modes;
	private:
		virtual void channel_modes_ban_list_entry_added(const channel_modes& aModes, const channel_modes_entry& aEntry) = 0;
		virtual void channel_modes_ban_list_entry_removed(const channel_modes& aModes, const channel_modes_entry& aEntry) = 0;
		virtual void channel_modes_ban_list_reset(const channel_modes& aModes) = 0;
		virtual void channel_modes_except_list_entry_added(const channel_modes& aModes, const channel_modes_entry& aEntry) = 0;
		virtual void channel_modes_except_list_entry_removed(const channel_modes& aModes, const channel_modes_entry& aEntry) = 0;
		virtual void channel_modes_except_list_reset(const channel_modes& aModes) = 0;
		virtual void channel_modes_invite_list_entry_added(const channel_modes& aModes, const channel_modes_entry& aEntry) = 0;
		virtual void channel_modes_invite_list_entry_removed(const channel_modes& aModes, const channel_modes_entry& aEntry) = 0;
		virtual void channel_modes_invite_list_reset(const channel_modes& aModes) = 0;
		virtual void channel_modes_list_done(const channel_modes& aModes) = 0;
		virtual void channel_modes_modes_updated(const channel_modes& aModes) = 0;
	public:
		enum notify_type { NotifyBanListEntryAdded, NotifyBanListEntryRemoved, NotifyBanListReset, NotifyExceptListEntryAdded, NotifyExceptListEntryRemoved, NotifyExceptListReset, NotifyInviteListEntryAdded, NotifyInviteListEntryRemoved, NotifyInviteListReset, NotifyListDone, NotifyModesUpdated };
	};

	class channel_modes : public neolib::observable<channel_modes_observer>, private connection_observer
	{
	public:
		// types
		typedef std::vector<channel_modes_entry> container_type;
		typedef container_type::const_iterator const_iterator;
		typedef container_type::iterator iterator;
		enum modes_e
		{
			None = 0x000,
			UserLimit = 0x001,
			Topic = 0x002,
			ServerReop = 0x004,
			Secret = 0x008,
			Quiet = 0x010,
			Private = 0x020,
			NoOutsideMessages = 0x040,
			Moderated = 0x080,
			InviteOnly = 0x100,
			ChannelKey = 0x200,
			Anonymous = 0x400,
		};

	public:
		// construction
		channel_modes(channel_buffer& aChannel);
		~channel_modes();

	public:
		// operations
		container_type& ban_list() { return iBanList; }
		const container_type& ban_list() const { return iBanList; }
		container_type& except_list() { return iExceptList; }
		const container_type& except_list() const { return iExceptList; }
		container_type& invite_list() { return iInviteList; }
		const container_type& invite_list() const { return iInviteList; }
		bool got_ban_list() const { return iGotBanList; }
		bool got_except_list() const { return iGotExceptList; }
		bool got_invite_list() const { return iGotInviteList; }
		modes_e modes() const { return iModes; }
		void set_modes(modes_e aNewModes, const std::string& aChannelKey, const std::string& aUserLimit);
		const std::string& channel_key() const {return iChannelKey; }
		const std::string& user_limit() const { return iUserLimit; }
		bool got_modes() const { return iGotModes; }

	private:
		// implementation
		// from neolib::observable<channel_modes_observer>
		virtual void notify_observer(channel_modes_observer& aObserver, channel_modes_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from connection_observer
		virtual void connection_connecting(connection& aConnection) {}
		virtual void connection_registered(connection& aConnection) {}
		virtual void buffer_added(buffer& aBuffer) {}
		virtual void buffer_removed(buffer& aBuffer) {}
		virtual void incoming_message(connection& aConnection, const message& aMessage);
		virtual void outgoing_message(connection& aConnection, const message& aMessage) {}
		virtual void connection_quitting(connection& aConnection) {}
		virtual void connection_disconnected(connection& aConnection) {}
		virtual void connection_giveup(connection& aConnection) {}

	private:
		// attributes
		channel_buffer& iParent;
		container_type iBanList;
		container_type iExceptList;
		container_type iInviteList;
		bool iGotBanList;
		bool iGotExceptList;
		bool iGotInviteList;
		modes_e iModes;
		std::string iChannelKey;
		std::string iUserLimit;
		bool iGotModes;
	};
}

#endif //IRC_CLIENT_CHANNEL_MODES
