// mode.h
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

#ifndef IRC_CLIENT_MODE
#define IRC_CLIENT_MODE

#include <string>
#include <list>

namespace irc
{
	class message;

	struct mode
	{
		enum direction
		{
			ADD, SUBTRACT
		};

		enum type
		{
			OPERATOR = 'o',
			VOICE = 'v',
			BAN = 'b',
			EXCEPT = 'e',
			INVITE = 'I',
			ANONYMOUS = 'a',
			INVITEONLY = 'i',
			MODERATED = 'm',
			NOOUTSIDEMESSAGES = 'n',
			QUIET = 'q',
			PRIVATE = 'p',
			SECRET = 's',
			SERVERREOP = 'r',
			TOPICOPCHANGE = 't',
			KEY = 'k',
			USERLIMIT = 'l'
		};

		direction iDirection;
		type iType;
		std::string iCharacter;
		std::string iParameter;
		typedef std::list<mode> list;
		typedef std::list<std::string> parameters;
	};

	void parse_mode(const message& aMessage, mode::list& aModeList);
}

#endif //IRC_CLIENT_MODE
