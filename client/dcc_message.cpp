// dcc_message.cpp
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
#include <array>
#include "dcc_connection_manager.hpp"
#include "dcc_chat_connection.hpp"
#include "user.hpp"
#include "dcc_message.hpp"
#include "message_strings.hpp"
#include "timestamp.hpp"

namespace irc
{
	dcc_message::dcc_message(dcc_chat_connection& aConnection, direction_e aDirection, type_e aType, bool aFromLog) : 
		iId(aConnection.dcc_connection_manager().next_message_id()), iFromLog(aFromLog), iTime(::time(0)), iDirection(aDirection), iType(aType)
	{
	}

	bool dcc_message::parse_log(const std::string& aLogEntry)
	{
		typedef std::pair<std::string::const_iterator, std::string::const_iterator> word_t;
		neolib::vecarray<word_t, 3> words;
		std::string sep(" ");
		neolib::tokens(aLogEntry.begin(), aLogEntry.end(), sep.begin(), sep.end(), words, 3, false);
		if (words.size() != 3 || words[0].first == words[0].second || words[1].first == words[1].second || words[2].first == words[2].second)
			return false;
		iTime = neolib::string_to_unsigned_integer(std::string(words[0].first, words[0].second));
		iDirection = (std::string(words[1].first, words[1].second) == "<" ? INCOMING : OUTGOING);
		iContent.assign(words[2].first, aLogEntry.end());
		return true;
	}

	std::string dcc_message::to_string() const
	{
		std::string ret = iContent;
		if (ret.empty() || ret[ret.size() - 1] != '\n')
			ret += "\r\n";
		return ret;
	}

	namespace
	{
		std::string timestamp_bit(const char* aParameter, const tm* aTime)
		{
			std::tr1::array<char, 256> tempBit;
			strftime(&tempBit[0], tempBit.size(), aParameter, aTime);
			return std::string(&tempBit[0]);
		}
	}

	std::string dcc_message::to_nice_string(const message_strings& aMessageStrings, const dcc_chat_connection& aConnection, const std::string& aAppendToContent, const void* aParameter, bool aAddTimeStamp, bool aAppendCRLF) const
	{
		std::string theContent = iContent;
		theContent += aAppendToContent;

		std::string ret = type() == NORMAL ? aMessageStrings.standard_message() : theContent;
		const user& theUser = (iDirection == INCOMING ? aConnection.remote_user() : aConnection.local_user());
		neolib::replace_string(ret, std::string("%N%"), theUser.nick_name());
		neolib::replace_string(ret, std::string("%N!%"), theUser.msgto_form());
		neolib::replace_string(ret, std::string("%QN%"), theUser.qualified_name());
		neolib::replace_string(ret, std::string("%M%"), theContent);

		if (aMessageStrings.mode() == message_strings::Column && ret.find('\t') == std::string::npos)
			ret = '\t' + ret;

		if (aAppendCRLF && ret[ret.size() - 1] != '\n')
			ret += "\r\n";

		if (aMessageStrings.display_timestamps() && aAddTimeStamp)
		{
			std::string format = aMessageStrings.timestamp_format();
			if (aMessageStrings.mode() == message_strings::Column)
			{
				neolib::remove_leading(format, std::string(" "));
				neolib::remove_trailing(format, std::string(" "));
				format += '\t';
			}
			ret = timestamp(iTime, format) + ret;
		}

		return ret;
	}
}