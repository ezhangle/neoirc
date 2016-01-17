// model.cpp
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

#include <boost/chrono.hpp>

#include "Model.hpp"

#include "connection_manager.hpp"
#include "dcc_connection_manager.hpp"
#include "logger.hpp"
#include "connection.hpp"
#include "identity.hpp"
#include "server.hpp"
#include "server_updater.hpp"
#include "identd.hpp"
#include "auto_joins.hpp"
#include "auto_join_watcher.hpp"
#include "contacts.hpp"
#include "connection_script.hpp"
#include "connection_script_watcher.hpp"
#include "ignore.hpp"
#include "notify.hpp"
#include "notify_watcher.hpp"
#include "auto_mode.hpp"
#include "auto_mode_watcher.hpp"
#include "macros.hpp"
#include "gui_data.hpp"

namespace irc
{
	class model_impl : private identities_observer,
		private connection_manager_observer, private connection_observer,
		private dcc_connection_manager_observer,
		private auto_joins_observer, private contacts_observer, private connection_scripts_observer,
		private ignore_list_observer, 
		private notify_list_observer, private notify_watcher_observer, private auto_mode_list_observer,
		private macros_observer
	{
		friend class model;

	public:
		// construction
		model_impl(model& aModel);
		~model_impl();

	private:
		// implementation
		// from identities_observer
		virtual void identity_added(const identity& aEntry);
		virtual void identity_updated(const identity& aEntry, const identity& aOldEntry);
		virtual void identity_removed(const identity& aEntry);
		// from connection_manager_observer
		virtual void connection_added(connection& aConnection);
		virtual void connection_removed(connection& aConnection);
		virtual void filter_message(connection& aConnection, const message& aMessage, bool& aFiltered) {}
		virtual bool query_disconnect(const connection& aConnection);
		virtual void query_nickname(connection& aConnection);
		virtual void disconnect_timeout_changed() {}
		virtual void retry_network_delay_changed() {}
		virtual void buffer_activated(buffer& aActiveBuffer) {}
		virtual void buffer_deactivated(buffer& aDeactivatedBuffer) {}
		// from connection_observer
		virtual void connection_connecting(connection& aConnection) {}
		virtual void connection_registered(connection& aConnection) {}
		virtual void buffer_added(buffer& aBuffer);
		virtual void buffer_removed(buffer& aBuffer);
		virtual void incoming_message(connection& aConnection, const message& aMessage);
		virtual void outgoing_message(connection& aConnection, const message& aMessage);
		virtual void connection_quitting(connection& aConnection) {}
		virtual void connection_disconnected(connection& aConnection) {}
		virtual void connection_giveup(connection& aConnection) {}
		// from dcc_connection_manager_observer
		virtual void dcc_connection_added(dcc_connection& aConnection);
		virtual void dcc_connection_removed(dcc_connection& aConnection) {}
		virtual void dcc_download_failure(dcc_connection& aConnection, const std::string& aErrorString) {}
		// from auto_joins_observer
		virtual void auto_join_added(const auto_join& aEntry);
		virtual void auto_join_updated(const auto_join& aEntry);
		virtual void auto_join_removed(const auto_join& aEntry);
		virtual void auto_join_cleared();
		virtual void auto_join_reset();
		// from connection_scripts_observer
		virtual void connection_script_added(const connection_script& aEntry);
		virtual void connection_script_updated(const connection_script& aEntry);
		virtual void connection_script_removed(const connection_script& aEntry);
		// from ignore_list_observer
		virtual void ignore_added(const ignore_entry& aEntry);
		virtual void ignore_updated(const ignore_entry& aEntry);
		virtual void ignore_removed(const ignore_entry& aEntry);
		// from notify_list_observer
		virtual void notify_added(const notify_entry& aEntry);
		virtual void notify_updated(const notify_entry& aEntry);
		virtual void notify_removed(const notify_entry& aEntry);
		// from notify_watcher_observer
		virtual notify_action_ptr notify_action(const buffer& aBuffer, const notify_entry& aEntry, const std::string& aNickName, const message& aMessage);
		// from auto_mode_list_observer
		virtual void auto_mode_added(const auto_mode_entry& aEntry);
		virtual void auto_mode_updated(const auto_mode_entry& aEntry);
		virtual void auto_mode_removed(const auto_mode_entry& aEntry);
		// from contacts_observer
		virtual void contact_added(const contact& aEntry);
		virtual void contact_updated(const contact& aEntry, const contact& aOldEntry);
		virtual void contact_removed(const contact& aEntry);
		// from macros_observer
		virtual void macro_added(const macro& aEntry);
		virtual void macro_updated(const macro& aEntry);
		virtual void macro_removed(const macro& aEntry);
		virtual void macro_syntax_error(const macro& aEntry, std::size_t aLineNumber, macro::error aError);

	private:
		// attributes
		model& iModel;
		neolib::random iRandom;
		identities iIdentities;
		identity iDefaultIdentity;
		server_list iServerList;
		server_list_updater iServerListUpdater;
		identd iIdentd;
		connection_scripts iConnectionScripts;
		auto_joins iAutoJoinList;
		ignore_list iIgnoreList;
		notify iNotifyList;
		auto_mode iAutoModeList;
		connection_manager iConnectionManager;
		contacts iContacts;
		dcc_connection_manager iDccConnectionManager;
		logger iLogger;
		connection_script_watcher iConnectionScriptWatcher;
		auto_join_watcher iAutoJoinWatcher;
		notify_watcher iNotifyWatcher;
		auto_mode_watcher iAutoModeWatcher;
		macros iMacros;
	};

