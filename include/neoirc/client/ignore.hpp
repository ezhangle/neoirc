// ignore.h
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

#ifndef IRC_CLIENT_IGNORE
#define IRC_CLIENT_IGNORE

#include <neoirc/client/server.hpp>
#include <neoirc/client/user.hpp>

namespace irc
{
	class ignore_entry
	{
	public:
		// construction
		ignore_entry() {}
		ignore_entry(const ignore_entry& aOther) : iServer(aOther.iServer), iUser(aOther.iUser) {}
		ignore_entry(const server_key& aServer, const user& aUser) : iServer(aServer), iUser(aUser) {}
	public:
		// operations
		const irc::server_key& server() const { return iServer; }
		irc::server_key& server() { return iServer; }
		const irc::user& user() const { return iUser; }
		irc::user& user() { return iUser; }
	private:
		// attributes
		server_key iServer;
		irc::user iUser;
	};

	class ignore_list_observer
	{
		friend class ignore_list;
	private:
		virtual void ignore_added(const ignore_entry& aEntry) = 0;
		virtual void ignore_updated(const ignore_entry& aEntry) = 0;
		virtual void ignore_removed(const ignore_entry& aEntry) = 0;
	public:
		enum notify_type { NotifyAdded, NotifyUpdated, NotifyRemoved };
	};

	class ignore_list : public neolib::observable<ignore_list_observer>
	{
	public:
		// types
		typedef std::list<ignore_entry> container_type;

	public:
		// construction
		ignore_list() : iLoading(false) {}

	public:
		// operations
		const container_type& entries() const { return iEntries; }
		container_type& entries() { return iEntries; }
		bool ignored(casemapping::type aCasemapping, const server_key& aServer, const user& aUser) const;
		bool add(const server_key& aServer, const user& aUser);
		bool add(const ignore_entry& aEntry);
		void update(ignore_entry& aExistingEntry, const ignore_entry& aNewEntry);
		void update_user(casemapping::type aCasemapping, const server_key& aServer, const user& aOldUser, const user& aNewUser);
		void remove(const server_key& aServer, const user& aUser);
		void remove(const ignore_entry& aEntry);
		container_type::const_iterator find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser) const;
		container_type::iterator find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser);
		container_type::const_iterator find(casemapping::type aCasemapping, const ignore_entry& aEntry) const;
		container_type::iterator find(casemapping::type aCasemapping, const ignore_entry& aEntry);
		void loading(bool aLoading) { iLoading = aLoading; }

	private:
		// implementation
		// from neolib::observable<ignore_list_observer>
		virtual void notify_observer(ignore_list_observer& aObserver, ignore_list_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);

	private:
		// attributes
		container_type iEntries;
		bool iLoading;
	};

	void read_ignore_list(model& aModel, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_ignore_list(const model& aModel);
}

#endif //IRC_CLIENT_IGNORE
