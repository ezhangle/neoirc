#pragma once

// irc.plg.hpp : Defines the exported functions for the DLL application.

#include <neolib/neolib.hpp>
#include <boost/bimap.hpp>
#include <neolib/random.hpp>
#include <neolib/io_thread.hpp>
#include <neolib/vector.hpp>
#include <neolib/string.hpp>
#include <neolib/version.hpp>
#include <neolib/timer.hpp>
#include <neolib/reference_counted.hpp>
#include <neolib/i_application.hpp>
#include <neolib/settings.hpp>
#include <caw/qt/qt_hook.hpp>
#include <caw/qt/qt_utils.hpp>

#include <QResource>

#include "caw/i_gui.hpp"
#include "caw/i_gui_action_control.hpp"
#include "caw/gui_action.hpp"
#include "caw/i_gui_resource_source.hpp"
#include "caw/gui_resource.hpp"
#include "caw/i_gui_setting_presentation_info_provider.hpp"
#include "caw/buffer_source.hpp"
#include "caw/i_gui_status_indicator.hpp"
#include "caw/i_favourites.hpp"
#include "caw/i_chat_protocol.hpp"
#include "caw/i_favourite_types.hpp"

#include "settings.hpp"
#include <irc/client/model.hpp>
#include <irc/client/connection_manager.hpp>
#include <irc/client/connection.hpp>
#include <irc/client/buffer.hpp>
#include "contacts.hpp"
#include "favourite_requester.hpp"