	model_impl::model_impl(model& aModel) :
		iModel(aModel),
		iRandom(static_cast<uint32_t>(aModel.owner_thread().elapsed_ms())),
		iServerListUpdater(iModel),
		iIdentd(iModel),
		iConnectionScripts(iIdentities),
		iAutoJoinList(iIdentities),
		iNotifyList(iModel),
		iConnectionManager(iModel, iRandom, iIdentities, iServerList, iIdentd, iAutoJoinList, iConnectionScripts, iIgnoreList, iNotifyList, iAutoModeList),
		iContacts(iModel, iConnectionManager),
		iDccConnectionManager(iModel),
		iLogger(iModel, iConnectionManager, iDccConnectionManager),
		iConnectionScriptWatcher(iRandom, iConnectionManager, iIdentities.identity_list(), iServerList), 
		iAutoJoinWatcher(iRandom, iConnectionManager, iIdentities.identity_list(), iServerList), 
		iNotifyWatcher(iConnectionManager), 
		iAutoModeWatcher(iConnectionManager)
	{
		iIdentities.add_observer(*this);
		iConnectionManager.add_observer(*this);
		iDccConnectionManager.add_observer(*this);
		iConnectionScripts.add_observer(*this);
		iAutoJoinList.add_observer(*this);
		iContacts.add_observer(*this);
		iIgnoreList.add_observer(*this);
		iNotifyList.add_observer(*this);
		iAutoModeList.add_observer(*this);
		iNotifyWatcher.add_observer(*this);
		iMacros.add_observer(*this);
	}

	model_impl::~model_impl()
	{
		iIdentities.remove_observer(*this);
		iConnectionManager.remove_observer(*this);
		iDccConnectionManager.remove_observer(*this);
		iConnectionScripts.remove_observer(*this);
		iAutoJoinList.remove_observer(*this);
		iContacts.remove_observer(*this);
		iIgnoreList.remove_observer(*this);
		iNotifyList.remove_observer(*this);
		iAutoModeList.remove_observer(*this);
		iNotifyWatcher.remove_observer(*this);
		iMacros.remove_observer(*this);
	}

	void model_impl::identity_added(const identity& aEntry)
	{
		write_identity_list(iModel, iDefaultIdentity.nick_name());	
	}

	void model_impl::identity_updated(const identity& aEntry, const identity& aOldEntry)
	{
		write_identity_list(iModel, iDefaultIdentity.nick_name());	
	}

	void model_impl::identity_removed(const identity& aEntry)
	{
		write_identity_list(iModel, iDefaultIdentity.nick_name());	
	}

	void model_impl::connection_added(connection& aConnection)
	{
		aConnection.add_observer(*this);
	}

	void model_impl::connection_removed(connection& aConnection)
	{
	}

	bool model_impl::query_disconnect(const connection& aConnection)
	{
		bool disconnect = false;
		iModel.neolib::observable<gui>::notify_observers(gui::NotifyQueryDisconnect, aConnection, disconnect);
		return disconnect;
	}

	void model_impl::query_nickname(connection& aConnection)
	{
		std::pair<std::string, std::pair<bool, bool> > param2 = std::make_pair(std::string(""), std::make_pair(false, false));
		iModel.neolib::observable<gui>::notify_observers(gui::NotifyAlternateNickName, aConnection, param2);
		std::string& nickName = param2.first;
		bool& cancel = param2.second.first;
		bool& callback = param2.second.second;

		if (cancel || (!callback && nickName.empty()))
		{
			aConnection.close();
			return;
		}

		if (!callback)
			aConnection.set_nick_name(nickName);
	}

