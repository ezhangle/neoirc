// gui.h
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

#ifndef IRC_CLIENT_GUI
#define IRC_CLIENT_GUI

#include <deque>
#include <neolib/vecarray.hpp>
#include <neolib/string_utils.hpp>

namespace irc
{
	class connection;
	class dcc_connection;
	class buffer;
	class macro;
	struct gui_notify_action_data;
	struct gui_download_file_data;
	struct gui_chat_data;
	struct gui_invite_data;

	class custom_command
	{
	public:
		typedef std::deque<neolib::ci_string> parameters_t;
	public:
		custom_command(const std::string& aCommand) : iCommand(aCommand)
		{
			neolib::vecarray<neolib::ci_string, 1> name;
			neolib::tokens(neolib::make_ci_string(iCommand), neolib::ci_string(" "), name, 1);
			if (!name.empty())
				iName = name[0];
		};
	public:
		const neolib::ci_string& name() const { return iName; }
		const parameters_t& parameters() const
		{
			if (iParameters.empty())
			{
				neolib::tokens(neolib::make_ci_string(iCommand), neolib::ci_string(" "), iParameters);
				iParameters.pop_front();
			}
			return iParameters;
		}
	private:
		std::string iCommand;
		neolib::ci_string iName;
		mutable parameters_t iParameters; 
	};

	class gui
	{
		friend class model;
	private:
		virtual void gui_enter_password(connection& aConnection, std::string& aPassword, bool& aCancelled) = 0;
		virtual void gui_query_disconnect(const connection& aConnection, bool& aDisconnect) = 0;
		virtual void gui_alternate_nickname(connection& aConnection, std::string& aNickName, bool& aCancel, bool& aCallBack) = 0;
		virtual void gui_open_buffer(buffer& aBuffer) = 0;
		virtual void gui_open_dcc_connection(dcc_connection& aConnection) = 0;
		virtual void gui_open_channel_list(connection& aConnection) = 0;
		virtual void gui_notify_action(gui_notify_action_data& aNotifyActionData) = 0;
		virtual void gui_is_unicode(const buffer& aBuffer, bool& aIsUnicode) = 0;
		virtual void gui_is_unicode(const dcc_connection& aConnection, bool& aIsUnicode) = 0;
		virtual void gui_download_file(gui_download_file_data& aDownloadFileData) = 0;
		virtual void gui_chat(gui_chat_data& aChatData) = 0;
		virtual void gui_invite(gui_invite_data& aInviteData) = 0;
		virtual void gui_macro_syntax_error(const macro& aMacro, std::size_t aLineNumber, int aError) = 0;
		virtual void gui_can_activate_buffer(const buffer& aBuffer, bool& aCanActivate) = 0;
		virtual void gui_custom_command(const custom_command& aCommand, bool& aHandled) = 0;

	public:
		enum notify_type { NotifyEnterPassword, NotifyQueryDisconnect, NotifyAlternateNickName, 
			NotifyOpenBuffer, NotifyOpenDccConnection, NotifyOpenChannelList, NotifyNotifyAction, 
			NotifyIsBufferUnicode, NotifyIsDccConnectionUnicode, NotifyDownloadFile, NotifyChat, 
			NotifyInvite, NotifyMacroSyntaxError, NotifyCanActivateBuffer, NotifyCustomCommand };
	};
}

#endif // IRC_CLIENT_GUI
