// notice_buffer.h
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

#ifndef IRC_CLIENT_NOTICE_BUFFER
#define IRC_CLIENT_NOTICE_BUFFER

#include "buffer.hpp"
#include "server.hpp"

namespace irc
{
	class connection;

	class notice_buffer : public buffer
	{
	public:
		// types

	public:
		// construction
		notice_buffer(irc::model& aModel, irc::connection& aConnection, const irc::server& aServer);
		~notice_buffer();

	public:
		// operations
		virtual bool has_user(const std::string& aUser) const { return false; }
		virtual const irc::user& user(const std::string& aUser) const;
		virtual irc::user& user(const std::string& aUser);
		virtual bool has_user(const irc::user& aUser) const { return false; }
		virtual const irc::user& user(const irc::user& aUser) const;
		virtual irc::user& user(const irc::user& aUser);
		const irc::server& server() const { return iServer; }
		irc::server& server() { return iServer; }

	private:
		// implementation
		virtual void handle_message(const message& aMessage);

	private:
		// attributes
		irc::server iServer;
	};
}

#endif //IRC_CLIENT_NOTICE_BUFFER