	void model_impl::buffer_added(buffer& aBuffer)
	{
		iModel.iNewBuffer = &aBuffer;
		iModel.neolib::observable<gui>::notify_observers(gui::NotifyOpenBuffer, aBuffer);
		iModel.iNewBuffer = 0;
	}

	void model_impl::buffer_removed(buffer& aBuffer)
	{
	}

	void model_impl::incoming_message(connection& aConnection, const message& aMessage)
	{
		if (aMessage.command() == message::INVITE &&
			!iConnectionManager.ignore_list().ignored(aConnection, aConnection.server(), user(aMessage.origin(), aConnection)) &&
			aMessage.parameters().size() >= 2)
		{
			if (iModel.auto_join_watcher().join_pending(aConnection, aMessage.parameters()[1]))
				return;
			user theUser(aMessage.origin(), aConnection);
			gui_invite_data invite(aConnection, theUser, aMessage.parameters()[1]);
			iModel.neolib::observable<gui>::notify_observers(gui::NotifyInvite, invite);
			return;
		}
		if (aMessage.command() != message::PRIVMSG ||
			neolib::to_upper(aMessage.content()).find("\001DCC") != 0)
			return;
		if (aConnection.connection_manager().ignore_list().ignored(aConnection, aConnection.server(), user(aMessage.origin(), aConnection)))
			return;
		typedef std::vector<std::pair<std::string::const_iterator, std::string::const_iterator> > bits_t;
		bits_t bits;
		neolib::tokens(aMessage.content(), std::string("\001 "), bits);
		if (neolib::to_upper(neolib::to_string(bits[0])) != "DCC")
			return;
		if (neolib::to_upper(neolib::to_string(bits[1])) == "SEND" && bits.size() >= 6)
		{
			if (*bits[2].first == '\"')
			{
				for (bits_t::size_type i = 3; i < bits.size(); ++i)
					if (*(bits[i].second - 1) == '\"')
					{
						bits[2].second = bits[i].second;
						bits.erase(bits.begin() + 3, bits.begin() + i + 1);
						break;
					}
			}
			user theUser(aMessage.origin(), aConnection);
			gui_download_file_data download(aConnection, theUser, neolib::to_string(bits[2]), neolib::string_to_unsigned_integer(neolib::to_string(bits[5])), neolib::string_to_unsigned_integer(neolib::to_string(bits[3])), static_cast<u_short>(neolib::string_to_unsigned_integer(neolib::to_string(bits[4]))));
			if (!download.iFileName.empty())
				iModel.neolib::observable<gui>::notify_observers(gui::NotifyDownloadFile, download);
		}
		if (neolib::to_upper(neolib::to_string(bits[1])) == "CHAT" && bits.size() >= 5)
		{
			user theUser(aMessage.origin(), aConnection);
			gui_chat_data chat(aConnection, theUser, neolib::string_to_unsigned_integer(neolib::to_string(bits[3])), static_cast<u_short>(neolib::string_to_unsigned_integer(neolib::to_string(bits[4]))));
			iModel.neolib::observable<gui>::notify_observers(gui::NotifyChat, chat);
		}
	}

	void model_impl::outgoing_message(connection& aConnection, const message& aMessage)
	{
		if (aMessage.command() == message::LIST)
			iModel.neolib::observable<gui>::notify_observers(gui::NotifyOpenChannelList, aConnection);
	}

	void model_impl::dcc_connection_added(dcc_connection& aConnection)
	{
		if (aConnection.type() == dcc_connection::CHAT &&
			static_cast<dcc_chat_connection&>(aConnection).origin() != dcc_chat_connection::Listen)
		{
			iModel.iNewDccConnection = &aConnection;
			iModel.neolib::observable<gui>::notify_observers(gui::NotifyOpenDccConnection, aConnection);
			iModel.iNewDccConnection = 0;
		}
	}

	void model_impl::auto_join_added(const auto_join& aEntry)
	{
		write_auto_joins(iModel);
	}

	void model_impl::auto_join_updated(const auto_join& aEntry)
	{
		write_auto_joins(iModel);
	}

	void model_impl::auto_join_removed(const auto_join& aEntry)
	{
		write_auto_joins(iModel);
	}

