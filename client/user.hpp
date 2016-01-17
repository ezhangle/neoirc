// user.h
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

#ifndef IRC_CLIENT_USER
#define IRC_CLIENT_USER

#include <list>
#include "../common/string.hpp"

namespace irc
{
	class user
	{
		// construction
	public:
		user(casemapping::type aCasemapping = casemapping::rfc1459) : iAway(false), iCasemapping(aCasemapping) {}
		explicit user(const std::string& aUser, casemapping::type aCasemapping = casemapping::rfc1459, bool aMayHavePrefix = true, bool aMayHaveWildcards = false);
		user(const std::string& aNickName, const std::string& aUserName, const std::string& aHostName, casemapping::type aCasemapping = casemapping::rfc1459) : 
			iNickName(aNickName), iUserName(aUserName), iHostName(aHostName), iAway(false), iCasemapping(aCasemapping) {}

		// operations
	public:
		const std::string& nick_name() const { return iNickName; }
		const std::string& user_name() const { return iUserName; }
		const std::string& host_name() const { return iHostName; }
		const std::string& full_name() const { return iFullName; }
		bool away() const { return iAway; }
		irc::casemapping::type casemapping() const { return iCasemapping; }
		std::string& nick_name() { return iNickName; }
		std::string& user_name() { return iUserName; }
		std::string& host_name() { return iHostName; }
		std::string& full_name() { return iFullName; }
		void set_away(bool aAway) { iAway = aAway; }
		irc::casemapping::type& casemapping() { return iCasemapping; }
		std::string qualified_name() const { return qualified_name(*this); }
		virtual std::string qualified_name(const user& aUserAtEpoch) const { return aUserAtEpoch.nick_name(); }
		virtual bool is_operator() const { return false; }
		virtual bool is_voice() const { return false; }
		std::string ban_mask() const;
		std::string ignore_mask() const;
		std::string notify_mask() const;
		std::string auto_mode_mask() const;
		std::string msgto_form() const;
		bool has_user_name() const { return !iUserName.empty() && iUserName != "*"; }
		bool has_host_name() const { return !iHostName.empty() && iHostName != "*"; }
		bool has_full_name() const { return !iFullName.empty(); }
		bool operator==(const user& aOther) const { return !iNickName.empty() && !aOther.iNickName.empty() ? irc::make_string(iCasemapping, iNickName) == aOther.iNickName : 
			!iUserName.empty() && iUserName == aOther.iUserName && 
			!iHostName.empty() && iHostName == aOther.iHostName; }
		bool operator!=(const user& aOther) const { return !(*this == aOther); }
		bool operator<(const user& aOther) const { return irc::make_string(iCasemapping, iNickName) < aOther.iNickName; }
		enum difference_e { DifferenceNone = 0x00, DifferenceNickName = 0x01, DifferenceUserName = 0x02, DifferenceHostName = 0x04, DifferenceMode = 0x08 };
		difference_e difference(const user& aOther) const;

		// mutable_set support
	public:
		class key_type
		{
		public:
			key_type(const user& aUser) : iNickName(aUser.iNickName), iCasemapping(aUser.iCasemapping) {}
		public:
			bool operator<(const key_type& aOther) const { return irc::make_string(iCasemapping, iNickName) < aOther.iNickName; }
		private:
			std::string iNickName;
			casemapping::type iCasemapping;
		};

		// attributes
	protected:
		std::string iNickName;
		std::string iUserName;
		std::string iHostName;
		std::string iFullName;
		bool iAway;
		casemapping::type iCasemapping;
	};

	typedef std::list<user> user_list;

	void parse_prefix(const std::string& aUser, std::string& aPrefix, std::string& aRest);
	bool valid_nick_name_char(char aCharacter, bool aFirst, bool aMayHaveWildcards = false);
}

#endif //IRC_CLIENT_USER
