// irc_plugin.cpp : Defines the exported functions for the DLL application.

#include <neolib/neolib.hpp>
#include <boost/filesystem.hpp>
#include <neolib/os_version.hpp>
#include <neolib/file.hpp>
#include <neolib/string_utils.hpp>

#include <QCoreApplication>

#include <caw/gui_buffer_info.hpp>
#include "revision.hpp"

#include "connection_manager.hpp"
#include "auto_joins.hpp"
#include "connection_script.hpp"
#include "ignore.hpp"
#include "notify.hpp"
#include "auto_mode.hpp"
#include "macros.hpp"
#include "contacts.hpp"
#include "auto_join_watcher.hpp"
#include "server_updater.hpp"

#include "buffer.hpp"

#include "IdentityDialog.hpp"
#include "ManageIdentitiesDialog.hpp"
#include "ManageServersDialog.hpp"
#include "ManageMacrosDialog.hpp"
#include "ConnectionPasswordDialog.hpp"
#include "InvalidNicknameDialog.hpp"
#include "ConnectToServerDialog.hpp"
#include "JoinChannelDialog.hpp"

#include "irc_plugin.hpp"

namespace caw_irc_plugin
{
	irc_plugin::irc_plugin(neolib::i_application& aApplication, const neolib::i_string& aPluginLocation) :
		neolib::io_thread("irc_plugin", true),
		iArgc(1),
		iArgv({ "irc_plugin", "" }),
		iQtHook(*this, iArgc, &iArgv[0]),
		iApplication(aApplication),
		iPluginManager(aApplication.plugin_manager()),
		iSettings(new settings(aApplication)),
		iSettingPresentationInfoProvider(static_cast<settings&>(*iSettings)),
		iFavourites(aApplication),
		iIrcBufferBeingOpened(0),
		iUpdater(*this, [this](neolib::timer_callback& aTimer)
		{
			aTimer.again();
			irc::buffer* activeBuffer = iModel->connection_manager().active_buffer();
			if (activeBuffer != 0)
			{
				iLatencyIndicator->set_text(neolib::string(activeBuffer->connection().latency_available() ? 
					"Latency: " + std::to_string(activeBuffer->connection().latency()) + " ms" : 
					"Latency: --"));
				iServerIndicator->set_text(neolib::string(activeBuffer->connection().server().name_and_address()));
				iNicknameIndicator->set_text(neolib::string(activeBuffer->connection().nick_name()));
			}
			else
			{
				iLatencyIndicator->set_text(neolib::string());
				iServerIndicator->set_text(neolib::string());
				iNicknameIndicator->set_text(neolib::string());
			}

		}, 40, true)
	{
		neolib::reference_counted<caw::i_favourite_types>::pin();
		neolib::reference_counted<caw::i_gui_resource_source>::pin();
		neolib::reference_counted<caw::i_chat_protocol>::pin();
		iBuffers.pin();
		iBuffers.subscribe(*this);
		iApplication.discover(iGui);
		iLatencyIndicator = iGui->add_status_indicator(*this);
		iServerIndicator = iGui->add_status_indicator(*this);
		iNicknameIndicator = iGui->add_status_indicator(*this);
		iSettings->subscribe(*this);
	}

	irc_plugin::~irc_plugin()
	{
		iSettings->unsubscribe(*this);
		iBuffers.unsubscribe(*this);
	}

	const neolib::i_version& irc_plugin::version() const
	{
		static const neolib::version sVersion(1, 0, 0, SCM_REVISION, "Orca");
		return sVersion;
	}