	void model_impl::auto_join_cleared()
	{
		write_auto_joins(iModel);
	}

	void model_impl::auto_join_reset()
	{
		write_auto_joins(iModel);
	}

	void model_impl::connection_script_added(const connection_script& aEntry)
	{
		write_connection_scripts(iModel);
	}

	void model_impl::connection_script_updated(const connection_script& aEntry)
	{
		write_connection_scripts(iModel);
	}

	void model_impl::connection_script_removed(const connection_script& aEntry)
	{
		write_connection_scripts(iModel);
	}

	void model_impl::ignore_added(const ignore_entry& aEntry)
	{
		write_ignore_list(iModel);
	}

	void model_impl::ignore_updated(const ignore_entry& aEntry)
	{
		write_ignore_list(iModel);
	}

	void model_impl::ignore_removed(const ignore_entry& aEntry)
	{
		write_ignore_list(iModel);
	}

	void model_impl::notify_added(const notify_entry& aEntry)
	{
		write_notify_list(iModel);
	}

	void model_impl::notify_updated(const notify_entry& aEntry)
	{
		write_notify_list(iModel);
	}

	void model_impl::notify_removed(const notify_entry& aEntry)
	{
		write_notify_list(iModel);
	}

	void model_impl::auto_mode_added(const auto_mode_entry& aEntry)
	{
		write_auto_mode_list(iModel);
	}

	void model_impl::auto_mode_updated(const auto_mode_entry& aEntry)
	{
		write_auto_mode_list(iModel);
	}

	void model_impl::auto_mode_removed(const auto_mode_entry& aEntry)
	{
		write_auto_mode_list(iModel);
	}

	void model_impl::contact_added(const contact& aEntry)
	{
		write_contacts_list(iModel);
	}

	void model_impl::contact_updated(const contact& aEntry, const contact& aOldEntry)
	{
		write_contacts_list(iModel);
	}

	void model_impl::contact_removed(const contact& aEntry)
	{
		write_contacts_list(iModel);
	}

	void model_impl::macro_added(const macro& aEntry)
	{
		write_macros(iModel);
	}

	void model_impl::macro_updated(const macro& aEntry)
	{
		write_macros(iModel);
	}

	void model_impl::macro_removed(const macro& aEntry)
	{
		write_macros(iModel);
	}

	void model_impl::macro_syntax_error(const macro& aEntry, std::size_t aLineNumber, macro::error aError)
	{
		iModel.neolib::observable<gui>::notify_observers(gui::NotifyMacroSyntaxError, aEntry, std::make_pair(aLineNumber, aError));
	}

	notify_action_ptr model_impl::notify_action(const buffer& aBuffer, const notify_entry& aEntry, const std::string& aNickName, const message& aMessage)
	{
		gui_notify_action_data data(aBuffer, aEntry, aNickName, aMessage);
		iModel.neolib::observable<gui>::notify_observers(gui::NotifyNotifyAction, data);
		return data.iResult;
	}


	model::model(neolib::io_thread& aOwnerThread) : 
		iOwnerThread(aOwnerThread), iModelImpl(new model_impl(*this)), iNewBuffer(0), iNewDccConnection(0)
	{
	}

	model::~model()
	{
	}

	neolib::random& model::random()
	{
		return iModelImpl->iRandom;
	}

	identities& model::identities()
	{
		return iModelImpl->iIdentities;
	}

	const identities& model::identities() const
	{
		return iModelImpl->iIdentities;
	}

	identity_list& model::identity_list()
	{
		return identities().identity_list();
	}

	const identity_list& model::identity_list() const
	{
		return identities().identity_list();
	}

	const identity& model::default_identity() const
	{
		return iModelImpl->iDefaultIdentity;
	}

	void model::set_default_identity(const identity& aIdentity)
	{
		iModelImpl->iDefaultIdentity = aIdentity;
	}

	server_list& model::server_list()
	{
		return iModelImpl->iServerList;
	}

	const server_list& model::server_list() const
	{
		return iModelImpl->iServerList;
	}

	server_list_updater& model::server_list_updater()
	{
		return iModelImpl->iServerListUpdater;
	}

	identd& model::identd()
	{
		return iModelImpl->iIdentd;
	}

	auto_joins& model::auto_joins()
	{
		return iModelImpl->iAutoJoinList;
	}

	const auto_joins& model::auto_joins() const
	{
		return iModelImpl->iAutoJoinList;
	}

