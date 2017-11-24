// auto_mode.h
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

#ifndef IRC_CLIENT_AUTO_MODE
#define IRC_CLIENT_AUTO_MODE

#include <neolib/observable.hpp>
#include <neoirc/client/server.hpp>
#include <neoirc/client/user.hpp>
#include <neoirc/client/message.hpp>

namespace irc
{
	class model;

	class auto_mode_entry
	{
	public:
		// types
		enum type_e { Op, Voice, BanKick };
	public:
		// construction
		auto_mode_entry() : iChannel("*"), iType(Op) {}
		auto_mode_entry(const auto_mode_entry& aOther) : iServer(aOther.iServer), iUser(aOther.iUser), iChannel(aOther.iChannel), iType(aOther.iType), iData(aOther.iData) {}
		auto_mode_entry(const server_key& aServer, const user& aUser, const std::string& aChannel, type_e aType, const std::string& aData) : iServer(aServer), iUser(aUser), iChannel(aChannel), iType(aType), iData(aData) {}
	public:
		// operations
		const irc::server_key& server() const { return iServer; }
		irc::server_key& server() { return iServer; }
		const irc::user& user() const { return iUser; }
		irc::user& user() { return iUser; }
		const std::string& channel() const { return iChannel; }
		std::string& channel() { return iChannel; }
		type_e type() const { return iType; }
		type_e& type() { return iType; }
		const std::string& data() const { return iData; }
		std::string& data() { return iData; }
		bool operator==(const auto_mode_entry& aOther) const 
		{ 
			return server_match(iServer, aOther.iServer) && 
				iUser == aOther.iUser && 
				(irc::make_string(casemapping::ascii, iChannel) == aOther.iChannel || iChannel == "*"); 
		}
	private:
		// attributes
		irc::server_key iServer;
		irc::user iUser;
		std::string iChannel;
		type_e iType;
		std::string iData;
	};

	class auto_mode_list_observer
	{
		friend class auto_mode;
	private:
		virtual void auto_mode_added(const auto_mode_entry& aEntry) = 0;
		virtual void auto_mode_updated(const auto_mode_entry& aEntry) = 0;
		virtual void auto_mode_removed(const auto_mode_entry& aEntry) = 0;
	public:
		enum notify_type { NotifyAdded, NotifyUpdated, NotifyRemoved };
	};

	class auto_mode : public neolib::observable<auto_mode_list_observer>
	{
	public:
		// types
		typedef std::list<auto_mode_entry> container_type;

	public:
		// construction
		auto_mode() : iLoading(false) {}

	public:
		// operations
		const container_type& entries() const { return iEntries; }
		container_type& entries() { return iEntries; }
		bool has_auto_mode(casemapping::type aCasemapping, const server_key& aServer, const user& aUser, const std::string& aChannel) const;
		bool has_auto_mode(casemapping::type aCasemapping, const server_key& aServer, const user& aUser, const std::string& aChannel, auto_mode_entry::type_e aType) const;
		bool add(const server_key& aServer, const user& aUser, const std::string& aChannel, auto_mode_entry::type_e aType, const std::string& aData);
		bool add(const auto_mode_entry& aEntry);
		void update(auto_mode_entry& aExistingEntry, const auto_mode_entry& aNewEntry);
		void update_user(casemapping::type aCasemapping, const server_key& aServer, const user& aOldUser, const user& aNewUser);
		void remove(const server_key& aServer, const user& aUser, const std::string& aChannel);
		void remove(const auto_mode_entry& aEntry);
		container_type::const_iterator find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser, const std::string& aChannel) const;
		container_type::iterator find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser, const std::string& aChannel);
		container_type::const_iterator find(casemapping::type aCasemapping, const auto_mode_entry& aEntry) const;
		container_type::iterator find(casemapping::type aCasemapping, const auto_mode_entry& aEntry);
		bool matches(casemapping::type aCasemapping, const auto_mode_entry& aEntry, const server_key& aServer, const user& aUser, const std::string& aChannel) const;
		void loading(bool aLoading) { iLoading = aLoading; }

	private:
		// implementation
		// from neolib::observable<auto_mode_list_observer>
		virtual void notify_observer(auto_mode_list_observer& aObserver, auto_mode_list_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);

	private:
		// attributes
		container_type iEntries;
		bool iLoading;
	};

	void read_auto_mode_list(model& aModel, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_auto_mode_list(const model& aModel);
}

#endif //IRC_CLIENT_AUTO_MODE
