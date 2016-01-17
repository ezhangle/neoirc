// auto_joins.h
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

#ifndef IRC_CLIENT_AUTO_JOIN
#define IRC_CLIENT_AUTO_JOIN

#include "../common/string.hpp"
#include "identity.hpp"
#include "server.hpp"

namespace irc
{
	class auto_join
	{
	public:
		// types
	public:
		// construction
		auto_join() {}
		auto_join(const auto_join& aOther) : iServer(aOther.iServer), iNickName(aOther.iNickName), iChannel(aOther.iChannel) {}
		auto_join(const server_key& aServer, const std::string& aNickName, const std::string& aChannel) : iServer(aServer), iNickName(aNickName), iChannel(aChannel) {}
	public:
		// operations
		const irc::server_key& server() const { return iServer; }
		void set_server(const irc::server_key& aServer) { iServer = aServer; }
		const std::string& nick_name() const { return iNickName; }
		void set_nick_name(const std::string& aNickName) { iNickName = aNickName; }
		const std::string& channel() const { return iChannel; }
		void set_channel(const std::string& aChannel) { iChannel = aChannel; }
		bool operator==(const auto_join& aOther) const 
		{ 
			return server_match(iServer, aOther.iServer) && 
				iNickName == aOther.iNickName && 
				(irc::make_string(casemapping::ascii, iChannel) == aOther.iChannel || iChannel == "*"); 
		}
		bool operator!=(const auto_join& aOther) const
		{
			return !operator==(aOther);
		}
	private:
		// attributes
		irc::server_key iServer;
		std::string iNickName;
		std::string iChannel;
	};

	class auto_joins_observer
	{
		friend class auto_joins;
	private:
		virtual void auto_join_added(const auto_join& aEntry) = 0;
		virtual void auto_join_updated(const auto_join& aEntry) = 0;
		virtual void auto_join_removed(const auto_join& aEntry) = 0;
		virtual void auto_join_cleared() = 0;
		virtual void auto_join_reset() = 0;
	public:
		enum notify_type { NotifyAdded, NotifyUpdated, NotifyRemoved, NotifyCleared, NotifyReset };
	};

	class auto_joins : public neolib::observable<auto_joins_observer>, private identities_observer
	{
	public:
		// types
		typedef std::list<auto_join> container_type;

	public:
		// construction
		auto_joins(identities& aIdentities);
		~auto_joins();

	public:
		// operations
		const container_type& entries() const { return iEntries; }
		container_type& entries() { return iEntries; }
		bool has_auto_join(const server_key& aServer, const std::string& aNickName) const;
		bool has_auto_join(const server_key& aServer, const std::string& aNickName, const std::string& aChannel) const;
		bool add(const server_key& aServer, const std::string& aNickName, const std::string& aChannel);
		bool add(const auto_join& aEntry);
		void update(auto_join& aExistingEntry, const auto_join& aNewEntry);
		void remove(const server_key& aServer, const std::string& aNickName, const std::string& aChannel);
		void remove(const auto_join& aEntry);
		void clear();
		void reset(const container_type& aEntries = container_type());
		container_type::const_iterator find(casemapping::type aCasemapping, const server_key& aServer, const std::string& aNickName) const;
		container_type::iterator find(casemapping::type aCasemapping, const server_key& aServer, const std::string& aNickName);
		container_type::const_iterator find(casemapping::type aCasemapping, const auto_join& aEntry) const;
		container_type::iterator find(casemapping::type aCasemapping, const auto_join& aEntry);
		bool matches(casemapping::type aCasemapping, const auto_join& aEntry, const server_key& aServer, const std::string& aNickName) const;
		void loading(bool aLoading) { iLoading = aLoading; }

	private:
		// implementation
		// from neolib::observable<auto_joins_observer>
		virtual void notify_observer(auto_joins_observer& aObserver, auto_joins_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from identities_observer
		virtual void identity_added(const identity& aEntry);
		virtual void identity_updated(const identity& aEntry, const identity& aOldEntry);
		virtual void identity_removed(const identity& aEntry);

	private:
		// attributes
		identities& iIdentities;
		container_type iEntries;
		bool iLoading;
	};

	void read_auto_joins(model& aModel, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_auto_joins(const model& aModel);
}

#endif //IRC_CLIENT_AUTO_JOIN
