// settings.cpp
/*
 *  Copyright (c) 2010, 2014 Leigh Johnston.
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
#include <map>
#include "settings.hpp"
#include <caw/gui_setting_presentation_info.hpp>
#include <caw/gui_colour.hpp>
#include <caw/gui_colour_gradient.hpp>
#include <caw/i_buffer.hpp>

namespace caw_irc_plugin
{
	bool settings::has_presentation_info(const neolib::i_setting& aSetting) const
	{
		return iSettingPresentationInfo.find(aSetting.id()) != iSettingPresentationInfo.end();
	}

	const caw::i_gui_setting_presentation_info& settings::presentation_info(const neolib::i_setting& aSetting) const
	{
		setting_presentation_info_map_type::const_iterator iter = iSettingPresentationInfo.find(aSetting.id());
		if (iter == iSettingPresentationInfo.end())
			throw setting_presentation_info_not_found();
		return iter->second;
	}

	namespace
	{
		struct setting_info
		{
			neolib::string iCategory;
			neolib::string iName;
			neolib::i_simple_variant::type_e iType;
			neolib::simple_variant iDefault;
			caw::gui_setting_presentation_info iPresentationInfo;
			setting_info(const char* const aCategory, const char* const aName, neolib::i_simple_variant::type_e aType, const neolib::simple_variant& aDefault, const caw::gui_setting_presentation_info& aPresentationInfo) :
				iCategory(aCategory), iName(aName), iType(aType), iDefault(aDefault), iPresentationInfo(aPresentationInfo) {}
		};
		const setting_info sSettingInfo[] =
		{
			{ "Formatting", "CharacterEncoding", neolib::i_simple_variant::Integer, settings::Mixed,
				{ caw::gui_setting_presentation_info::ComboBox, "General", "Character encoding (default): ", caw::gui_setting_presentation_info::option_list::container_type
					{ { "ANSI", settings::ANSI }, { "Mixed", settings::Mixed }, { "UTF8", settings::UTF8 } } } },
			{ "Formatting", "ParagraphAlignment", neolib::i_simple_variant::Integer, settings::Left,
				{ caw::gui_setting_presentation_info::ComboBox, "General", "Paragraph alignment (default): ", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Left", settings::Left }, { "Centred", settings::Centred }, { "Right", settings::Right }, { "Justified", settings::Justified } } } },
			{ "Formatting", "BufferSize", neolib::i_simple_variant::Integer, 1000,
				{ caw::gui_setting_presentation_info::LineEdit, "General", "Buffer size (default): %w:width(\"0000\")% paragraphs", {}, true, 100, 9999 } },
			{ "Formatting", "CodeBehaviour", neolib::i_simple_variant::Integer, settings::Display,
				{ caw::gui_setting_presentation_info::ComboBox, "General", "Colour code handling behaviour (default): ", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Ignore", settings::Ignore }, { "Strip", settings::Strip }, { "Display", settings::Display } } } },
			{ "Formatting", "DisplayMode", neolib::i_simple_variant::Integer, irc::message_strings::Normal,
				{ caw::gui_setting_presentation_info::ComboBox, "General", "Display mode: ", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Normal", irc::message_strings::Normal }, { "Column", irc::message_strings::Column } } } },
			{ "Formatting", "ParagraphSpacingBefore", neolib::i_simple_variant::Real, 0.0,
				{ caw::gui_setting_presentation_info::ComboBox, "Paragraph", "Space before paragraph (default): %w% lines", caw::gui_setting_presentation_info::option_list::container_type
					{ { "0", 0.0 }, { "0.25", 0.25 }, { "0.5", 0.5 }, { "0.75", 0.75 }, { "1", 1.0 }, { "1.5", 1.5 }, { "2", 2.0 } }, true } },
			{ "Formatting", "ParagraphSpacingAfter", neolib::i_simple_variant::Real, 0.0,
				{ caw::gui_setting_presentation_info::ComboBox, "Paragraph", "Space after paragraph (default): %w% lines", caw::gui_setting_presentation_info::option_list::container_type
					{ { "0", 0.0 }, { "0.25", 0.25 }, { "0.5", 0.5 }, { "0.75", 0.75 }, { "1", 1.0 }, { "1.5", 1.5 }, { "2", 2.0 } }, true } },
			{ "Formatting", "ParagraphSameStyleNoSpace", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Paragraph", "No spacing between paragraphs of the same style (default)" } },
			{ "Formatting", "DisplayTimestamps", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Timestamps", "Display timestamps (default)" } },
			{ "Formatting", "TimestampFormat", neolib::i_simple_variant::String, "[%H%:%M%]",
				{ caw::gui_setting_presentation_info::LineEdit, "Timestamps", "Timestamp format (default): " } },
			{ "Formatting", "InputWordwrap", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Input Box", "Word wrap" } },
			{ "Formatting", "InputMin", neolib::i_simple_variant::Integer, 1,
				{ caw::gui_setting_presentation_info::LineEdit, "Input Box", "Minimum lines: %w:width(\"00\")%", {}, true, 1, 99 } },
			{ "Formatting", "InputMax", neolib::i_simple_variant::Integer, 2,
				{ caw::gui_setting_presentation_info::LineEdit, "Input Box", "Maximum lines: %w:width(\"00\")%", {}, true, 1, 99 } },
			{ "Formatting", "InputNewLineModifierKey", neolib::i_simple_variant::Integer, caw::i_buffer::i_input_info::ModifierKeyCtrl,
				{ caw::gui_setting_presentation_info::ComboBox, "Input Box", "New input line: ", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Ctrl+Return", caw::i_buffer::i_input_info::ModifierKeyCtrl }, { "Alt+Return", caw::i_buffer::i_input_info::ModifierKeyAlt } } } },
			{ "Formatting", "InputHistoryModifierKey", neolib::i_simple_variant::Integer, caw::i_buffer::i_input_info::ModifierKeyAlt,
				{ caw::gui_setting_presentation_info::ComboBox, "Input Box", "Input history navigation: ", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Ctrl+Up/Down", caw::i_buffer::i_input_info::ModifierKeyCtrl }, { "Alt+Up/Down", caw::i_buffer::i_input_info::ModifierKeyAlt } } } },
			{ "Formatting", "CrazyColours", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Crazy Colours", "Randomly colourise nicknames (default)" } },
			{ "Formatting", "CrazyColoursSeed", neolib::i_simple_variant::Integer, 0,
				{ caw::gui_setting_presentation_info::LineEdit, "Crazy Colours", "Random number seed (default): %w:width(\"00000\")%", {}, true, 0, 65535 } },
			{ "Formatting", "CrazyColoursKey", neolib::i_simple_variant::Integer, settings::NickName,
				{ caw::gui_setting_presentation_info::ComboBox, "Crazy Colours", "Key on: ", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Nickname", settings::NickName }, { "User Name", settings::UserName }, { "Host Name", settings::HostName } } } },
			{ "Formatting", "TimestampColumnMaxWidth", neolib::i_simple_variant::Integer, 25,
				{ caw::gui_setting_presentation_info::LineEdit, "Timestamp Column", "Maximum width: %w:width(\"0000\")%%s:id(\"Formatting\",\"TimestampColumnMaxWidthType\")%", {}, true, 0, 4096 } },
			{ "Formatting", "TimestampColumnMaxWidthType", neolib::i_simple_variant::Integer, settings::Percentage,
				{ caw::gui_setting_presentation_info::ComboBox, "Timestamp Column", "", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Percentage", settings::Percentage }, { "Pixels", settings::Pixels } } } },
			{ "Formatting", "TimestampColumnAlignment", neolib::i_simple_variant::Integer, settings::Right,
				{ caw::gui_setting_presentation_info::ComboBox, "Timestamp Column", "Alignment", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Left", settings::Left }, { "Centred", settings::Centred }, { "Right", settings::Right }, { "Justified", settings::Justified } } } },
			{ "Formatting", "TimestampColumnSeparatorLine", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Timestamp Column", "%w% Separator line, colour: %s:id(\"Formatting\",\"TimestampColumnSeparatorLineColour\")%" } },
			{ "Formatting", "TimestampColumnSeparatorLineColour", neolib::i_simple_variant::CustomType, neolib::auto_ref<neolib::i_custom_type>(new neolib::custom_type<caw::i_gui_colour_gradient, caw::gui_colour_gradient>("colour_gradient", caw::gui_colour_gradient("000000,0.0,FF0000,0.5,000000,1.0:0.0,0.0,1.0,0.5,0.0,1.0"))),
				{ caw::gui_setting_presentation_info::Custom, "Timestamp Column", "" } },
			{ "Formatting", "NicknameColumnMaxWidth", neolib::i_simple_variant::Integer, 25,
				{ caw::gui_setting_presentation_info::LineEdit, "Nickname Column", "Maximum width: %w:width(\"0000\")%%s:id(\"Formatting\",\"NicknameColumnMaxWidthType\")%", {}, true, 0, 4096 } },
			{ "Formatting", "NicknameColumnMaxWidthType", neolib::i_simple_variant::Integer, settings::Percentage,
				{ caw::gui_setting_presentation_info::ComboBox, "Nickname Column", "", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Percentage", settings::Percentage }, { "Pixels", settings::Pixels } } } },
			{ "Formatting", "NicknameColumnAlignment", neolib::i_simple_variant::Integer, settings::Right,
				{ caw::gui_setting_presentation_info::ComboBox, "Nickname Column", "Alignment", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Left", settings::Left }, { "Centred", settings::Centred }, { "Right", settings::Right }, { "Justified", settings::Justified } } } },
			{ "Formatting", "NicknameColumnSeparatorLine", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Nickname Column", "%w% Separator line, colour: %s:id(\"Formatting\",\"NicknameColumnSeparatorLineColour\")%" } },
			{ "Formatting", "NicknameColumnSeparatorLineColour", neolib::i_simple_variant::CustomType, neolib::auto_ref<neolib::i_custom_type>(new neolib::custom_type<caw::i_gui_colour_gradient, caw::gui_colour_gradient>("colour_gradient", caw::gui_colour_gradient("000000,0.0,0000FF,0.5,000000,1.0:0.0,0.0,1.0,0.5,0.0,1.0"))),
				{ caw::gui_setting_presentation_info::Custom, "Nickname Column", "" } },

			{ "Connection", "AutoConnectOnStartup", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "General", "Automatically connect to servers with auto-joins on startup" } },
			{ "Connection", "AutoReconnect", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Auto-reconnect", "Enable auto-reconnect" } },
			{ "Connection", "ReconnectServer", neolib::i_simple_variant::Integer, settings::ReconnectServerNetwork,
				{ caw::gui_setting_presentation_info::RadioButton, "Auto-reconnect", "", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Reconnect to any server in network", settings::ReconnectServerNetwork }, { "Reconnect to same server", settings::ReconnectServerSame } } } },
			{ "Connection", "ReconnectRetries", neolib::i_simple_variant::Integer, 3,
				{ caw::gui_setting_presentation_info::LineEdit, "Auto-reconnect", "Maximum number of retry attempts per server: %w:width(\"000\")%", {}, true, 1, 999 } },
			{ "Connection", "ReconnectNetworkDelay", neolib::i_simple_variant::Integer, 10,
				{ caw::gui_setting_presentation_info::LineEdit, "Auto-reconnect", "Network retry delay: %w:width(\"000\")% seconds", {}, true, 1, 999 } },
			{ "Connection", "DisconnectTimeout", neolib::i_simple_variant::Integer, 120,
				{ caw::gui_setting_presentation_info::LineEdit, "Auto-reconnect", "Disconnection timeout: %w:width(\"000\")% seconds", {}, true, 1, 999 } },
			{ "Connection", "IpAddress", neolib::i_simple_variant::String, "",
				{ caw::gui_setting_presentation_info::LineEdit, "Direct Client-to-Client", "Local IP address: %w:width(\"000.000.000.000\")%" } },
			{ "Connection", "AutomaticIpAddress", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "Direct Client-to-Client", "Get local IP address automatically" } },
			{ "Connection", "DccBasePort", neolib::i_simple_variant::Integer, 4200,
				{ caw::gui_setting_presentation_info::LineEdit, "Direct Client-to-Client", "DCC base port: %w:width(\"00000\")%", {}, true, 0, 65535 } },
			{ "Connection", "DccFastSend", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Direct Client-to-Client", "Enable \"Fast Send\" file transfers" } },
			{ "Connection", "IdentdEnabled", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "identd", "Enable identd" } },
			{ "Connection", "IdentdType", neolib::i_simple_variant::Integer, irc::identd::Nickname,
				{ caw::gui_setting_presentation_info::RadioButton, "identd", "User ID: ", caw::gui_setting_presentation_info::option_list::container_type
					{ { "Nickname", irc::identd::Nickname }, { "E-mail (username)", irc::identd::Email }, { "Other: %s:id(\"Connection\",\"IdentdUserid\")%", irc::identd::Userid } } } },
			{ "Connection", "IdentdUserid", neolib::i_simple_variant::String, "",
				{ caw::gui_setting_presentation_info::LineEdit, "identd", "%w:width(\"WWWWWWWWWWWW\")%" } },
			{ "Connection", "PasswordEntryGlyphs", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::Hidden, "", "" } },

			{ "Logging", "LoggingEnabled", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Logging enabled" } },
			{ "Logging", "ServerLog", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Server log" } },
			{ "Logging", "ScrollbackLog", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Scrollback log enabled" } },
			{ "Logging", "ScrollbackLogSize", neolib::i_simple_variant::Integer, 100,
				{ caw::gui_setting_presentation_info::LineEdit, "", "Maximum scrollback log size: %w:width(\"000\")% KB" } },
			{ "Logging", "LogArchive", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Archiving enabled" } },
			{ "Logging", "LogArchiveSize", neolib::i_simple_variant::Integer, 1000,
				{ caw::gui_setting_presentation_info::LineEdit, "", "Archive when log file size is %w:width(\"0000\")% KB" } },
			{ "Logging", "LogFileDirectory", neolib::i_simple_variant::String, {},
				{ caw::gui_setting_presentation_info::LineEdit, "", "Log file directory: %w% %browse_directory%" } },
			{ "Logging", "LoggerEvents", neolib::i_simple_variant::Integer, irc::logger::Message,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Events to log: ", caw::gui_setting_presentation_info::option_list::container_type{
					{ "Messages", irc::logger::Message },
					{ "Notices", irc::logger::Notice },
					{ "Joins, Parts and Quits", irc::logger::JoinPartQuit },
					{ "Kicks", irc::logger::Kick },
					{ "Mode changes", irc::logger::Mode },
					{ "Other", irc::logger::Other } } } },
			{ "Messages", "ShowReplyMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")columns(\"|Normal Display Mode|Column Display Mode\",false)%Reply message: %w%%s:id(\"Messages\",\"ReplyMessage\")%%s:id(\"Messages\",\"ColumnReplyMessage\")%" } },
			{ "Messages", "ShowNormalMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Normal message: %w%%s:id(\"Messages\",\"NormalMessage\")%%s:id(\"Messages\",\"ColumnNormalMessage\")%" } },
			{ "Messages", "ShowActionMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Action message: %w%%s:id(\"Messages\",\"ActionMessage\")%%s:id(\"Messages\",\"ColumnActionMessage\")%" } },
			{ "Messages", "ShowNoticeMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Notice message: %w%%s:id(\"Messages\",\"NoticeMessage\")%%s:id(\"Messages\",\"ColumnNoticeMessage\")%" } },
			{ "Messages", "ShowJoinMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Join message: %w%%s:id(\"Messages\",\"JoinMessage\")%%s:id(\"Messages\",\"ColumnJoinMessage\")%" } },
			{ "Messages", "ShowKickMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Kick message: %w%%s:id(\"Messages\",\"KickMessage\")%%s:id(\"Messages\",\"ColumnKickMessage\")%" } },
			{ "Messages", "ShowNicknameMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Nickname change message: %w%%s:id(\"Messages\",\"NicknameMessage\")%%s:id(\"Messages\",\"ColumnNicknameMessage\")%" } },
			{ "Messages", "ShowPartMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Part message: %w%%s:id(\"Messages\",\"PartMessage\")%%s:id(\"Messages\",\"ColumnPartMessage\")%" } },
			{ "Messages", "ShowQuitMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Quit message: %w%%s:id(\"Messages\",\"QuitMessage\")%%s:id(\"Messages\",\"ColumnQuitMessage\")%" } },
			{ "Messages", "ShowTopicMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Topic message: %w%%s:id(\"Messages\",\"TopicMessage\")%%s:id(\"Messages\",\"ColumnTopicMessage\")%" } },
			{ "Messages", "ShowTopicAuthorMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Topic author message: %w%%s:id(\"Messages\",\"TopicAuthorMessage\")%%s:id(\"Messages\",\"ColumnTopicAuthorMessage\")%" } },
			{ "Messages", "ShowTopicChangeMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Topic change message: %w%%s:id(\"Messages\",\"TopicChangeMessage\")%%s:id(\"Messages\",\"ColumnTopicChangeMessage\")%" } },
			{ "Messages", "ShowModeMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Mode change message: %w%%s:id(\"Messages\",\"ModeMessage\")%%s:id(\"Messages\",\"ColumnModeMessage\")%" } },
			{ "Messages", "ShowCtcpMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%CTCP message: %w%%s:id(\"Messages\",\"CtcpMessage\")%%s:id(\"Messages\",\"ColumnCtcpMessage\")%" } },
			{ "Messages", "ShowSentNoticeMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Notice (sent) message: %w%%s:id(\"Messages\",\"SentNoticeMessage\")%%s:id(\"Messages\",\"ColumnSentNoticeMessage\")%" } },
			{ "Messages", "ShowJoiningMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Joining (self) message: %w%%s:id(\"Messages\",\"JoiningMessage\")%%s:id(\"Messages\",\"ColumnJoiningMessage\")%" } },
			{ "Messages", "ShowSelfJoinMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Join (self) message: %w%%s:id(\"Messages\",\"SelfJoinMessage\")%%s:id(\"Messages\",\"ColumnSelfJoinMessage\")%" } },
			{ "Messages", "ShowSelfKickMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Kick (self) message: %w%%s:id(\"Messages\",\"SelfKickMessage\")%%s:id(\"Messages\",\"ColumnSelfKickMessage\")%" } },
			{ "Messages", "ShowSelfNicknameMessage", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "%g:id(\"msgs\")%Nickname change (self) message: %w%%s:id(\"Messages\",\"SelfNicknameMessage\")%%s:id(\"Messages\",\"ColumnSelfNicknameMessage\")%" } },
			{ "Messages", "ReplyMessage", neolib::i_simple_variant::String, "* %M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "NormalMessage", neolib::i_simple_variant::String, "<%N%> %M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ActionMessage", neolib::i_simple_variant::String, "%N% %M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "NoticeMessage", neolib::i_simple_variant::String, "%?N%<<%N%>> %?%%?C%(%C%) %?%%M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "JoinMessage", neolib::i_simple_variant::String, "%N% has joined %C%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "KickMessage", neolib::i_simple_variant::String, "%N1% kicks %N2% from %C% (%R%)",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "NicknameMessage", neolib::i_simple_variant::String, "%O% is now known as %N%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "PartMessage", neolib::i_simple_variant::String, "%N% has left %C%%?R% (%R%)%?%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "QuitMessage", neolib::i_simple_variant::String, "%N% has quit IRC%?R% (%R%)%?%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "TopicMessage", neolib::i_simple_variant::String, "Topic is '%T%'",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "TopicAuthorMessage", neolib::i_simple_variant::String, "Set by %N% on %D%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "TopicChangeMessage", neolib::i_simple_variant::String, "%N% changes topic to '%T%'",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ModeMessage", neolib::i_simple_variant::String, "%N% sets mode %M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "CtcpMessage", neolib::i_simple_variant::String, "CTCP %R% request from %N%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "SentNoticeMessage", neolib::i_simple_variant::String, "<<%N%>> -> <%DN%> %M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "JoiningMessage", neolib::i_simple_variant::String, "Joining %C%...",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "SelfJoinMessage", neolib::i_simple_variant::String, "Now talking in %C%...",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "SelfKickMessage", neolib::i_simple_variant::String, "%N% kicks you from %C% (%R%)",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "SelfNicknameMessage", neolib::i_simple_variant::String, "You are now known as %N%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnReplyMessage", neolib::i_simple_variant::String, "*\\t%M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnNormalMessage", neolib::i_simple_variant::String, "%N%\\t%M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnActionMessage", neolib::i_simple_variant::String, "%N% %M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnNoticeMessage", neolib::i_simple_variant::String, "%?N%<<%N%>>%?%%?C% (%C%)%?%\\t%M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnJoinMessage", neolib::i_simple_variant::String, "%N% has joined %C%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnKickMessage", neolib::i_simple_variant::String, "%N1% kicks %N2% from %C% (%R%)",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnNicknameMessage", neolib::i_simple_variant::String, "%O% is now known as %N%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnPartMessage", neolib::i_simple_variant::String, "%N% has left %C%%?R% (%R%)%?%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnQuitMessage", neolib::i_simple_variant::String, "%N% has quit IRC%?R% (%R%)%?%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnTopicMessage", neolib::i_simple_variant::String, "Topic is '%T%'",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnTopicAuthorMessage", neolib::i_simple_variant::String, "Set by %N% on %D%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnTopicChangeMessage", neolib::i_simple_variant::String, "%N% changes topic to '%T%'",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnModeMessage", neolib::i_simple_variant::String, "%N% sets mode %M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnCtcpMessage", neolib::i_simple_variant::String, "CTCP %R% request from %N%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnSentNoticeMessage", neolib::i_simple_variant::String, "<<%N%>>\\t-> <%DN%> %M%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnJoiningMessage", neolib::i_simple_variant::String, "Joining %C%...",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnSelfJoinMessage", neolib::i_simple_variant::String, "Now talking in %C%...",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnSelfKickMessage", neolib::i_simple_variant::String, "%N% kicks you from %C% (%R%)",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },
			{ "Messages", "ColumnSelfNicknameMessage", neolib::i_simple_variant::String, "You are now known as %N%",
				{ caw::gui_setting_presentation_info::LineEdit, "", "" } },

			{ "Miscellaneous", "OpenNewConsoleOnStartup", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Open new console on startup" } },
			{ "Miscellaneous", "UseNoticeBuffer", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Use a separate window for notices" } },
			{ "Miscellaneous", "AutoRejoinOnKick", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Channels", "Auto re-join on kick" } },
			{ "Miscellaneous", "QuitMessage", neolib::i_simple_variant::String, "",
				{ caw::gui_setting_presentation_info::LineEdit, "Channels", "Quit message (default): " } },
			{ "Miscellaneous", "FloodPrevention", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "Flood Prevention", "Flood prevention enabled" } },
			{ "Miscellaneous", "FloodPreventionDelay", neolib::i_simple_variant::Integer, 500,
				{ caw::gui_setting_presentation_info::LineEdit, "Flood Prevention", "Flood prevention delay (ms): %w:width(\"0000\")%", {}, true, 0, 2000 } },
			{ "Miscellaneous", "UserListShowStatus", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "Channel User List", "Display channel status" } },
			{ "Miscellaneous", "UserAwayUpdate", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Channel User List", "Display away status" } },
			{ "Miscellaneous", "UserListShowIcons", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Channel User List", "Display icons" } },
			{ "Miscellaneous", "UserListShowGroups", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Channel User List", "Display groups" } },
			{ "Miscellaneous", "UserListUpdate", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "Channel User List", "Get user information" } },
			{ "Miscellaneous", "CtcpSoundFile", neolib::i_simple_variant::String, "",
				{ caw::gui_setting_presentation_info::LineEdit, "CTCP Requests", "Play sound: %w% %browse_file:sound%" } },
			{ "Miscellaneous", "CheckForNewSoftwareUpdates", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "Automatic Updates", "Automatically check for new software updates" } },
			{ "Miscellaneous", "CheckForIRCServerListUpdates", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "Automatic Updates", "Automatically check for IRC server list updates" } },

			{ "Windows", "ActivityPopupDelay", neolib::i_simple_variant::Integer, 5,
				{ caw::gui_setting_presentation_info::LineEdit, "", "Activity popup closes after %w:width(\"00\")% seconds of inactivity", {}, true, 1, 10 } },
			{ "Windows", "RememberWindowPos", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Remember a window's size and position" } },
			{ "Windows", "RememberWindowIconized", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "", "Remember a window's iconised state" } },
			{ "Windows", "OpenNewWindowIconized", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "New Windows", "Open iconised", } },
			{ "Windows", "OpenNewWindowOnDesktop", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "New Windows", "Open on desktop", } },
			{ "Windows", "OpenNewServerWindowOnDesktop", neolib::i_simple_variant::Boolean, false,
				{ caw::gui_setting_presentation_info::CheckBox, "New Windows", "Open new server window on desktop", } },
			{ "Windows", "OpenChannelWindowImmediately", neolib::i_simple_variant::Boolean, true,
				{ caw::gui_setting_presentation_info::CheckBox, "New Windows", "Open channel window immediately", } },
		};
	}

	void settings::register_settings()
	{
		for (std::size_t i = 0; i < sizeof(sSettingInfo) / sizeof(sSettingInfo[0]); ++i)
			iSettingPresentationInfo.insert(std::make_pair(do_register_setting(sSettingInfo[i].iCategory, sSettingInfo[i].iName, sSettingInfo[i].iType, sSettingInfo[i].iDefault), sSettingInfo[i].iPresentationInfo));

		neolib::i_setting& logFileDirectory = find_setting(neolib::string("Logging"), neolib::string("LogFileDirectory"));
		if (logFileDirectory.value().empty())
			logFileDirectory.set(neolib::simple_variant(neolib::string(iApplication.info().data_folder()) + "/logs"));
	}
}
