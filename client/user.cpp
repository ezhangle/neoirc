// user.cpp
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
#include <neolib/string_utils.hpp>
#include "user.hpp"

namespace irc
{
	user::user(const std::string& aUser, casemapping::type aCasemapping, bool aMayHavePrefix, bool aMayHaveWildcards) : iAway(false), iCasemapping(aCasemapping)
	{
		if (aUser.empty())
			return;
		std::string prefix;
		std::string rest;
		if (aMayHavePrefix)
			parse_prefix(aUser, prefix, rest);
		else
			rest = aUser;
		typedef std::pair<std::string::const_iterator, std::string::const_iterator> word;
		std::vector<word> words;
		neolib::tokens(rest, std::string("!"), words, 2, false);
		if (words.size() >= 1)
		{
			bool first = true;
			while(!valid_nick_name_char(*words[0].first, first, aMayHaveWildcards) && words[0].first != words[0].second)
			{
				first = false;
				words[0].first++;
			}
			iNickName.assign(words[0].first, words[0].second);
		}
		if (words.size() >= 2)
		{
			word userhost = words[1];
			words.clear();
			std::string separator("@");
			neolib::tokens(userhost.first, userhost.second, separator.begin(), separator.end(), words, 2);
			if (words.size() >= 1)
				iUserName.assign(words[0].first, words[0].second);
			if (words.size() >= 2)
				iHostName.assign(words[1].first, words[1].second);
		}
	}

	std::string user::ban_mask() const
	{
		if (iHostName.empty() || iHostName == "*")
			return iNickName + "!*@*";
		else
			return "*!*@" + iHostName;
	}

	std::string user::ignore_mask() const
	{
		if ((iHostName.empty() || iHostName == "*") &&
			(iUserName.empty() || iUserName == "*"))
			return iNickName + "!*@*";
		else
			return "*!" + iUserName + "@" + iHostName;
	}

	std::string user::notify_mask() const
	{
		return ignore_mask();
	}

	std::string user::auto_mode_mask() const
	{
		return ignore_mask();
	}

	std::string user::msgto_form() const
	{
		std::string ret;
		if (!iNickName.empty())
			ret += iNickName;
		else 
			ret += "*";
		ret += "!";
		if (!iUserName.empty())
			ret += iUserName;
		else 
			ret += "*";
		ret += "@";
		if (!iHostName.empty())
			ret += iHostName;
		else 
			ret += "*";
		return ret;
	}

	user::difference_e user::difference(const user& aOther) const
	{
		difference_e ret = DifferenceNone;
		if (nick_name() != aOther.nick_name())
			ret = static_cast<difference_e>(ret | DifferenceNickName);
		if (user_name() != aOther.user_name())
			ret = static_cast<difference_e>(ret | DifferenceUserName);
		if (host_name() != aOther.host_name())
			ret = static_cast<difference_e>(ret | DifferenceHostName);
		if (is_operator() != aOther.is_operator() || is_voice() != aOther.is_voice())
			ret = static_cast<difference_e>(ret | DifferenceMode);
		return ret;
	}


	namespace
	{
		const std::string sValidNickNameCharacters = 
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-[]\\`_^{|}";
		const std::string sValidNickNameFirstCharacter = 
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz[]\\`_^{|}";
	}

	void parse_prefix(const std::string& aUser, std::string& aPrefix, std::string& aRest)
	{
		bool first = true;
		std::string::const_iterator start = aUser.begin();
		while(start != aUser.end() && !valid_nick_name_char(*start, first))
		{
			first = false;
			++start;
		}
		aPrefix.assign(aUser.begin(), start);
		aRest.assign(start, aUser.end());
	}

	bool valid_nick_name_char(char aCharacter, bool aFirst, bool aMayHaveWildcards)
	{
		if (static_cast<unsigned char>(aCharacter) >= 128)
			return true;
		std::string match = (aFirst ? sValidNickNameFirstCharacter : sValidNickNameCharacters);
		if (aMayHaveWildcards)
			match += "*?";
		return std::find(match.begin(), match.end(), aCharacter) != match.end();
	}
}
