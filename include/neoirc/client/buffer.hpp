// buffer.h
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

#ifndef IRC_CLIENT_BUFFER
#define IRC_CLIENT_BUFFER

#include <deque>
#include <neolib/timer.hpp>
#include <neoirc/common/string.hpp>
#include <neoirc/client/message.hpp>
#include <neoirc/client/model.hpp>

namespace irc
{
	class connection;
	class user;

	class buffer_observer
	{
		friend class buffer;
	private:
		virtual void buffer_message(buffer& aBuffer, const message& aMessage) = 0;
		virtual void buffer_message_updated(buffer& aBuffer, const message& aMessage) = 0;
		virtual void buffer_message_removed(buffer& aBuffer, const message& aMessage) = 0;
		virtual void buffer_message_failure(buffer& aBuffer, const message& aMessage) = 0;
		virtual void buffer_activate(buffer& aBuffer) = 0;
		virtual void buffer_reopen(buffer& aBuffer) = 0;
		virtual void buffer_open(buffer& aBuffer, const message& aMessage) = 0;
		virtual void buffer_closing(buffer& aBuffer) = 0;
		virtual bool is_weak_observer(buffer& aBuffer) = 0;
		virtual void buffer_name_changed(buffer& aBuffer, const std::string& aOldName) = 0;
		virtual void buffer_title_changed(buffer& aBuffer, const std::string& aOldTitle) = 0;
		virtual void buffer_ready_changed(buffer& aBuffer) = 0;
		virtual void buffer_scrollbacked(buffer& aBuffer) = 0;
		virtual void buffer_cleared(buffer& aBuffer) = 0;
		virtual void buffer_hide(buffer& aBuffer) = 0;
		virtual void buffer_show(buffer& aBuffer) = 0;
	public:
		enum notify_type { NotifyMessage, NotifyMessageUpdated, NotifyMessageRemoved, NotifyMessageFailure, NotifyActivate, NotifyReopen, NotifyOpen, NotifyClosing, NotifyIsWeakObserver, NotifyNameChange, NotifyTitleChange, NotifyReadyChange, NotifyScrollbacked, NotifyCleared, NotifyHide, NotifyShow };
	};

	class buffer : public neolib::observable<buffer_observer>, private model_observer
	{
		friend class connection;

	public:
		// types
		enum type_e
		{
			SERVER, CHANNEL, USER, NOTICE
		};
		typedef std::deque<message> message_list;
		struct invalid_user : public std::logic_error { invalid_user() : std::logic_error("irc::buffer::invalid_user") {} };
	public:
		// construction
		buffer(irc::model& aModel, type_e aType, irc::connection& aConnection, const std::string& aName, const std::string& aTitle = std::string());
		virtual ~buffer();

	public:
		// operations
		irc::model& model() const { return iModel; }
		type_e type() const { return iType; }
		model::id id() const { return iId; }
		const std::string& name() const { return iName; }
		void set_name(const std::string& aName);
		const std::string& title() const { return iTitle; }
		void set_title(const std::string& aTitle);
		bool is_channel() const;
		bool is_channel(const std::string& aName) const;
		const message_list& messages() const { return iMessages; }
		message_list& messages() { return iMessages; }
		bool closing() const { return iClosing; }
		bool is_ready() const { return iReady; }
		void set_ready(bool aReady);
		void new_message(const std::string& aMessage, bool aAsPrivmsg = false, bool aAll = false);
		void new_message(const message& aMessage);
		void echo(const std::string& aMessage, bool aAll = false);
		model::id next_message_id();
		bool send_message(const message& aMessage);
		bool find_message(model::id aMessageId, buffer*& aBuffer, message*& aMessage);
		void activate();
		void deactivate();
		void show();
		void hide();
		void reopen();
		void open(const message& aMessage);
		void close();
		irc::connection& connection() { return iConnection; }
		const irc::connection& connection() const { return iConnection; }
		irc::casemapping::type casemapping() const;
		operator irc::casemapping::type() const { return casemapping(); }
		virtual bool has_user(const std::string& aUser) const = 0;
		virtual const irc::user& user(const std::string& aUser) const = 0;
		virtual irc::user& user(const std::string& aUser) = 0;
		virtual bool has_user(const irc::user& aUser) const = 0;
		virtual const irc::user& user(const irc::user& aUser) const = 0;
		virtual irc::user& user(const irc::user& aUser) = 0;
		virtual std::vector<const irc::user*> find_users(const std::string& aSearchString) const { return std::vector<const irc::user*>(); }
		virtual void remove_observer(buffer_observer& aObserver);
		void scrollback(const message_list& aMessages);
		void clear();
		bool just_weak_observers();

	private:
		friend class connection_manager;
		friend class connection;
		// implementation
		void add_and_handle_message(const message& aMessage);
		void insert_channel_parameter(message& aMessage) const;
		void update_chantype_messages();
	protected:
		// implementation
		virtual bool on_close() { return true; } // should return true if buffer should be closed immediately, false otherwise
		virtual void on_set_ready() {}
		virtual bool add_message(const message& aMessage);
		virtual void handle_message(const message& aMessage);
		virtual void user_new_host_info(const irc::user& aUser) {}
		virtual void user_away_status_changed(const irc::user& aUser) {}
		// from neolib::observable<buffer_observer>
		virtual void notify_observer(buffer_observer& aObserver, buffer_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from model_observer
		virtual void buffer_size_changed(std::size_t aNewBufferSize);

	private:
		// attributes
		irc::model& iModel;
		type_e iType;
		model::id iId;
		irc::connection& iConnection;
		std::size_t iBufferSize;
		std::string iName;
		std::string iTitle;
		message_list iMessages;
		bool iClosing;
		bool iReady;
		struct delayed_command : public neolib::timer
		{
			buffer& iOwner;
			std::string iCommand;
			delayed_command(buffer& aOwner, const std::string& aCommand, unsigned long aDelay) : 
				neolib::timer(aOwner.iModel.io_task(), aDelay), 
				iOwner(aOwner), 
				iCommand(aCommand) 
			{ 
			}
			virtual void ready()
			{
				std::string command(iCommand);
				buffer& owner(iOwner);
				for (delayed_commands::iterator i = owner.iDelayedCommands.begin(); i != owner.iDelayedCommands.end(); ++i)
					if (&*i == this)
					{
						owner.iDelayedCommands.erase(i);
						break;
					}
				owner.new_message(command);
			}
		};
		typedef std::list<delayed_command> delayed_commands;
		delayed_commands iDelayedCommands;
	};

	inline irc::string make_string(const buffer& aBuffer, const std::string& s)
	{
		return make_string(aBuffer.casemapping(), s);
	}

	struct internal_command
	{
		enum command_e
		{
			UNKNOWN, 
			OPEN, LEAVE, MSG, QUERY, RAW, ACTION, IGNORE_USER, UNIGNORE_USER, 
			VERSION, CLIENTINFO, TIME, 	FINGER, SOURCE, USERINFO,
			SERVER, CTCP,CHAT,
			DNS,
			SHOWPINGS, HIDEPINGS,
			AUTOJOIN, AUTOREJOIN,
			ALL,
			CLEAR, HIDE, SHOW,
			DELAY,
			ECHO, XYZZY,
			FINDUSER,
			TIMER
		};
		const char* iString;
		command_e iCommand;
	};

	std::pair<internal_command::command_e, std::string> get_internal_command(const std::string& aCommand);
}

#endif //IRC_CLIENT_BUFFER