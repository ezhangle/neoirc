// connection_scripts.h
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

#ifndef IRC_CLIENT_CONNECTION_SCRIPT
#define IRC_CLIENT_CONNECTION_SCRIPT

#include "../common/string.hpp"
#include "identity.hpp"
#include "server.hpp"

namespace irc
{
	class connection_script
	{
	public:
		// types
	public:
		// construction
		connection_script() : iEnabled(true) {}
		connection_script(const connection_script& aOther) : iServer(aOther.iServer), iNickName(aOther.iNickName), iScript(aOther.iScript), iEnabled(aOther.iEnabled) {}
		connection_script(const server_key& aServer, const std::string& aNickName, const std::string& aScript, bool aEnabled) : iServer(aServer), iNickName(aNickName), iScript(aScript), iEnabled(aEnabled) {}
	public:
		// operations
		const irc::server_key& server() const { return iServer; }
		void set_server(const irc::server_key& aServer) { iServer = aServer; }
		const std::string& nick_name() const { return iNickName; }
		void set_nick_name(const std::string& aNickName) { iNickName = aNickName; }
		const std::string& script() const { return iScript; }
		void set_script(const std::string& aScript) { iScript = aScript; }
		bool enabled() const { return iEnabled; }
		void set_enabled(bool aEnabled) { iEnabled = aEnabled; }
		bool operator==(const connection_script& aOther) const 
		{ 
			return server_match(iServer, aOther.iServer) && 
				iNickName == aOther.iNickName;; 
		}
		bool operator!=(const connection_script& aOther) const
		{
			return !operator==(aOther);
		}
	private:
		// attributes
		server_key iServer;
		std::string iNickName;
		std::string iScript;
		bool iEnabled;
	};

	class connection_scripts_observer
	{
		friend class connection_scripts;
	private:
		virtual void connection_script_added(const connection_script& aEntry) = 0;
		virtual void connection_script_updated(const connection_script& aEntry) = 0;
		virtual void connection_script_removed(const connection_script& aEntry) = 0;
	public:
		enum notify_type { NotifyAdded, NotifyUpdated, NotifyRemoved };
	};

	class connection_scripts : public neolib::observable<connection_scripts_observer>, private identities_observer
	{
	public:
		// types
		typedef std::list<connection_script> container_type;

	public:
		// construction
		connection_scripts(identities& aIdentities);
		~connection_scripts();

	public:
		// operations
		const container_type& entries() const { return iEntries; }
		container_type& entries() { return iEntries; }
		bool has_connection_scripts(const server_key& aServer, const std::string& aNickName) const;
		bool has_connection_scripts(const server_key& aServer, const std::string& aNickName, const std::string& aScript) const;
		bool add(const server_key& aServer, const std::string& aNickName, const std::string& aScript, bool aEnabled);
		bool add(const connection_script& aEntry);
		void update(connection_script& aExistingEntry, const connection_script& aNewEntry);
		void remove(const server_key& aServer, const std::string& aNickName);
		void remove(const connection_script& aEntry);
		container_type::const_iterator find(casemapping::type aCasemapping, const server_key& aServer, const std::string& aNickName) const;
		container_type::iterator find(casemapping::type aCasemapping, const server_key& aServer, const std::string& aNickName);
		container_type::const_iterator find(casemapping::type aCasemapping, const connection_script& aEntry) const;
		container_type::iterator find(casemapping::type aCasemapping, const connection_script& aEntry);
		bool matches(casemapping::type aCasemapping, const connection_script& aEntry, const server_key& aServer, const std::string& aNickName) const;
		void loading(bool aLoading) { iLoading = aLoading; }

	private:
		// implementation
		// from neolib::observable<connection_scripts_observer>
		virtual void notify_observer(connection_scripts_observer& aObserver, connection_scripts_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
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

	void read_connection_scripts(model& aModel, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_connection_scripts(const model& aModel);
}

#endif //IRC_CLIENT_CONNECTION_SCRIPT
