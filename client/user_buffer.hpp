// user_buffer.h
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

#ifndef IRC_CLIENT_USER_BUFFER
#define IRC_CLIENT_USER_BUFFER

#include "buffer.hpp"
#include "user.hpp"

namespace irc
{
	class connection;

	class user_buffer_observer
	{
		friend class user_buffer;
	private:
		virtual void user_host_info(user_buffer& aBuffer) = 0;
	public:
		enum notify_type { NotifyUserHostInfo };
	};

	class user_buffer : public buffer, public neolib::observable<user_buffer_observer>
	{
	public:
		// types
		enum origin_e
		{
			LOCAL, REMOTE
		};

	public:
		// construction
		user_buffer(irc::model& aModel, irc::connection& aConnection, origin_e aOrigin, irc::user& aUser);
		~user_buffer();

	public:
		// operations
		origin_e origin() const { return iOrigin; }
		const irc::user& user() const { return iUser; }
		virtual bool has_user(const std::string& aUser) const;
		virtual const irc::user& user(const std::string& aUser) const;
		virtual irc::user& user(const std::string& aUser);
		virtual bool has_user(const irc::user& aUser) const;
		virtual const irc::user& user(const irc::user& aUser) const;
		virtual irc::user& user(const irc::user& aUser);

		// implementation
		// from buffer
	private:
		virtual void handle_message(const message& aMessage);
		virtual void user_new_host_info(const irc::user& aUser);
	private:
		// from neolib::observable<user_buffer_observer>
		virtual void notify_observer(user_buffer_observer& aObserver, user_buffer_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// own
	private:
		void update_title();

	private:
		// attributes
		origin_e iOrigin;
		irc::user iUser;
	};
}

#endif //IRC_CLIENT_USER_BUFFER