	contacts& model::contacts()
	{
		return iModelImpl->iContacts;
	}

	const contacts& model::contacts() const
	{
		return iModelImpl->iContacts;
	}

	connection_scripts& model::connection_scripts()
	{
		return iModelImpl->iConnectionScripts;
	}

	const connection_scripts& model::connection_scripts() const
	{
		return iModelImpl->iConnectionScripts;
	}

	ignore_list& model::ignore_list()
	{
		return iModelImpl->iIgnoreList;
	}

	const ignore_list& model::ignore_list() const
	{
		return iModelImpl->iIgnoreList;
	}

	notify& model::notify_list()
	{
		return iModelImpl->iNotifyList;
	}

	const notify& model::notify_list() const
	{
		return iModelImpl->iNotifyList;
	}

	auto_join_watcher& model::auto_join_watcher()
	{
		return iModelImpl->iAutoJoinWatcher;
	}

	connection_script_watcher& model::connection_script_watcher()
	{
		return iModelImpl->iConnectionScriptWatcher;
	}

	notify_watcher& model::notify_watcher()
	{
		return iModelImpl->iNotifyWatcher;
	}

	auto_mode& model::auto_mode_list()
	{
		return iModelImpl->iAutoModeList;
	}

	const  auto_mode& model::auto_mode_list() const
	{
		return iModelImpl->iAutoModeList;
	}

	connection_manager& model::connection_manager()
	{
		return iModelImpl->iConnectionManager;
	}

	const connection_manager& model::connection_manager() const
	{
		return iModelImpl->iConnectionManager;
	}

	dcc_connection_manager& model::dcc_connection_manager()
	{
		return iModelImpl->iDccConnectionManager;
	}

	const dcc_connection_manager& model::dcc_connection_manager() const
	{
		return iModelImpl->iDccConnectionManager;
	}

	logger& model::logger()
	{
		return iModelImpl->iLogger;
	}

	macros& model::macros()
	{
		return iModelImpl->iMacros;
	}

	const macros& model::macros() const
	{
		return iModelImpl->iMacros;
	}

	std::string model::sErrorMessageString;
	char model::sPathSeparator;

	std::string model::error_message(int aError, const std::string& aErrorString) const
	{
		std::string ret = sErrorMessageString;
		neolib::replace_string(ret, std::string("%E%"), aErrorString);
		neolib::replace_string(ret, std::string("%C%"), neolib::integer_to_string<char>(aError));
		return ret;
	}

	void model::set_buffer_size(std::size_t aBufferSize)
	{
		iBufferSize = aBufferSize;
		neolib::observable<model_observer>::notify_observers(model_observer::NotifyBufferSizeChanged);
	}

	bool model::is_unicode(const buffer& aBuffer)
	{
		bool isUnicode = false;
		neolib::observable<gui>::notify_observers(gui::NotifyIsBufferUnicode, aBuffer, isUnicode);
		return isUnicode;
	}

	bool model::is_unicode(const dcc_connection& aConnection)
	{
		bool isUnicode = false;
		neolib::observable<gui>::notify_observers(gui::NotifyIsDccConnectionUnicode, aConnection, isUnicode);
		return isUnicode;
	}

	bool model::can_activate_buffer(const buffer& aBuffer)
	{
		bool canActivate = true;
		neolib::observable<gui>::notify_observers(gui::NotifyCanActivateBuffer, aBuffer, canActivate);
		return canActivate;
	}

	bool model::do_custom_command(const std::string& aCommand)
	{
		bool handled = false;
		custom_command theCommand(aCommand);
		neolib::observable<gui>::notify_observers(gui::NotifyCustomCommand, theCommand, handled);
		return handled;
	}

	bool model::enter_password(irc::connection& aConnection)
	{
		identity::passwords_t::const_iterator password = aConnection.identity().passwords().find(aConnection.server().key());
		if (password != aConnection.identity().passwords().end())
			aConnection.set_password(password->second);
		else if ((password = aConnection.identity().passwords().find(std::make_pair(aConnection.server().key().first, "*")))
			!= aConnection.identity().passwords().end())
			aConnection.set_password(password->second);
		else
		{
			std::pair<std::string, bool> result;
			neolib::observable<gui>::notify_observers(gui::NotifyEnterPassword, aConnection, result);
			if (result.second)
				return false;
			if (!result.first.empty())
				aConnection.set_password(result.first);
		}
		return true;
	}