namespace caw_irc_plugin
{
	class irc_plugin : 
		public neolib::reference_counted<neolib::i_plugin>, 
		private neolib::io_thread, 
		private neolib::i_settings::i_subscriber, 
		private irc::gui, 
		private irc::connection_manager_observer,
		private irc::connection_observer,
		private irc::buffer_observer,
		private irc::contacts_observer, 
		private caw::i_buffer_source::i_subscriber,
		private neolib::reference_counted<caw::i_favourite_types>,
		private neolib::reference_counted<caw::i_gui_resource_source>,
		private neolib::reference_counted<caw::i_chat_protocol>
	{
		// construction
	public:
		irc_plugin(neolib::i_application& aApplication, const neolib::i_string& aPluginLocation);
		~irc_plugin();

		// implementation
	public:
		// from neolib::io_thread
		virtual void task()
		{
		}
		// from neolib::i_discoverable
		virtual bool discover(const neolib::uuid& aId, void*& aObject)
		{
			if (aId == neolib::i_settings::id())
			{
				aObject = static_cast<neolib::i_settings*>(iSettings.ptr());
				return true;
			}
			else if (aId == caw::i_gui_setting_presentation_info_provider::id())
			{
				aObject = static_cast<caw::i_gui_setting_presentation_info_provider*>(iSettingPresentationInfoProvider.ptr());
				return true;
			}
			else if (aId == caw::i_gui_resource_source::id())
			{
				aObject = static_cast<caw::i_gui_resource_source*>(this);
				return true;
			}
			else if (aId == caw::i_buffer_source::id())
			{
				aObject = static_cast<caw::i_buffer_source*>(&iBuffers);
				return true;
			}
			else if (aId == caw::i_chat_protocol::id())
			{
				aObject = static_cast<caw::i_chat_protocol*>(this);
				return true;
			}
			else if (aId == caw::i_favourite_types::id())
			{
				aObject = static_cast<caw::i_favourite_types*>(this);
				return true;
			}
			return false;
		}
		// from neolib::i_plugin
		virtual const neolib::uuid& id() const
		{
			static const neolib::uuid sId = neolib::make_uuid("B7EE349A-D939-4317-A087-69558F01604E");
			return sId;
		}
		virtual const neolib::i_string& name() const
		{
			static const neolib::string sName = "irc";
			return sName;
		}
		virtual const neolib::i_string& description() const
		{
			static const neolib::string sDescription = "IRC Engine";
			return sDescription;
		}
		virtual const neolib::i_version& version() const;
		virtual const neolib::i_string& copyright() const
		{
			static const neolib::string sCopyright = "Copyright (c) 2015 i42 Software. All rights reserved.";
			return sCopyright;
		}
		virtual bool load();
		virtual bool unload();
		virtual bool loaded() const
		{
			return iModel.get() != 0;
		}
		virtual bool open_uri(const neolib::i_string& aUri);
		// from caw::i_gui_resource_source
		virtual caw::i_gui_resource* load_resource(const neolib::i_string& aUri) const
		{
			if (aUri.to_std_string() == "icon")
			{
				return caw::qt::load_resource("icon", ":/irc/Resources/irc.png").release();
			}
			else
			{
				return caw::qt::load_resource(aUri.c_str(), aUri.c_str()).release();
			}
		}
		// from caw::i_chat_protocol
		virtual const neolib::i_string& protocol() const
		{
			static const neolib::string sProtocol = "IRC";
			return sProtocol;
		}
		virtual const neolib::i_string& provider() const
		{
			static const neolib::string sProvider = "i42 Software";
			return sProvider;
		}

	private:
		void add_setting_observer(const neolib::string& aSettingCategory, const neolib::string& aSettingName, std::function<void(const neolib::i_setting&)> aCallBack);
		bool have_default_identity(bool aCreateOneIfNone);
		void manage_identities();
		void manage_servers();
		void manage_macros();
		void new_console();
		void connect();
		void disconnect();
		void join_channel();
		void change_server();

	private:
		// from neolib::i_settings::i_subscriber
		virtual void settings_changed(const neolib::i_string& aSettingCategory) {};
		virtual void setting_changed(const neolib::i_setting& aSetting);
		virtual void setting_deleted(const neolib::i_setting& aSetting) {}
		virtual bool interested_in_dirty_settings() const { return false; }
		// from irc::gui
		virtual void gui_enter_password(irc::connection& aConnection, std::string& aPassword, bool& aCancelled);
		virtual void gui_query_disconnect(const irc::connection& aConnection, bool& aDisconnect);
		virtual void gui_alternate_nickname(irc::connection& aConnection, std::string& aNickName, bool& aCancel, bool& aCallBack);
		virtual void gui_open_buffer(irc::buffer& aBuffer);
		virtual void gui_open_dcc_connection(irc::dcc_connection& aConnection);
		virtual void gui_open_channel_list(irc::connection& aConnection);
		virtual void gui_notify_action(irc::gui_notify_action_data& aNotifyActionData);
		virtual void gui_is_unicode(const irc::buffer& aBuffer, bool& aIsUnicode);
		virtual void gui_is_unicode(const irc::dcc_connection& aConnection, bool& aIsUnicode);
		virtual void gui_download_file(irc::gui_download_file_data& aDownloadFileData);
		virtual void gui_chat(irc::gui_chat_data& aChatData);
		virtual void gui_invite(irc::gui_invite_data& aInviteData);
		virtual void gui_macro_syntax_error(const irc::macro& aMacro, std::size_t aLineNumber, int aError);
		virtual void gui_can_activate_buffer(const irc::buffer& aBuffer, bool& aCanActivate);
		virtual void gui_custom_command(const irc::custom_command& aCommand, bool& aHandled);
		// from irc::connection_manager_observer
		virtual void connection_added(irc::connection& aConnection);
		virtual void connection_removed(irc::connection& aConnection);
		virtual void filter_message(irc::connection& aConnection, const irc::message& aMessage, bool& aFiltered);
		virtual bool query_disconnect(const irc::connection& aConnection);
		virtual void query_nickname(irc::connection& aConnection);
		virtual void disconnect_timeout_changed();
		virtual void retry_network_delay_changed();
		virtual void buffer_activated(irc::buffer& aActiveBuffer);
		virtual void buffer_deactivated(irc::buffer& aDeactivatedBuffer);
		// from irc::connection_observer
		virtual void connection_connecting(irc::connection& aConnection);
		virtual void connection_registered(irc::connection& aConnection);
		virtual void buffer_added(irc::buffer& aBuffer);
		virtual void buffer_removed(irc::buffer& aBuffer);
		virtual void incoming_message(irc::connection& aConnection, const irc::message& aMessage);
		virtual void outgoing_message(irc::connection& aConnection, const irc::message& aMessage);
		virtual void connection_quitting(irc::connection& aConnection);
		virtual void connection_disconnected(irc::connection& aConnection);
		virtual void connection_giveup(irc::connection& aConnection);
		// from irc::buffer_observer
		virtual void buffer_message(irc::buffer& aBuffer, const irc::message& aMessage);
		virtual void buffer_message_updated(irc::buffer& aBuffer, const irc::message& aMessage);
		virtual void buffer_message_removed(irc::buffer& aBuffer, const irc::message& aMessage);
		virtual void buffer_message_failure(irc::buffer& aBuffer, const irc::message& aMessage);
		virtual void buffer_activate(irc::buffer& aBuffer);
		virtual void buffer_reopen(irc::buffer& aBuffer);
		virtual void buffer_open(irc::buffer& aBuffer, const irc::message& aMessage);
		virtual void buffer_closing(irc::buffer& aBuffer);
		virtual bool is_weak_observer(irc::buffer& aBuffer);
		virtual void buffer_name_changed(irc::buffer& aBuffer, const std::string& aOldName);
		virtual void buffer_title_changed(irc::buffer& aBuffer, const std::string& aOldTitle);
		virtual void buffer_ready_changed(irc::buffer& aBuffer);
		virtual void buffer_reloaded(irc::buffer& aBuffer);
		virtual void buffer_cleared(irc::buffer& aBuffer);
		virtual void buffer_hide(irc::buffer& aBuffer);
		virtual void buffer_show(irc::buffer& aBuffer);
		// from irc::contacts_observer
		virtual void contact_added(const irc::contact& aEntry);
		virtual void contact_updated(const irc::contact& aEntry, const irc::contact& aOldEntry);
		virtual void contact_removed(const irc::contact& aEntry);
		// from caw::i_buffer_source::i_subscriber
		virtual void buffer_added(caw::i_buffer& aBuffer, const caw::i_gui_buffer_info& aGuiBufferInfo);
		virtual void buffer_removed(caw::i_buffer& aBuffer);
		// from caw::i_favourite_types
		virtual uint32_t favourite_type_count() const;
		virtual const caw::i_favourite_type& favourite_type(uint32_t aIndex) const;
		virtual void new_favourite(const caw::i_favourite_type& aFavouriteType);
		virtual void edit_favourite(caw::i_favourite& aFavourite);
		virtual void open_favourite(const caw::i_favourite& aFavourite);
	private:
		neolib::random iRandom;
		int iArgc;
		std::vector<char*> iArgv;
		caw::qt::hook iQtHook;
		neolib::i_application& iApplication;
		neolib::i_plugin_manager& iPluginManager;
		neolib::auto_ref<neolib::i_settings> iSettings;
		neolib::auto_ref<caw::i_gui_setting_presentation_info_provider> iSettingPresentationInfoProvider;
		caw::buffer_source iBuffers;
		std::unique_ptr<irc::model> iModel;
		neolib::auto_ref<caw::i_gui> iGui;
		typedef std::map<const neolib::i_setting*, std::function<void(const neolib::i_setting&)>> setting_observers;
		setting_observers iSettingObservers;
		neolib::auto_ref<caw::i_favourites> iFavourites;
		std::unique_ptr<favourite_requester> iFavouriteRequester;
		irc::buffer* iIrcBufferBeingOpened;
		typedef boost::bimap<const irc::buffer*, const caw::i_buffer*> buffer_map;
		buffer_map iBufferMap;
		neolib::timer_callback iUpdater;
		neolib::auto_ref<caw::i_gui_status_indicator> iLatencyIndicator;
		neolib::auto_ref<caw::i_gui_status_indicator> iServerIndicator;
		neolib::auto_ref<caw::i_gui_status_indicator> iNicknameIndicator;
	};
}