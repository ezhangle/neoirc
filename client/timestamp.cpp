// timestamp.cpp
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
#include <clocale>
#include <array>
#include <neolib/string_utils.hpp>
#include "codes.hpp"
#include "timestamp.hpp"

namespace irc
{
	namespace 
	{
		std::string timestamp_bit(const char* aParameter, const tm* aTime)
		{
			std::tr1::array<char, 256> tempBit;
			strftime(&tempBit[0], tempBit.size(), aParameter, aTime);
			return std::string(&tempBit[0]);
		}
		std::string month_ordinal_bit(const tm* aTime)
		{
			std::string ret = neolib::integer_to_string<char>(aTime->tm_mday);
			switch(aTime->tm_mday)
			{
			case 1:
			case 21:
			case 31:
				ret += "st";
				break;
			case 2:
			case 22:
				ret + "nd";
				break;
			case 3:
			case 23:
				ret += "rd";
				break;
			default:
				ret += "th";
			}
			return ret; 
		}
		struct timestamp_codes
		{
			timestamp_codes()
			{
				iCodes["%a%"].first = std::tr1::bind(timestamp_bit, "%a", &iTime);
				iCodes["%A%"].first = std::tr1::bind(timestamp_bit, "%A", &iTime);
				iCodes["%b%"].first = std::tr1::bind(timestamp_bit, "%b", &iTime);
				iCodes["%B%"].first = std::tr1::bind(timestamp_bit, "%B", &iTime);
				iCodes["%c%"].first = std::tr1::bind(timestamp_bit, "%c", &iTime);
				iCodes["%d%"].first = std::tr1::bind(timestamp_bit, "%d", &iTime);
				iCodes["%do%"].first = std::tr1::bind(month_ordinal_bit, &iTime);
				iCodes["%H%"].first = std::tr1::bind(timestamp_bit, "%H", &iTime);
				iCodes["%I%"].first = std::tr1::bind(timestamp_bit, "%I", &iTime);
				iCodes["%j%"].first = std::tr1::bind(timestamp_bit, "%j", &iTime);
				iCodes["%m%"].first = std::tr1::bind(timestamp_bit, "%m", &iTime);
				iCodes["%M%"].first = std::tr1::bind(timestamp_bit, "%M", &iTime);
				iCodes["%p%"].first = std::tr1::bind(timestamp_bit, "%p", &iTime);
				iCodes["%S%"].first = std::tr1::bind(timestamp_bit, "%S", &iTime);
				iCodes["%U%"].first = std::tr1::bind(timestamp_bit, "%U", &iTime);
				iCodes["%w%"].first = std::tr1::bind(timestamp_bit, "%w", &iTime);
				iCodes["%W%"].first = std::tr1::bind(timestamp_bit, "%W", &iTime);
				iCodes["%x%"].first = std::tr1::bind(timestamp_bit, "%x", &iTime);
				iCodes["%X%"].first = std::tr1::bind(timestamp_bit, "%X", &iTime);
				iCodes["%y%"].first = std::tr1::bind(timestamp_bit, "%y", &iTime);
				iCodes["%Y%"].first = std::tr1::bind(timestamp_bit, "%Y", &iTime);
				iCodes["%z%"].first = std::tr1::bind(timestamp_bit, "%z", &iTime);
				iCodes["%Z%"].first = std::tr1::bind(timestamp_bit, "%Z", &iTime);
				iCodes["%#a%"].first = std::tr1::bind(timestamp_bit, "%#a", &iTime);
				iCodes["%#A%"].first = std::tr1::bind(timestamp_bit, "%#A", &iTime);
				iCodes["%#b%"].first = std::tr1::bind(timestamp_bit, "%#b", &iTime);
				iCodes["%#B%"].first = std::tr1::bind(timestamp_bit, "%#B", &iTime);
				iCodes["%#c%"].first = std::tr1::bind(timestamp_bit, "%#c", &iTime);
				iCodes["%#d%"].first = std::tr1::bind(timestamp_bit, "%#d", &iTime);
				iCodes["%#do%"].first = std::tr1::bind(month_ordinal_bit, &iTime);
				iCodes["%#H%"].first = std::tr1::bind(timestamp_bit, "%#H", &iTime);
				iCodes["%#I%"].first = std::tr1::bind(timestamp_bit, "%#I", &iTime);
				iCodes["%#j%"].first = std::tr1::bind(timestamp_bit, "%#j", &iTime);
				iCodes["%#m%"].first = std::tr1::bind(timestamp_bit, "%#m", &iTime);
				iCodes["%#M%"].first = std::tr1::bind(timestamp_bit, "%#M", &iTime);
				iCodes["%#p%"].first = std::tr1::bind(timestamp_bit, "%#p", &iTime);
				iCodes["%#S%"].first = std::tr1::bind(timestamp_bit, "%#S", &iTime);
				iCodes["%#U%"].first = std::tr1::bind(timestamp_bit, "%#U", &iTime);
				iCodes["%#w%"].first = std::tr1::bind(timestamp_bit, "%#w", &iTime);
				iCodes["%#W%"].first = std::tr1::bind(timestamp_bit, "%#W", &iTime);
				iCodes["%#x%"].first = std::tr1::bind(timestamp_bit, "%#x", &iTime);
				iCodes["%#X%"].first = std::tr1::bind(timestamp_bit, "%#X", &iTime);
				iCodes["%#y%"].first = std::tr1::bind(timestamp_bit, "%#y", &iTime);
				iCodes["%#Y%"].first = std::tr1::bind(timestamp_bit, "%#Y", &iTime);
				iCodes["%#z%"].first = std::tr1::bind(timestamp_bit, "%#z", &iTime);
				iCodes["%#Z%"].first = std::tr1::bind(timestamp_bit, "%#Z", &iTime);
			}
			codes iCodes;
			tm iTime;
		};
	}

	std::string timestamp(time_t aTime, const std::string& aFormat)
	{
		tm* theTime = localtime(&aTime);
		std::string ret = aFormat;
		setlocale(LC_TIME, "");
		static timestamp_codes sCodes;
		sCodes.iTime = *theTime;
		parse_codes(ret, sCodes.iCodes);
		return ret;
	}
}