	void model::reopen_buffer(buffer& aBuffer)
	{
		iNewBuffer = &aBuffer;
		neolib::observable<gui>::notify_observers(gui::NotifyOpenBuffer, aBuffer);
		iNewBuffer = 0;
	}

	void model::reopen_connection(dcc_connection& aConnection)
	{
		if (aConnection.type() != dcc_connection::CHAT ||
			static_cast<dcc_chat_connection&>(aConnection).origin() != dcc_chat_connection::Listen)
		{
			iNewDccConnection = &aConnection;
			neolib::observable<gui>::notify_observers(gui::NotifyOpenDccConnection, aConnection);
			iNewDccConnection = 0;
		}
	}

	void model::notify_observer(model_observer& aObserver, model_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case model_observer::NotifyBufferSizeChanged:
			aObserver.buffer_size_changed(iBufferSize);
			break;
		}
	}

	void model::notify_observer(gui& aObserver, gui::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch (aType)
		{
		case gui::NotifyEnterPassword:
			{
				typedef std::pair<std::string, bool> result_t;
				aObserver.gui_enter_password(*static_cast<connection*>(const_cast<void*>(aParameter)), 
					static_cast<result_t*>(const_cast<void*>(aParameter2))->first, static_cast<result_t*>(const_cast<void*>(aParameter2))->second);
			}
			break;
		case gui::NotifyQueryDisconnect:
			aObserver.gui_query_disconnect(*static_cast<const connection*>(aParameter), *static_cast<bool*>(const_cast<void*>(aParameter2)));
			break;
		case gui::NotifyAlternateNickName:
			{
				typedef std::pair<std::string, std::pair<bool, bool> > param2_t;
				param2_t& p2 = *static_cast<param2_t*>(const_cast<void*>(aParameter2));
				aObserver.gui_alternate_nickname(*static_cast<connection*>(const_cast<void*>(aParameter)), p2.first, p2.second.first, p2.second.second);
			}
			break;
		case gui::NotifyOpenBuffer:
			aObserver.gui_open_buffer(*static_cast<buffer*>(const_cast<void*>(aParameter)));
			break;
		case gui::NotifyOpenDccConnection:
			aObserver.gui_open_dcc_connection(*static_cast<dcc_connection*>(const_cast<void*>(aParameter)));
			break;
		case gui::NotifyOpenChannelList:
			aObserver.gui_open_channel_list(*static_cast<connection*>(const_cast<void*>(aParameter)));
			break;
		case gui::NotifyNotifyAction:
			aObserver.gui_notify_action(*static_cast<gui_notify_action_data*>(const_cast<void*>(aParameter)));
			break;
		case gui::NotifyIsBufferUnicode:
			aObserver.gui_is_unicode(*static_cast<const buffer*>(aParameter), *static_cast<bool*>(const_cast<void*>(aParameter2)));
			break;
		case gui::NotifyIsDccConnectionUnicode:
			aObserver.gui_is_unicode(*static_cast<const dcc_connection*>(aParameter), *static_cast<bool*>(const_cast<void*>(aParameter2)));
			break;
		case gui::NotifyDownloadFile:
			{
				gui_download_file_data& download = *static_cast<gui_download_file_data*>(const_cast<void*>(aParameter));
				aObserver.gui_download_file(download);
			}
			break;
		case gui::NotifyChat:
			{
				gui_chat_data& download = *static_cast<gui_chat_data*>(const_cast<void*>(aParameter));
				aObserver.gui_chat(download);
			}
			break;
		case gui::NotifyInvite:
			{
				gui_invite_data& invite = *static_cast<gui_invite_data*>(const_cast<void*>(aParameter));
				aObserver.gui_invite(invite);
			}
			break;
		case gui::NotifyMacroSyntaxError:
			aObserver.gui_macro_syntax_error(*static_cast<const macro*>(aParameter), 
				static_cast<std::pair<std::size_t, macro::error>*>(const_cast<void*>(aParameter2))->first, 
				static_cast<std::pair<std::size_t, macro::error>*>(const_cast<void*>(aParameter2))->second);
			break;
		case gui::NotifyCanActivateBuffer:
			aObserver.gui_can_activate_buffer(*static_cast<const buffer*>(aParameter), *static_cast<bool*>(const_cast<void*>(aParameter2)));
			break;
		case gui::NotifyCustomCommand:
			aObserver.gui_custom_command(*static_cast<const custom_command*>(aParameter), *static_cast<bool*>(const_cast<void*>(aParameter2)));
			break;
		}
	}
}