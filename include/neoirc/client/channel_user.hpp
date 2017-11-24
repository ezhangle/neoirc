// channel_user.h
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

#ifndef IRC_CLIENT_CHANNEL_USER
#define IRC_CLIENT_CHANNEL_USER

#include <neoirc/client/model.hpp>
#include <neoirc/client/user.hpp>

namespace irc
{
	class buffer;
	class connection;

	class channel_user : public user
	{
		// construction
	public:
		channel_user(buffer& aBuffer, const std::string& aUser, casemapping::type aCasemapping = casemapping::rfc1459);
		channel_user(const channel_user& aOther);

		// operations
	public:
		channel_user& operator=(const channel_user& aUser);
		channel_user& operator=(const user& aUser);
		const irc::connection& connection() const { return iConnection; }
		irc::connection& connection() { return iConnection; }
		const std::string& modes() const { return iModes; }
		std::string& modes() { return iModes; }
		unsigned int compare_value() const;
		using user::qualified_name;
		virtual std::string qualified_name(const user& aUserAtEpoch) const;
		virtual bool is_operator() const;
		virtual bool is_voice() const;
		bool has_last_message_id() const { return iHasLastMessageId; }
		model::id last_message_id() const { return iLastMessageId; }
		void set_last_message_id(model::id aLastMessageId) { iLastMessageId = aLastMessageId; iHasLastMessageId = true; }
		bool operator<(const channel_user& aOther) const 
		{ 
			if (compare_value() != aOther.compare_value())
				return compare_value() < aOther.compare_value(); 
			return user::operator<(aOther); 
		}

		// mutable_set support
	public:
		class key_type : user::key_type
		{
		public:
			key_type(const channel_user& aUser) : user::key_type(aUser), iConnection(aUser.iConnection), iModes(aUser.iModes) {}
		public:
			bool operator<(const key_type& aOther) const
			{
				if (compare_value() != aOther.compare_value())
					return compare_value() < aOther.compare_value(); 
				return user::key_type::operator<(aOther); 
			}
		private:
			unsigned int compare_value() const;
		private:
			irc::connection& iConnection;
			std::string iModes;
		};

		// attributes
	protected:
		irc::connection& iConnection;
		std::string iModes;
		bool iHasLastMessageId;
		model::id iLastMessageId;
	};
}

#endif //IRC_CLIENT_CHANNEL_USER
