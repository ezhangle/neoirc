// channel_user.cpp
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
#include <neoirc/client/channel_user.hpp>
#include <neoirc/client/connection.hpp>

namespace irc
{
	channel_user::channel_user(buffer& aBuffer, const std::string& aUser, casemapping::type aCasemapping) : user(aUser, aCasemapping), iConnection(aBuffer.connection()), iHasLastMessageId(false), iLastMessageId(0)
	{
		std::string prefix, rest;
		parse_prefix(aUser, prefix, rest);
		for (std::string::const_iterator i = prefix.begin(); i != prefix.end(); ++i)
		{
			if (iConnection.is_prefix(*i))
				iModes += iConnection.mode_from_prefix(*i);
		}
	}

	channel_user::channel_user(const channel_user& aOther) : user(aOther), iConnection(aOther.iConnection), 
		iModes(aOther.iModes), iHasLastMessageId(aOther.iHasLastMessageId), iLastMessageId(aOther.iLastMessageId)
	{
	}

	channel_user& channel_user::operator=(const channel_user& aUser)
	{
		static_cast<user&>(*this) = aUser;
		iModes = aUser.iModes;
		iHasLastMessageId = aUser.iHasLastMessageId;
		iLastMessageId = aUser.iLastMessageId;
		return *this;
	}

	channel_user& channel_user::operator=(const user& aUser)
	{
		static_cast<user&>(*this) = aUser;
		return *this;
	}

	unsigned int channel_user::compare_value() const
	{
		return iConnection.mode_compare_value(iModes);
	}

	std::string channel_user::qualified_name(const user& aUserAtEpoch) const
	{
		std::string ret;
		if (!iModes.empty())
			ret += iConnection.best_prefix(iModes);
		ret += aUserAtEpoch.nick_name();
		return ret;
	}

	bool channel_user::is_operator() const
	{
		return iConnection.is_prefix_mode('o') && iConnection.mode_compare_value(iModes) <= iConnection.mode_compare_value("o");
	}

	bool channel_user::is_voice() const
	{
		return !is_operator() && iConnection.is_prefix_mode('v') && iConnection.mode_compare_value(iModes) <= iConnection.mode_compare_value("v");
	}

	unsigned int channel_user::key_type::compare_value() const
	{
		return iConnection.mode_compare_value(iModes);
	}
}