// server_buffer.cpp
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
#include "server_buffer.hpp"
#include "user.hpp"
#include "connection.hpp"

namespace irc
{
	server_buffer::server_buffer(irc::model& aModel, irc::connection& aConnection, const irc::server& aServer) :
		buffer(aModel, SERVER, aConnection, aServer.name()), iServer(aServer)
	{
		update_title();
		connection().object_created(*this);
	}

	server_buffer::~server_buffer()
	{
		connection().object_destroyed(*this);
	}

	const user& server_buffer::user(const std::string& aUser) const
	{
		throw invalid_user();
	}

	user& server_buffer::user(const std::string& aUser)
	{
		throw invalid_user();
	}

	const user& server_buffer::user(const irc::user& aUser) const
	{
		throw invalid_user();
	}

	user& server_buffer::user(const irc::user& aUser)
	{
		throw invalid_user();
	}

	bool server_buffer::on_close()
	{
		connection().close(true);
		return false;
	}

	void server_buffer::update_title()
	{
		set_title(iServer.name_and_address());
	}
}