	bool irc_plugin::load()
	{
		if (loaded())
			return true;

		iGui->file_menu().add_action(new caw::gui_action(*this, std::bind(&irc_plugin::connect, this), 0, neolib::string("&Connect..."), neolib::string("Ctrl+N"), neolib::string("Connect to an IRC server"), neolib::string("Server Connect"), neolib::string(":/irc/Resources/connect_128.png")));
		neolib::auto_ref<caw::i_gui_action> joinAction(new caw::gui_action(*this, std::bind(&irc_plugin::join_channel, this), 0, neolib::string("&Join Channel..."), neolib::string("Ctrl+J"), neolib::string("Join an IRC channel"), neolib::string("Join Channel"), neolib::string(":/irc/Resources/channel_128.png")));
		static_cast<caw::gui_action*>(joinAction.ptr())->set_update_function([this](caw::i_gui_action& aAction)
		{
			irc::buffer* activeBuffer = iModel->connection_manager().active_buffer();
			aAction.set_enabled(activeBuffer != 0 && activeBuffer->connection().connected());
		});
		iGui->file_menu().add_action(joinAction.ptr());
		neolib::auto_ref<caw::i_gui_action> changeServerAction(new caw::gui_action(*this, std::bind(&irc_plugin::change_server, this), 0, neolib::string("Change &Server"), neolib::string("Ctrl+S"), neolib::string("Change to another server in the same network"), neolib::string("Change Server"), neolib::string(":/irc/Resources/change_server_128.png")));
		static_cast<caw::gui_action*>(changeServerAction.ptr())->set_update_function([this](caw::i_gui_action& aAction)
		{
			irc::buffer* activeBuffer = iModel->connection_manager().active_buffer();
			aAction.set_enabled(activeBuffer != 0 && !activeBuffer->connection().is_console());
		});
		iGui->file_menu().add_action(changeServerAction.ptr());
		neolib::auto_ref<caw::i_gui_action> disconnectAction(new caw::gui_action(*this, std::bind(&irc_plugin::disconnect, this), 0, neolib::string("&Disconnect"), neolib::string(), neolib::string("Disconnect from IRC server"), neolib::string("Server Disconnect"), neolib::string(":/irc/Resources/disconnect_128.png")));
		static_cast<caw::gui_action*>(disconnectAction.ptr())->set_update_function([this](caw::i_gui_action& aAction)
		{
			irc::buffer* activeBuffer = iModel->connection_manager().active_buffer();
			irc::dcc_connection* activeConnection = iModel->dcc_connection_manager().active_connection();
			aAction.set_enabled(activeBuffer != 0 || activeConnection != 0);
		});
		iGui->file_menu().add_action(disconnectAction.ptr());
		iGui->file_menu().add_action(new caw::gui_action(*this, std::bind(&irc_plugin::new_console, this), 0, neolib::string("&New Console"), neolib::string(), neolib::string("Open a new IRC console"), neolib::string("New Console"), neolib::string(":/irc/Resources/console_128.png")));
		iGui->shared_plugin_toolbar().add_action(new caw::gui_action(*this, std::bind(&irc_plugin::connect, this), 0, neolib::string("&Connect..."), neolib::string("Ctrl+N"), neolib::string("Connect to an IRC server"), neolib::string("Server Connect"), neolib::string(":/irc/Resources/connect_128.png")));
		iGui->shared_plugin_toolbar().add_action(joinAction.ptr());
		iGui->shared_plugin_toolbar().add_action(changeServerAction.ptr());
		iGui->shared_plugin_toolbar().add_action(disconnectAction.ptr());
		iGui->shared_plugin_toolbar().add_action(new caw::gui_action(*this, std::bind(&irc_plugin::new_console, this), 0, neolib::string("&New Console"), neolib::string(), neolib::string("Open a new IRC console"), neolib::string("New Console"), neolib::string(":/irc/Resources/console_128.png")));
		iGui->file_menu().add_action(caw::gui_action::separator(*this, 0));
		iGui->file_menu().add_action(new caw::gui_action(*this, std::bind(&irc_plugin::manage_identities, this), 0, neolib::string("&Identities..."), neolib::string(), neolib::string("Manage IRC identities"), neolib::string("Manage Identities"), neolib::string(":/irc/Resources/identity_64.png")));
		iGui->file_menu().add_action(new caw::gui_action(*this, std::bind(&irc_plugin::manage_servers, this), 0, neolib::string("&Servers..."), neolib::string(), neolib::string("Manage IRC servers"), neolib::string("Manage Servers"), neolib::string(":/irc/Resources/server.png")));
		iGui->tools_menu().add_action(new caw::gui_action(*this, std::bind(&irc_plugin::manage_macros, this), 0, neolib::string("&Macros..."), neolib::string(), neolib::string("Manage macros"), neolib::string("Manage Macros"), neolib::string(":/irc/Resources/macro.png")));

		iModel.reset(new irc::model(*this));
		iModel->neolib::observable<irc::gui>::add_observer(*this);
		iModel->contacts().add_observer(*this);
		iModel->set_root_path(iApplication.info().settings_folder().to_std_string() + "/");

		add_setting_observer("Connection", "IdentdEnabled", [this](const neolib::i_setting& aSetting) { iModel->identd().enable(aSetting.value().value_as_boolean()); });
		add_setting_observer("Connection", "IdentdType", [this](const neolib::i_setting& aSetting) { iModel->identd().set_type(static_cast<irc::identd::type_e>(aSetting.value().value_as_integer())); });
		add_setting_observer("Connection", "IdentdUserid", [this](const neolib::i_setting& aSetting) { iModel->identd().set_userid(aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Connection", "IpAddress", [this](const neolib::i_setting& aSetting) { iModel->dcc_connection_manager().set_ip_address(aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Connection", "AutomaticIpAddress", [this](const neolib::i_setting& aSetting) { iModel->dcc_connection_manager().set_automatic_ip_address(aSetting.value().value_as_boolean()); });
		add_setting_observer("Connection", "DccBasePort", [this](const neolib::i_setting& aSetting) { iModel->dcc_connection_manager().set_dcc_base_port(aSetting.value().value_as_integer()); });
		add_setting_observer("Connection", "DccFastSend", [this](const neolib::i_setting& aSetting) { iModel->dcc_connection_manager().set_dcc_fast_send(aSetting.value().value_as_boolean()); });
		add_setting_observer("Connection", "AutoReconnect", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_auto_reconnect(aSetting.value().value_as_boolean()); });
		add_setting_observer("Connection", "ReconnectServer", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_reconnect_any_server(aSetting.value().value_as_integer() == settings::ReconnectServerNetwork); });
		add_setting_observer("Connection", "ReconnectRetries", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_retry_count(aSetting.value().value_as_integer()); });
		add_setting_observer("Connection", "ReconnectNetworkDelay", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_retry_network_delay(aSetting.value().value_as_integer()); });
		add_setting_observer("Connection", "DisconnectTimeout", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_disconnect_timeout(aSetting.value().value_as_integer()); });

		irc::connection_manager::sConnectingToString = "Connecting to %S%...";
		irc::connection_manager::sConnectedToString = "Connected to %S%";
		irc::connection_manager::sTimedOutString = "Connection to %S% timed out";
		irc::connection_manager::sReconnectedToString = "Reconnected to %S%";
		irc::connection_manager::sWaitingForString = "Waiting for %T% second(s)...";
		irc::connection_manager::sDisconnectedString = "Disconnected from %S%";
		irc::connection_manager::sNoticeBufferName = "Notices";
		irc::connection_manager::sNoIpAddress = "Cannot determine IP address, using local IP address...";
		irc::dcc_connection_manager::sConnectingToString = "Connecting to user...";
		irc::dcc_connection_manager::sConnectedToString = "Connected to user";
		irc::dcc_connection_manager::sDisconnectedString = "DCC session closed";
		irc::model::sErrorMessageString = "Network error: %E%. Code: %C%";
		irc::model::sPathSeparator = boost::filesystem::path("/").make_preferred().generic_string()[0];
		irc::connection_manager::sClientName = iApplication.info().name().to_std_string();
		irc::connection_manager::sClientVersion = neolib::to_string(iApplication.info().version());
		irc::connection_manager::sClientEnvironment = neolib::os_name();
		irc::connection::sConsolesTitle = "No Network";
		irc::connection::sConsoleTitle = "Console";
		irc::connection::sIgnoreAddedMessage = "%U% added to ignore list";
		irc::connection::sIgnoreUpdatedMessage = "%U% updated in ignore list";
		irc::connection::sIgnoreRemovedMessage = "%U% removed from ignore list";
		irc::logger::sConvertStartMessage = "Converting log file...";
		irc::logger::sConvertEndMessage = "Log file conversion complete.";
		iModel->message_strings().set_seconds_string("sec(s)");
		iModel->message_strings().set_minutes_string("min(s)");
		iModel->message_strings().set_hours_string("hour(s)");
		iModel->message_strings().set_days_string("day(s)");

		add_setting_observer("Miscellaneous", "UseNoticeBuffer", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_use_notice_buffer(aSetting.value().value_as_boolean()); });

		add_setting_observer("Logging", "LogFileDirectory", [this](const neolib::i_setting& aSetting) { iModel->logger().set_directory(neolib::convert_path(neolib::utf8_to_wide(aSetting.value().value_as_string().c_str()))); });
		add_setting_observer("Logging", "ServerLog", [this](const neolib::i_setting& aSetting) { iModel->logger().server_log() = aSetting.value().value_as_boolean(); });
		add_setting_observer("Logging", "ScrollbackLog", [this](const neolib::i_setting& aSetting) { iModel->logger().reloading() = aSetting.value().value_as_boolean(); });
		add_setting_observer("Logging", "ScrollbackLogSize", [this](const neolib::i_setting& aSetting) { iModel->logger().reload_size() = aSetting.value().value_as_integer(); });
		add_setting_observer("Logging", "LogArchive", [this](const neolib::i_setting& aSetting) { iModel->logger().archive() = aSetting.value().value_as_boolean(); });
		add_setting_observer("Logging", "LogArchiveSize", [this](const neolib::i_setting& aSetting) { iModel->logger().archive_size() = aSetting.value().value_as_integer(); });
		add_setting_observer("Logging", "LoggerEvents", [this](const neolib::i_setting& aSetting) { iModel->logger().events() = static_cast<irc::logger::events_e>(aSetting.value().value_as_integer()); });
		add_setting_observer("Logging", "LoggingEnabled", [this](const neolib::i_setting& aSetting) { iModel->logger().enable(aSetting.value().value_as_boolean()); });

		boost::system::error_code ec;
		boost::filesystem::copy_file(boost::filesystem::path(iApplication.info().application_folder().to_std_string() + "/servers.xml"),
			boost::filesystem::path(iModel->root_path() + "servers.xml"), boost::filesystem::copy_option::fail_if_exists, ec);
		boost::filesystem::copy_file(boost::filesystem::path(iApplication.info().application_folder().to_std_string() + "/macros.xml"),
			boost::filesystem::path(iModel->root_path() + "macros.xml"), boost::filesystem::copy_option::fail_if_exists, ec);
		boost::filesystem::copy_file(boost::filesystem::path(iApplication.info().application_folder().to_std_string() + "/themes.xml"),
			boost::filesystem::path(iModel->root_path() + "themes.xml"), boost::filesystem::copy_option::fail_if_exists, ec);

		std::string defaultIdentity;
		read_identity_list(*iModel, defaultIdentity, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The identities file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});
		for (irc::identity_list::iterator i = iModel->identity_list().begin(); i != iModel->identity_list().end();)
		{
			if (i->nick_name() == defaultIdentity)
			{
				iModel->set_default_identity(*i);
				break;
			}
			if (++i == iModel->identity_list().end())
			{
				iModel->set_default_identity(iModel->identity_list().front());
				write_identity_list(*iModel, iModel->default_identity().nick_name());
			}
		}

		read_server_list(*iModel, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The server list file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});
		iModel->server_list().sort();

		read_auto_joins(*iModel, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The auto join list file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});

		read_connection_scripts(*iModel, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The connection scripts file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});

		read_ignore_list(*iModel, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The ignore list file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});

		read_notify_list(*iModel, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The notify list file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});

		read_auto_mode_list(*iModel, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The auto mode list file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});

		read_macros(*iModel, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The macros file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});

		read_contacts_list(*iModel, []()->bool
		{
			return QMessageBox::warning(NULL, "Configuration File Error", "The contacts file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
		});

		add_setting_observer("Formatting", "BufferSize", [this](const neolib::i_setting& aSetting) { iModel->set_buffer_size(aSetting.value().value_as_integer()); });
		add_setting_observer("Formatting", "DisplayMode", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_mode(static_cast<irc::message_strings::mode_e>(aSetting.value().value_as_integer())); });
		add_setting_observer("Miscellaneous", "QuitMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_own_quit_message(aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ReplyMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_reply_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "TopicMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_topic_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "TopicChangeMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_topic_change_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "TopicAuthorMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_topic_author_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "JoinMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_join_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "SelfJoinMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_join_self_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "PartMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_part_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "QuitMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_quit_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "KickMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_kick_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "SelfKickMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_kick_self_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "NicknameMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_nick_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "SelfNicknameMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_nick_self_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "NormalMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_standard_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ActionMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_action_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "NoticeMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_notice_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "SentNoticeMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_sent_notice_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ModeMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_mode_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "CtcpMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_ctcp_message(irc::message_strings::Normal, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnReplyMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_reply_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnTopicMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_topic_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnTopicChangeMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_topic_change_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnTopicAuthorMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_topic_author_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnJoinMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_join_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnSelfJoinMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_join_self_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnPartMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_part_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnQuitMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_quit_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnKickMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_kick_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnSelfKickMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_kick_self_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnNicknameMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_nick_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnSelfNicknameMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_nick_self_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnNormalMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_standard_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnActionMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_action_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnNoticeMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_notice_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnSentNoticeMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_sent_notice_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnModeMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_mode_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Messages", "ColumnCtcpMessage", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_ctcp_message(irc::message_strings::Column, aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Formatting", "DisplayTimestamps", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_display_timestamps(aSetting.value().value_as_boolean()); });
		add_setting_observer("Formatting", "TimestampFormat", [this](const neolib::i_setting& aSetting) { iModel->message_strings().set_timestamp_format(aSetting.value().value_as_string().to_std_string()); });
		add_setting_observer("Miscellaneous", "FloodPrevention", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_flood_prevention(aSetting.value().value_as_boolean()); });
		add_setting_observer("Miscellaneous", "FloodPreventionDelay", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_flood_prevention_delay(aSetting.value().value_as_integer()); });
		add_setting_observer("Miscellaneous", "UseNoticeBuffer", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_use_notice_buffer(aSetting.value().value_as_boolean()); });
		add_setting_observer("Miscellaneous", "UserListUpdate", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_auto_who(aSetting.value().value_as_boolean()); });
		add_setting_observer("Miscellaneous", "UserAwayUpdate", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_away_update(aSetting.value().value_as_boolean()); });
		add_setting_observer("Windows", "OpenChannelWindowImmediately", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_create_channel_buffer_upfront(aSetting.value().value_as_boolean()); });
		add_setting_observer("Miscellaneous", "AutoRejoinOnKick", [this](const neolib::i_setting& aSetting) { iModel->connection_manager().set_auto_rejoin_on_kick(aSetting.value().value_as_boolean()); });

		if (iSettings->find_setting(neolib::string("Connection"), neolib::string("AutoConnectOnStartup")).value().value_as_boolean())
			iModel->auto_join_watcher().startup_connect();

		iModel->server_list_updater().check_for_updates(iSettings->find_setting(neolib::string("Miscellaneous"), neolib::string("CheckForIRCServerListUpdates")).value().value_as_boolean());

		iFavouriteRequester.reset(new favourite_requester(iRandom, iModel->connection_manager(), iModel->identities(), iModel->server_list(), *iFavourites));

		iModel->connection_manager().add_observer(*this);
			
		return true;
	}

	bool irc_plugin::unload()
	{
		for (irc::connection_manager::connection_list::iterator i = iModel->connection_manager().connections().begin(); i != iModel->connection_manager().connections().end();)
		{
			irc::connection& theConnection = **i++;
			if (!theConnection.registered())
				theConnection.close();
			else
			{
				theConnection.close(true);
			}
		}
		for (irc::dcc_connection_manager::dcc_connection_list::iterator i = iModel->dcc_connection_manager().dcc_connections().begin(); i != iModel->dcc_connection_manager().dcc_connections().end();)
		{
			irc::dcc_connection& theConnection = **i++;
			theConnection.close();
		}
		
		uint64_t start = neolib::thread::elapsed_ms();
		while (neolib::thread::elapsed_ms() - start < 10000 &&
			(!iModel->connection_manager().connections().empty() || !iModel->dcc_connection_manager().dcc_connections().empty()))
		{
			QCoreApplication::processEvents();
		}

		return true;
	}

	bool irc_plugin::open_uri(const neolib::i_string& aUri)
	{
		if (neolib::make_ci_string(aUri.to_std_string()).find("irc://") != 0)
			return false;

		neolib::vecarray<std::string, 2> parts;
		neolib::tokens(neolib::parse_url_escapes(aUri.to_std_string()), std::string("//"), parts, 2, true, true);
		if (parts.size() != 2)
			return false;
		std::string second = parts[1];
		parts.clear();
		neolib::tokens(second, std::string("/"), parts, 2);
		if (parts.empty())
			return false;

		std::string channel;
		if (parts.size() == 2)
		{
			channel = parts[1];
			if (!irc::message::is_channel(channel))
				channel = "#" + channel;
		}

		std::string first = parts[0];
		parts.clear();
		neolib::tokens(first, std::string(":"), parts, 2);
		if (parts.empty())
			return false;

		neolib::optional<u_short> port;
		neolib::optional<bool> secure;
		if (parts.size() == 2)
		{
			port = static_cast<u_short>(neolib::string_to_unsigned_integer(parts[1]));
			secure = false;
			if (parts[1][0] == '+')
				secure = true;
		}
		std::string address = parts[0];

		if (!have_default_identity(true))
			return false;

		irc::server* theServer = 0;
		std::string network;
		for (irc::server_list::iterator i = iModel->server_list().begin(); theServer == 0 && i != iModel->server_list().end(); ++i)
			if (neolib::make_ci_string(i->address()) == address)
			{
				network = i->network();
				if (!secure || *secure == i->secure())
					theServer = &*i;
			}

		if (theServer == 0)
		{
			iModel->server_list().push_back(irc::server());
			theServer = &iModel->server_list().back();
			theServer->set_address(address);
			theServer->set_network(network.empty() ? address : network);
			theServer->set_name(address);
			if (secure && *secure)
				theServer->set_name(theServer->name() + " (secure)");
			irc::server::port_list ports;
			if (port)
				ports.push_back(std::make_pair(*port, *port));
			else
				ports.push_back(std::make_pair(6667, 6667));
			theServer->set_ports(ports);
			if (secure)
				theServer->set_secure(*secure);
			iModel->server_list().sort();
			write_server_list(*iModel);
		}

		caw::favourite_item item(iFavouriteRequester->request_folder());
		item.properties()[neolib::string("nick_name")] = iModel->default_identity().nick_name();
		item.properties()[neolib::string("server_network")] = theServer->key().first;
		item.properties()[neolib::string("server_name")] = theServer->key().first;
		item.properties()[neolib::string("name")] = channel;
		item.properties()[neolib::string("type")] = boost::lexical_cast<std::string>(favourite_requester::Channel);
		if (port)
			item.properties()[neolib::string("server_port")] = boost::lexical_cast<std::string>(port);
		if (secure)
			item.properties()[neolib::string("server_secure")] = boost::lexical_cast<std::string>(secure);
		iFavouriteRequester->add_request(item);

		iGui->activate_main_window();
		return true;
	}

	void irc_plugin::add_setting_observer(const neolib::string& aSettingCategory, const neolib::string& aSettingName, std::function<void(const neolib::i_setting&)> aCallBack)
	{
		const neolib::i_setting& setting = iSettings->find_setting(aSettingCategory, aSettingName);
		iSettingObservers[&setting] = aCallBack;
		aCallBack(setting);
	}

	bool irc_plugin::have_default_identity(bool aCreateOneIfNone)
	{
		if (!iModel->identities().empty())
			return true;

		caw_irc_plugin::gui::IdentityDialog dialog(*iModel);
		if (dialog.exec() == QDialog::Accepted)
		{
			irc::identity id;
			bool temp;
			dialog.getIdentity(id, temp);
			iModel->set_default_identity(id);
			iModel->identities().add_item(id);
		}
		return true;
	}

	void irc_plugin::new_console()
	{
		if (have_default_identity(true))
			iModel->connection_manager().add_connection();
	}

	void irc_plugin::connect()
	{
		if (have_default_identity(true))
		{
			caw_irc_plugin::gui::ConnectToServerDialog dialog(iApplication, *iSettings, *iModel);
			dialog.exec();
		}
	}

	void irc_plugin::disconnect()
	{
		irc::buffer* activeBuffer = iModel->connection_manager().active_buffer();
		if (activeBuffer != 0)
		{
			activeBuffer->connection().close(true);
			return;
		}
		irc::dcc_connection* activeConnection = iModel->dcc_connection_manager().active_connection();
		if (activeConnection != 0)
		{
			activeConnection->close();
			return;
		}
	}

	void irc_plugin::join_channel()
	{
		irc::buffer* activeBuffer = iModel->connection_manager().active_buffer();
		caw_irc_plugin::gui::ChannelListDialog* channelListDialog = 0;
		if (activeBuffer != 0)
		{
			caw_irc_plugin::gui::JoinChannelDialog dialog(*iGui, *iModel);
			if (dialog.exec() == QDialog::Accepted && !dialog.channelName().empty())
			{
				irc::message theMessage(activeBuffer->connection(), irc::message::OUTGOING);
				theMessage.set_command(irc::message::JOIN);
				theMessage.parse_parameters("JOIN " + dialog.channelName() + (dialog.key().empty() ? "" : " " + dialog.key()));
				activeBuffer->connection().send_message(theMessage);
			}
			channelListDialog = dialog.channelListDialog();
		}
		if (channelListDialog != 0)
		{
			channelListDialog->activateWindow();
			channelListDialog->refresh();
		}
	}

	void irc_plugin::change_server()
	{
		irc::buffer* activeBuffer = iModel->connection_manager().active_buffer();
		if (activeBuffer != 0)
		{
			activeBuffer->connection().server_buffer().new_message("/server");
			return;
		}
	}

	void irc_plugin::manage_identities()
	{
		caw_irc_plugin::gui::ManageIdentitiesDialog dialog(*iModel);
		dialog.exec();
	}

	void irc_plugin::manage_servers()
	{
		caw_irc_plugin::gui::ManageServersDialog dialog(iApplication, *iSettings, *iModel);
		dialog.exec();
	}

	void irc_plugin::manage_macros()
	{
		caw_irc_plugin::gui::ManageMacrosDialog dialog(iApplication, *iModel);
		dialog.exec();
	}

	void irc_plugin::setting_changed(const neolib::i_setting& aSetting)
	{
		setting_observers::const_iterator observer = iSettingObservers.find(&aSetting);
		if (observer != iSettingObservers.end())
			observer->second(aSetting);
	}

	void irc_plugin::gui_enter_password(irc::connection& aConnection, std::string& aPassword, bool& aCancelled)
	{
		caw_irc_plugin::gui::ConnectionPasswordDialog dialog(*this, *iGui, *iSettings, aConnection.server().name(), aConnection.identity().nick_name());
		if (dialog.exec() == QDialog::Accepted)
		{
			aPassword = dialog.getPassword();
			if (dialog.rememberPassword())
			{
				irc::identity_list::iterator theIdentity = iModel->identities().find_item(aConnection.identity());
				if (theIdentity != iModel->identities().identity_list().end())
				{
					theIdentity->passwords()[std::make_pair(aConnection.server().key().first, "*")] = aPassword;
					iModel->identities().update_item(*theIdentity, *theIdentity);
				}
			}
		}
		else
			aCancelled = true;
	}

	void irc_plugin::gui_query_disconnect(const irc::connection& aConnection, bool& aDisconnect)
	{
		aDisconnect = (QMessageBox::question(0, "IRC Connection", (std::string("Disconnect from ") + aConnection.server().name_and_address() + "?").c_str(),
			QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes);
	}

	void irc_plugin::gui_alternate_nickname(irc::connection& aConnection, std::string& aNickName, bool& aCancel, bool& aCallBack)
	{
		new caw_irc_plugin::gui::InvalidNicknameDialog(aConnection);
		aCallBack = true;
	}

	void irc_plugin::gui_open_buffer(irc::buffer& aBuffer)
	{
		iIrcBufferBeingOpened = &aBuffer;
		new buffer(*this, *iGui, *iSettings, aBuffer, iBuffers);
		iIrcBufferBeingOpened = 0;
	}

	void irc_plugin::gui_open_dcc_connection(irc::dcc_connection& aConnection)
	{
	}

	void irc_plugin::gui_open_channel_list(irc::connection& aConnection)
	{
	}

	void irc_plugin::gui_notify_action(irc::gui_notify_action_data& aNotifyActionData)
	{
	}

	void irc_plugin::gui_is_unicode(const irc::buffer& aBuffer, bool& aIsUnicode)
	{
	}

	void irc_plugin::gui_is_unicode(const irc::dcc_connection& aConnection, bool& aIsUnicode)
	{
	}

	void irc_plugin::gui_download_file(irc::gui_download_file_data& aDownloadFileData)
	{
	}

	void irc_plugin::gui_chat(irc::gui_chat_data& aChatData)
	{
	}

	void irc_plugin::gui_invite(irc::gui_invite_data& aInviteData)
	{
	}

	void irc_plugin::gui_macro_syntax_error(const irc::macro& aMacro, std::size_t aLineNumber, int aError)
	{
		std::string message;
		switch (static_cast<irc::macro::error>(aError))
		{
		case irc::macro::syntax_error:
			message = "Syntax error in macro %M%, line %L%";
			break;
		case irc::macro::insufficient_parameters:
			message = "Insufficient parameters for macro %M%, line %L%";
			break;
		case irc::macro::not_found:
			message = "Parameter not found for macro %M%, line %L%";
			break;
		case irc::macro::too_deep:
			message = "Macro call too deep: check your macros that call other macros.";
			break;
		}
		std::string::size_type pos = message.find("%M%");
		if (pos != std::string::npos)
			message.replace(pos, 3, aMacro.name());
		pos = message.find("%L%");
		if (pos != std::string::npos)
			message.replace(pos, 3, neolib::unsigned_integer_to_string<char>(aLineNumber));
		QMessageBox::warning(0, "Macro Error", message.c_str(), QMessageBox::Ok);
	}

	void irc_plugin::gui_can_activate_buffer(const irc::buffer& aBuffer, bool& aCanActivate)
	{
		bool canActivate = iBufferMap.left.find(&aBuffer)->second->can_activate();
	}

	void irc_plugin::gui_custom_command(const irc::custom_command& aCommand, bool& aHandled)
	{
	}

	void irc_plugin::connection_added(irc::connection& aConnection)
	{
		aConnection.add_observer(*this);
	}

	void irc_plugin::connection_removed(irc::connection& aConnection)
	{
	}

	void irc_plugin::filter_message(irc::connection& aConnection, const irc::message& aMessage, bool& aFiltered)
	{
	}

	bool irc_plugin::query_disconnect(const irc::connection& aConnection)
	{
		return false;
	}

	void irc_plugin::query_nickname(irc::connection& aConnection)
	{
	}

	void irc_plugin::disconnect_timeout_changed()
	{
	}

	void irc_plugin::retry_network_delay_changed()
	{
	}

	void irc_plugin::buffer_activated(irc::buffer& aActiveBuffer)
	{
	}

	void irc_plugin::buffer_deactivated(irc::buffer& aDeactivatedBuffer)
	{
	}

	void irc_plugin::connection_connecting(irc::connection& aConnection)
	{
	}

	void irc_plugin::connection_registered(irc::connection& aConnection)
	{
	}

	void irc_plugin::buffer_added(irc::buffer& aBuffer)
	{
		aBuffer.add_observer(*this);
	}

	void irc_plugin::buffer_removed(irc::buffer& aBuffer)
	{
	}

	void irc_plugin::incoming_message(irc::connection& aConnection, const irc::message& aMessage)
	{
	}

	void irc_plugin::outgoing_message(irc::connection& aConnection, const irc::message& aMessage)
	{
	}

	void irc_plugin::connection_quitting(irc::connection& aConnection)
	{
	}

	void irc_plugin::connection_disconnected(irc::connection& aConnection)
	{
	}

	void irc_plugin::connection_giveup(irc::connection& aConnection)
	{
	}

	void irc_plugin::buffer_message(irc::buffer& aBuffer, const irc::message& aMessage)
	{
		if (aMessage.is_ctcp() && aMessage.direction() == irc::message::INCOMING && aMessage.command() == irc::message::PRIVMSG)
		{
			neolib::string path = iSettings->find_setting("Miscellaneous", "CtcpSoundFile").value().value_as_string();
			if (!path.empty())
				iGui->play_sound(path);
		}
	}

	void irc_plugin::buffer_message_updated(irc::buffer& aBuffer, const irc::message& aMessage)
	{
	}

	void irc_plugin::buffer_message_removed(irc::buffer& aBuffer, const irc::message& aMessage)
	{
	}

	void irc_plugin::buffer_message_failure(irc::buffer& aBuffer, const irc::message& aMessage)
	{
	}

	void irc_plugin::buffer_activate(irc::buffer& aBuffer)
	{
	}

	void irc_plugin::buffer_reopen(irc::buffer& aBuffer)
	{
	}

	void irc_plugin::buffer_open(irc::buffer& aBuffer, const irc::message& aMessage)
	{
	}

	void irc_plugin::buffer_closing(irc::buffer& aBuffer)
	{
	}

	bool irc_plugin::is_weak_observer(irc::buffer& aBuffer)
	{
		return true;
	}

	void irc_plugin::buffer_name_changed(irc::buffer& aBuffer, const std::string& aOldName)
	{
	}

	void irc_plugin::buffer_title_changed(irc::buffer& aBuffer, const std::string& aOldTitle)
	{
	}

	void irc_plugin::buffer_ready_changed(irc::buffer& aBuffer)
	{
	}

	void irc_plugin::buffer_reloaded(irc::buffer& aBuffer)
	{
	}

	void irc_plugin::buffer_cleared(irc::buffer& aBuffer)
	{
	}

	void irc_plugin::buffer_hide(irc::buffer& aBuffer)
	{
	}

	void irc_plugin::buffer_show(irc::buffer& aBuffer)
	{
	}

	void irc_plugin::contact_added(const irc::contact& aEntry)
	{
	}

	void irc_plugin::contact_updated(const irc::contact& aEntry, const irc::contact& aOldEntry)
	{
	}

	void irc_plugin::contact_removed(const irc::contact& aEntry)
	{
	}

	void irc_plugin::buffer_added(caw::i_buffer& aBuffer, const caw::i_gui_buffer_info& aGuiBufferInfo)
	{
		iBufferMap.left.insert(std::make_pair(iIrcBufferBeingOpened, &aBuffer));
	}

	void irc_plugin::buffer_removed(caw::i_buffer& aBuffer)
	{
		iBufferMap.right.erase(iBufferMap.right.find(&aBuffer));
	}

	namespace
	{
		enum EFavouriteType
		{
			FAVOURITE_TYPE_CHANNEL,
			FAVOURITE_TYPE_USER,
			FAVOURITE_TYPE_COUNT
		};

		class favourite_type : public caw::i_favourite_type
		{
		public:
			favourite_type(const neolib::uuid& aId, const neolib::string& aName, const neolib::string& aIconUri) : 
				iId(aId), iName(aName), iIconUri(aIconUri) {}
		public:
			virtual const neolib::uuid& id() const { return iId; }
			virtual const neolib::i_string& name() const { return iName; }
			virtual const neolib::i_string& icon_uri() const { return iIconUri; }
		private:
			neolib::uuid iId;
			neolib::string iName;
			neolib::string iIconUri;
		};
	}

	uint32_t irc_plugin::favourite_type_count() const
	{
		return FAVOURITE_TYPE_COUNT;
	}

	const caw::i_favourite_type& irc_plugin::favourite_type(uint32_t aIndex) const
	{
		static const caw_irc_plugin::favourite_type sFavouriteTypes[] =
		{
			caw_irc_plugin::favourite_type(neolib::make_uuid("54CB2F94-CED4-480B-943E-524B68FBC61C"), neolib::string("IRC Channel"), neolib::string(":/irc/Resources/channel_32.png")),
			caw_irc_plugin::favourite_type(neolib::make_uuid("8B805BFB-B5F1-4110-845C-A6F0B164814F"), neolib::string("IRC User"), neolib::string(":/irc/Resources/user_32.png"))
		};
		if (aIndex >= FAVOURITE_TYPE_COUNT)
			throw favourite_type_not_found();
		return sFavouriteTypes[aIndex];
	}

	void irc_plugin::new_favourite(const caw::i_favourite_type& aFavouriteType)
	{
	}

	void irc_plugin::edit_favourite(caw::i_favourite& aFavourite)
	{
	}

	void irc_plugin::open_favourite(const caw::i_favourite& aFavourite)
	{
	}
}

extern "C"
{
	__declspec(dllexport) void entry_point(neolib::i_application& aApplication, const neolib::i_string& aPluginLocation, neolib::i_plugin*& aPlugin)
	{
		aPlugin = new caw_irc_plugin::irc_plugin(aApplication, aPluginLocation);
	}
}