// identity.h
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

#ifndef IRC_CLIENT_IDENTITY
#define IRC_CLIENT_IDENTITY

#include <map>
#include <neolib/observable.hpp>
#include <neolib/string_utils.hpp>
#include <neoirc/client/server.hpp>

namespace irc
{
	class model;

	class identity
	{
	public:
		// types
		typedef std::vector<std::string> alternate_nick_names_t;
		typedef std::map<server_key, std::string> passwords_t;

	public:
		// construction
		identity(const std::string& aNickName, const std::string& aFullName, const std::string& aEmailAddress, bool aInvisible, const std::pair<std::string, std::string>& aLastServerUsed, const alternate_nick_names_t& aAlternateNickNames) : iNickName(aNickName),  iFullName(aFullName), iEmailAddress(aEmailAddress), iInvisible(aInvisible), iLastServerUsed(aLastServerUsed), iAlternateNickNames(aAlternateNickNames) {}
		identity() : iInvisible(false) {}

	public:
		// operations
		const std::string& nick_name() const { return iNickName; }
		const std::string& full_name() const { return iFullName; }
		const std::string& email_address() const { return iEmailAddress; }
		bool invisible() const { return iInvisible; }
		const std::pair<std::string, std::string>& last_server_used() const { return iLastServerUsed; }
		const alternate_nick_names_t& alternate_nick_names() const { return iAlternateNickNames; }
		const passwords_t& passwords() const { return iPasswords; }
		void set_nick_name(const std::string& aNickName) { iNickName = aNickName; }
		void set_full_name(const std::string& aFullname) { iFullName = aFullname; }
		void set_email_address(const std::string& aEmailAddress) { iEmailAddress = aEmailAddress; }
		void set_invisible(bool aInvisible) { iInvisible = aInvisible; }
		void set_last_server_used(const std::pair<std::string, std::string>& aLastServerUsed) { iLastServerUsed = aLastServerUsed; }
		alternate_nick_names_t& alternate_nick_names() { return iAlternateNickNames; }
		passwords_t& passwords() { return iPasswords; }
		bool operator==(const identity& aOther) const { return iNickName == aOther.iNickName; }
		bool operator!=(const identity& aOther) const { return iNickName != aOther.iNickName; }
		bool operator<(const identity& aOther) const { return neolib::make_ci_string(iNickName) < neolib::make_ci_string(aOther.iNickName); }

	private:
		// attributes
		std::string iNickName;
		std::string iFullName;
		std::string iEmailAddress;
		bool iInvisible;
		std::pair<std::string, std::string> iLastServerUsed;
		alternate_nick_names_t iAlternateNickNames;
		passwords_t iPasswords;
	};

	class identities_observer
	{
		friend class identities;
	private:
		virtual void identity_added(const identity& aEntry) = 0;
		virtual void identity_updated(const identity& aEntry, const identity& aOldEntry) = 0;
		virtual void identity_removed(const identity& aEntry) = 0;
	public:
		enum notify_type { NotifyAdded, NotifyUpdated, NotifyRemoved };
	};

	class identities : public neolib::observable<identities_observer>
	{
	public:
		// types
		typedef std::list<identity> container_type;

	public:
		// construction
		identities() {}
		~identities() {}

	public:
		// operations
		container_type& identity_list() { return iIdentities; }
		const container_type& identity_list() const { return iIdentities; }
		bool empty() const { return iIdentities.empty(); }
		identity& add_item(const identity& aItem);
		void update_item(identity& aExistingItem, const identity& aUpdatedItem);
		void delete_item(identity& aItem);
		container_type::iterator find_item(const std::string& aNickName);
		container_type::iterator find_item(const identity& aIdentity);
		void read(const model& aModel, std::string& aDefaultIdentity, std::function<bool()> aErrorFunction = std::function<bool()>());
		void write(const model& aModel, const std::string& aDefaultIdentity) const;

	private:
		// from neolib::observable<identities_observer>
		virtual void notify_observer(identities_observer& aObserver, identities_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);

	private:
		// attributes
		container_type iIdentities;
	};

	typedef identities::container_type identity_list;

	void read_identity_list(model& aModel, std::string& aDefaultIdentity, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_identity_list(const model& aModel, const std::string& aDefaultIdentity);
}

#endif //IRC_CLIENT_IDENTITY