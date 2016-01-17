// user_buffer.cpp
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
#include "connection.hpp"
#include "contacts.hpp"
#include "user_buffer.hpp"

namespace irc
{
	user_buffer::user_buffer(irc::model& aModel, irc::connection& aConnection, origin_e aOrigin, irc::user& aUser) : 
		buffer(aModel, USER, aConnection, aUser.nick_name()), iOrigin(aOrigin), iUser(aUser) 
	{
		update_title();
		connection().object_created(*this);
	}

	user_buffer::~user_buffer()
	{
		connection().object_destroyed(*this);
	}

	bool user_buffer::has_user(const std::string& aUser) const 
	{ 
		return irc::user(aUser, *this) == iUser; 
	}

	const user& user_buffer::user(const std::string& aUser) const
	{
		if (!has_user(aUser))
			throw invalid_user();
		return iUser;
	}

	user& user_buffer::user(const std::string& aUser)
	{
		if (!has_user(aUser))
			throw invalid_user();
		return iUser;
	}

	bool user_buffer::has_user(const irc::user& aUser) const 
	{ 
		return irc::user(std::string(), aUser.user_name(), aUser.host_name(), *this) == iUser; 
	}

	const user& user_buffer::user(const irc::user& aUser) const
	{
		if (!has_user(aUser))
			throw invalid_user();
		return iUser;
	}

	user& user_buffer::user(const irc::user& aUser)
	{
		if (!has_user(aUser))
			throw invalid_user();
		return iUser;
	}

	void user_buffer::handle_message(const message& aMessage)
	{
		switch(aMessage.command())
		{
		case message::NICK:
			if (irc::user(aMessage.origin(), *this).nick_name() == irc::make_string(*this, name()))
			{
				if (!aMessage.parameters().empty())
					iUser.nick_name() = aMessage.parameters()[0];
				set_name(iUser.nick_name());
				update_title();
			}
			break;
		case message::PRIVMSG:
			break;
		default:
			break;
		}
		buffer::handle_message(aMessage);
	}

	void user_buffer::user_new_host_info(const irc::user& aUser)
	{
		neolib::observable<user_buffer_observer>::notify_observers(user_buffer_observer::NotifyUserHostInfo);
	}

	void user_buffer::notify_observer(user_buffer_observer& aObserver, user_buffer_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case user_buffer_observer::NotifyUserHostInfo:
			aObserver.user_host_info(*this);
			break;
		}
	}

	void user_buffer::update_title()
	{
		std::string newTitle = name();
		if (model().contacts().contains(*this, connection().server(), user()))
		{
			contacts::container_type::const_iterator i = model().contacts().find(*this, connection().server(), user());
			if (!i->name().empty() && i->name() != name())
				newTitle = newTitle + " (" + i->name() + ")";
		}
		set_title(newTitle);
	}
}