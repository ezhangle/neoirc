// gui_data.h
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

#ifndef IRC_CLIENT_GUI_DATA
#define IRC_CLIENT_GUI_DATA

#include <neoirc/client/notify.hpp>
#include <neoirc/client/notify_watcher.hpp>
#include <neoirc/client/message.hpp>
#include <neoirc/client/user.hpp>

namespace irc
{
	class connection;

	struct gui_notify_action_data
	{
		gui_notify_action_data(const buffer& aBuffer, const notify_entry& aEntry, const std::string& aNickName,
			const message& aMessage) : 
				iBuffer(aBuffer), iEntry(aEntry), iNickName(aNickName), iMessage(aMessage) {}
		const buffer& iBuffer;
		const notify_entry& iEntry;
		const std::string& iNickName;
		const message& iMessage;
		notify_action_ptr iResult;
	};

	struct gui_download_file_data
	{
		gui_download_file_data(connection& aConnection, user& aUser, const std::string& aFileName, unsigned long aFileSize, u_long aAddress, u_short aPort)
			: iConnection(aConnection), iUser(aUser), iFileName(aFileName), iFileSize(aFileSize), iAddress(aAddress), iPort(aPort) 
		{
		}
		gui_download_file_data(const gui_download_file_data& aOther)
			: iConnection(aOther.iConnection), iUser(aOther.iUser), iFileName(aOther.iFileName), iFileSize(aOther.iFileSize), iAddress(aOther.iAddress), iPort(aOther.iPort) {}
		connection& iConnection;
		user iUser;
		std::string iFileName;
		unsigned long iFileSize;
		u_long iAddress;
		u_short iPort;
	};

	struct gui_chat_data
	{
		gui_chat_data(connection& aConnection, user& aUser, u_long aAddress, u_short aPort)
			: iConnection(aConnection), iUser(aUser), iAddress(aAddress), iPort(aPort) {}
		gui_chat_data(const gui_chat_data& aOther)
			: iConnection(aOther.iConnection), iUser(aOther.iUser), iAddress(aOther.iAddress), iPort(aOther.iPort) {}
		connection& iConnection;
		user iUser;
		u_long iAddress;
		u_short iPort;
	};

	struct gui_invite_data
	{
		gui_invite_data(connection& aConnection, user& aUser, const std::string& aChannel)
			: iConnection(aConnection), iUser(aUser), iChannel(aChannel) {}
		gui_invite_data(const gui_invite_data& aOther)
			: iConnection(aOther.iConnection), iUser(aOther.iUser), iChannel(aOther.iChannel) {}
		connection& iConnection;
		user iUser;
		std::string iChannel;
	};
}

#endif // IRC_CLIENT_GUI_DATA
