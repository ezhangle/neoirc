#pragma once

// buffer.hpp

#include <neolib/neolib.hpp>
#include <neolib/observable.hpp>
#include <neolib/string.hpp>
#include <neolib/i_settings.hpp>
#include <neolib/set.hpp>
#include <irc/client/connection_manager.hpp>
#include <irc/client/auto_join_watcher.hpp>
#include <irc/client/connection.hpp>
#include <irc/client/channel_buffer.hpp>
#include <irc/client/buffer.hpp>
#include <irc/client/user.hpp>
#include <irc/client/whois.hpp>
#include <irc/client/macros.hpp>
#include <irc/common/string.hpp>
#include <caw/i_gui.hpp>
#include <caw/i_buffer.hpp>
#include <caw/i_buffer_source.hpp>
#include <caw/i_buffer_message.hpp>
#include <caw/i_gui_buffer_info.hpp>
#include <caw/i_users.hpp>
#include <caw/i_user.hpp>
#include <caw/gui_action.hpp>

#include "KickReasonDialog.hpp"

namespace caw_irc_plugin
{
	namespace
	{
		inline std::string buffer_type_to_string(irc::buffer::type_e aBufferType)
		{
			switch (aBufferType)
			{
			case irc::buffer::SERVER:
				return "Server";
			case irc::buffer::CHANNEL:
				return "Channel";
			case irc::buffer::USER:
				return "User";
			case irc::buffer::NOTICE:
				return "Notice";
			default:
				throw std::logic_error("caw_irc_plugin::buffer_type_to_string: Unknown buffer type");
			}
		}
	}

	class buffer_message : public caw::i_buffer_message
	{
	public:
		buffer_message(neolib::i_settings& aSettings, irc::buffer& aIrcBuffer, const irc::message& aIrcMessage) :
			iSettings(aSettings), iIrcBuffer(aIrcBuffer), iIrcMessage(aIrcMessage)
		{
		}
	public:
		const irc::message& irc_message() const
		{
			return iIrcMessage;
		}
		virtual message_id_type message_id() const
		{
			return iIrcMessage.id();
		}
		virtual const neolib::i_string& as_text() const
		{
			if (iText.empty())
			{
				switch (iIrcMessage.command())
				{
				case irc::message::KICK:
					iText = iIrcMessage.to_nice_string(iIrcBuffer.model().message_strings(), &iIrcBuffer, std::string(),
						iIrcMessage.parameters().size() >= 2 && irc::make_string(iIrcBuffer, irc::user(iIrcMessage.parameters()[1], iIrcBuffer).nick_name()) == iIrcBuffer.connection().nick_name(),
						iSettings.find_setting("Formatting", "DisplayTimestamps").value().value_as_boolean(), false);
					break;
				case irc::message::JOIN:
				case irc::message::NICK:
					iText = iIrcMessage.to_nice_string(iIrcBuffer.model().message_strings(), &iIrcBuffer, std::string(),
						irc::user(iIrcMessage.origin(), iIrcBuffer).nick_name() == iIrcBuffer.connection().nick_name(),
						iSettings.find_setting("Formatting", "DisplayTimestamps").value().value_as_boolean(), false);
					break;
				default:
					iText = iIrcMessage.to_nice_string(iIrcBuffer.model().message_strings(), &iIrcBuffer, std::string(),
						false,
						iSettings.find_setting("Formatting", "DisplayTimestamps").value().value_as_boolean(), false);
					break;
				}
			}
			return iText;
		}
	private:
		neolib::i_settings& iSettings;
		irc::buffer& iIrcBuffer;
		const irc::message& iIrcMessage;
		mutable neolib::string iText;
	};

	class buffer : public
		caw::i_buffer,
		public caw::i_gui_buffer_info,
		public caw::i_buffer::i_input_info,
		public neolib::reference_counted<caw::i_users>,
		private irc::buffer_observer,
		private irc::channel_buffer_observer,
		private neolib::i_settings::i_subscriber,
		private neolib::observable<caw::i_buffer::i_subscriber>,
		private neolib::observable<caw::i_users::i_subscriber>,
		private neolib::destroyable
	{
		// types
	public:
		typedef std::map<irc::model::id, caw_irc_plugin::buffer_message> message_list;
	private:
		class user : public neolib::reference_counted<caw::i_user>
		{
		public:
			user(buffer& aBuffer, irc::channel_user& aChannelUser) :
				iBuffer(&aBuffer), iChannelUser(&aChannelUser)
			{
				pin();
				update();
			}
			user(const user& aOther) :
				iBuffer(aOther.iBuffer), iChannelUser(aOther.iChannelUser)
			{
				pin();
				update();
			}
			user(const i_user& aOther) :
				iBuffer(0), iChannelUser(0)
			{
				throw std::logic_error("caw_irc_plugin::buffer::user::user: Function not implemented.");
			}
		public:
			virtual const neolib::i_string& display_name() const
			{
				return iDisplayName;
			}
			virtual const neolib::i_string& user_name() const
			{
				return iUserName;
			}
			virtual const neolib::i_string& user_info() const
			{
				return iUserInfo;
			}
			virtual bool has_icon() const
			{
				return true;
			}
			virtual const neolib::i_string& icon_uri() const
			{
				static const neolib::string sNormalIcon = ":/irc/Resources/user.png";
				static const neolib::string sVoiceIcon = ":/irc/Resources/voice.png";
				static const neolib::string sOperatorIcon = ":/irc/Resources/operator.png";
				if (iChannelUser->is_operator())
					return sOperatorIcon;
				else if (iChannelUser->is_voice())
					return sVoiceIcon;
				else
					return sNormalIcon;
			}
			virtual bool away() const
			{
				return iChannelUser->away();
			}
			virtual void open() const
			{
				iBuffer->iIrcBuffer.connection().create_buffer(user_name().to_std_string()).activate();
			}
			virtual void context_menu(caw::i_gui_action_control& aMenu) const
			{
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					open();
				}, 0, neolib::string("Private Message..."), neolib::string(), neolib::string("Private Message"), neolib::string("Private Message"), neolib::string(":/irc/Resources/user.png")));
				aMenu.add_action(caw::gui_action::separator(iBuffer->plugin(), 0));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					/* todo */
				}, 0, neolib::string("Send File..."), neolib::string(), neolib::string("Send File"), neolib::string("Send File"), neolib::string(":/irc/Resources/dcc_sendfile.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					/* todo */
				}, 0, neolib::string("Chat Direct..."), neolib::string(), neolib::string("Chat Direct"), neolib::string("Chat Direct"), neolib::string(":/irc/Resources/dcc_chat.png")));
				aMenu.add_action(caw::gui_action::separator(iBuffer->plugin(), 0));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					iBuffer->iIrcBuffer.connection().whois().new_request(user_name().to_std_string(), buffer_for_user(true));
				}, 0, neolib::string("Whois"), neolib::string(), neolib::string("Whois"), neolib::string("Whois"), neolib::string(":/irc/Resources/whois.png")));
				caw::i_gui_action_control& ctcpMenu = aMenu.add_action(caw::gui_action::sub_control(iBuffer->plugin(), 0, neolib::string("CTCP")));
				ctcpMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/PING " + user_name().to_std_string();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Ping"), neolib::string(), neolib::string("Ping"), neolib::string("Ping"), neolib::string(":/irc/Resources/ping.png")));
				ctcpMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/TIME " + user_name().to_std_string();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Time"), neolib::string(), neolib::string("Time"), neolib::string("Time"), neolib::string(":/irc/Resources/time.png")));
				ctcpMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/FINGER " + user_name().to_std_string();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Finger"), neolib::string(), neolib::string("Finger"), neolib::string("Finger"), neolib::string(":/irc/Resources/finger.png")));
				ctcpMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/USERINFO " + user_name().to_std_string();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Userinfo"), neolib::string(), neolib::string("Userinfo"), neolib::string("Userinfo"), neolib::string(":/irc/Resources/userinfo.png")));
				ctcpMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/VERSION " + user_name().to_std_string();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Version"), neolib::string(), neolib::string("Version"), neolib::string("Version"), neolib::string(":/irc/Resources/version.png")));
				ctcpMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/SOURCE " + user_name().to_std_string();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Source"), neolib::string(), neolib::string("Source"), neolib::string("Source"), neolib::string(":/irc/Resources/source.png")));
				aMenu.add_action(caw::gui_action::separator(iBuffer->plugin(), 0));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/MUTE ";
					strMessage += iChannelUser->msgto_form();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Mute"), neolib::string(), neolib::string("Mute"), neolib::string("Mute"), neolib::string(":/irc/Resources/mute.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/UNMUTE ";
					strMessage += iChannelUser->msgto_form();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Unmute"), neolib::string(), neolib::string("Unmute"), neolib::string("Unmute"), neolib::string(":/irc/Resources/unmute.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/IGNORE ";
					strMessage += iChannelUser->msgto_form();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Ignore"), neolib::string(), neolib::string("Ignore"), neolib::string("Ignore"), neolib::string(":/irc/Resources/ignore.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					std::string strMessage = "/UNIGNORE ";
					strMessage += iChannelUser->msgto_form();
					buffer_for_user(true).new_message(strMessage);
				}, 0, neolib::string("Unignore"), neolib::string(), neolib::string("Unignore"), neolib::string("Unignore"), neolib::string(":/irc/Resources/unignore.png")));
				aMenu.add_action(caw::gui_action::separator(iBuffer->plugin(), 0));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					set_user_mode("+o");
				}, 0, neolib::string("Op"), neolib::string(), neolib::string("Op"), neolib::string("Op"), neolib::string(":/irc/Resources/mode.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					set_user_mode("+v");
				}, 0, neolib::string("Voice"), neolib::string(), neolib::string("Voice"), neolib::string("Voice"), neolib::string(":/irc/Resources/mode.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					set_user_mode("-o");
				}, 0, neolib::string("Deop"), neolib::string(), neolib::string("Deop"), neolib::string("Deop"), neolib::string(":/irc/Resources/unmode.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					set_user_mode("-v");
				}, 0, neolib::string("Devoice"), neolib::string(), neolib::string("Devoice"), neolib::string("Devoice"), neolib::string(":/irc/Resources/unmode.png")));
				if (iBuffer->iIrcBuffer.connection().prefixes().first != "ov")
				{
					caw::i_gui_action_control& modeMenu = aMenu.add_action(caw::gui_action::sub_control(iBuffer->plugin(), 0, neolib::string("Mode")));
					for (std::size_t i = 0; i < iBuffer->iIrcBuffer.connection().prefixes().first.size(); ++i)
					{
						std::string prefixChar;
						prefixChar += iBuffer->iIrcBuffer.connection().prefixes().second[i];
						if (prefixChar == "&")
							prefixChar = "&&";
						std::string addMode = "+"; addMode += iBuffer->iIrcBuffer.connection().prefixes().first[i];
						std::string addMenuText = addMode + " ( " + prefixChar + " )";
						modeMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this, addMode]()
						{
							set_user_mode(addMode);
						}, 0, neolib::string(addMenuText), neolib::string(), neolib::string(addMenuText), neolib::string(addMenuText), neolib::string(":/irc/Resources/mode.png")));
					}
					for (std::size_t i = 0; i < iBuffer->iIrcBuffer.connection().prefixes().first.size(); ++i)
					{
						std::string prefixChar;
						prefixChar += iBuffer->iIrcBuffer.connection().prefixes().second[i];
						if (prefixChar == "&")
							prefixChar = "&&";
						std::string removeMode = "-"; removeMode += iBuffer->iIrcBuffer.connection().prefixes().first[i];
						std::string removeMenuText = removeMode + " ( " + prefixChar + " )";
						modeMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this, removeMode]()
						{
							set_user_mode(removeMode);
						}, 0, neolib::string(removeMenuText), neolib::string(), neolib::string(removeMenuText), neolib::string(removeMenuText), neolib::string(":/irc/Resources/unmode.png")));
					}
				}
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					kick_user();
				}, 0, neolib::string("Kick"), neolib::string(), neolib::string("Kick"), neolib::string("Kick"), neolib::string(":/irc/Resources/kick.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					gui::KickReasonDialog dialog(iBuffer->iIrcBuffer, *iChannelUser);
					if (dialog.exec() == QDialog::Accepted)
						kick_user(dialog.kick_reason());
				}, 0, neolib::string("Kick (Reason)..."), neolib::string(), neolib::string("Kick (Reason)"), neolib::string("Kick (Reason)"), neolib::string(":/irc/Resources/kick.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					set_user_mode("+b");
				}, 0, neolib::string("Ban"), neolib::string(), neolib::string("Ban"), neolib::string("Ban"), neolib::string(":/irc/Resources/ban.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					set_user_mode("+b");
					kick_user();
				}, 0, neolib::string("Ban/Kick"), neolib::string(), neolib::string("Ban/Kick"), neolib::string("Ban/Kick"), neolib::string(":/irc/Resources/ban.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					set_user_mode("+b");
					gui::KickReasonDialog dialog(iBuffer->iIrcBuffer, *iChannelUser);
					if (dialog.exec() == QDialog::Accepted)
						kick_user(dialog.kick_reason());
				}, 0, neolib::string("Ban/Kick (Reason)..."), neolib::string(), neolib::string("Ban/Kick (Reason)"), neolib::string("Ban/Kick (Reason)"), neolib::string(":/irc/Resources/ban.png")));
				aMenu.add_action(caw::gui_action::separator(iBuffer->plugin(), 0));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					/* todo */
				}, 0, neolib::string("Add/Edit Contact..."), neolib::string(), neolib::string("Add/Edit Contact"), neolib::string("Add/Edit Contact"), neolib::string(":/ClicksAndWhistles/Resources/contacts.png")));
				aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this]()
				{
					/* todo */
				}, 0, neolib::string("Highlight..."), neolib::string(), neolib::string("Highlight"), neolib::string("Highlight"), neolib::string(":/ClicksAndWhistles/Resources/text_effects.png")));
				bool addedMacro = false;
				for (irc::macros::const_iterator i = iBuffer->iIrcBuffer.model().macros().entries().begin(); i != iBuffer->iIrcBuffer.model().macros().entries().end(); ++i)
				{
					if (i->user_menu())
					{
						if (!addedMacro)
						{
							addedMacro = true;
							aMenu.add_action(caw::gui_action::separator(iBuffer->plugin(), 0));
						}
						std::string text = i->description().empty() ? i->name() : i->description();
						aMenu.add_action(new caw::gui_action(iBuffer->plugin(), [this, i]()
						{
							std::string strMessage = i->name();
							if (!strMessage.empty())
							{
								strMessage += " " + user_name().to_std_string();
								buffer_for_user(true).new_message(strMessage);
							}
						}, 0, neolib::string(text), neolib::string(), neolib::string(text), neolib::string(text), neolib::string(":/irc/Resources/macro.png")));
					}
				}
			}
		public:
			virtual uint32_t compare_value() const
			{
				return iChannelUser->compare_value();
			}
			virtual bool operator<(const i_user& aRhs) const
			{
				if (compare_value() != aRhs.compare_value())
					return compare_value() < aRhs.compare_value();
				return irc::make_string(iChannelUser->casemapping(), user_name().to_std_string()) < aRhs.user_name().to_std_string();
			}
		public:
			virtual void update()
			{
				iDisplayName = iChannelUser->qualified_name();
				iUserName = iChannelUser->nick_name();
				iUserInfo = iChannelUser->msgto_form() + (iChannelUser->has_full_name() ? " - " + iChannelUser->full_name() : std::string());
			}
			// mutable_set support
		public:
			class value_type : public neolib::auto_ref<caw::i_user>, public irc::channel_user::key_type
			{
			public:
				typedef irc::channel_user::key_type key_type;
			public:
				value_type(const neolib::auto_ref<caw::i_user>& aUser) : 
					neolib::auto_ref<caw::i_user>(aUser), irc::channel_user::key_type(*static_cast<user&>(*aUser).iChannelUser)
				{
				}
			};		
		private:
			irc::buffer& buffer_for_user(bool aActivate = false) const
			{
				irc::buffer* theBuffer = iBuffer->iIrcBuffer.connection().connection_manager().active_buffer();
				if (theBuffer == 0 || &theBuffer->connection() != &iBuffer->iIrcBuffer.connection() ||
					!theBuffer->has_user(user_name().to_std_string()))
					theBuffer = &iBuffer->iIrcBuffer.connection().server_buffer();
				if (aActivate)
					theBuffer->activate();
				return *theBuffer;
			}
			void set_user_mode(const std::string& aMode) const
			{
				irc::message theMessage(iBuffer->iIrcBuffer, irc::message::OUTGOING);
				theMessage.set_command(irc::message::MODE);
				theMessage.parameters().push_back(iBuffer->iIrcBuffer.name());
				theMessage.parameters().push_back(aMode);
				theMessage.parameters().push_back(aMode != "+b" ? iChannelUser->nick_name() : iChannelUser->ban_mask());
				iBuffer->iIrcBuffer.new_message(theMessage);
			}
			void kick_user(const std::string& aReason = "") const
			{
				irc::message theMessage(iBuffer->iIrcBuffer, irc::message::OUTGOING);
				theMessage.set_command(irc::message::KICK);
				theMessage.parameters().push_back(iBuffer->iIrcBuffer.name());
				theMessage.parameters().push_back(iChannelUser->nick_name());
				theMessage.parameters().push_back(aReason);
				iBuffer->iIrcBuffer.new_message(theMessage);
			}
		private:
			buffer* iBuffer;
			irc::channel_user* iChannelUser;
			neolib::string iDisplayName;
			neolib::string iUserName;
			neolib::string iUserInfo;
		};
		typedef neolib::multiset<neolib::i_auto_ref<caw::i_user>, user::value_type> concrete_user_list_type;
		struct user_map_comparitor
		{
			bool operator()(const irc::channel_user_list::iterator& aLhs, const irc::channel_user_list::iterator& aRhs) const
			{
				return &*aLhs < &*aRhs;
			}
		};
		typedef std::multimap<irc::channel_user_list::iterator, concrete_user_list_type::container_type::iterator, user_map_comparitor> user_map;
		// construction
	public:
		buffer(neolib::i_plugin& aPlugin, caw::i_gui& aGui, neolib::i_settings& aSettings, irc::buffer& aIrcBuffer, caw::i_buffer_source& aBufferSource) :
			iPlugin(aPlugin),
			iGui(aGui),
			iSettings(aSettings),
			iIrcBuffer(aIrcBuffer),
			iBufferSource(aBufferSource),
			iId(buffer_type_to_string(aIrcBuffer.type()) + ":" + aIrcBuffer.connection().server().network() + ":" + aIrcBuffer.connection().server().name() + ":" + aIrcBuffer.name()),
			iName(aIrcBuffer.name()),
			iTitle(aIrcBuffer.title()),
			iDoingTabCompletion(false)
		{
			reference_counted<caw::i_users>::pin();
			iBufferSource.add_buffer(this, *this);
			iIrcBuffer.add_observer(*this);
			if (iIrcBuffer.type() == irc::buffer::CHANNEL)
				static_cast<irc::channel_buffer&>(iIrcBuffer).observable<channel_buffer_observer>::add_observer(static_cast<irc::channel_buffer_observer&>(*this));
			iSettings.subscribe(*this);
		}
		~buffer()
		{
			iSettings.unsubscribe(*this);
			if (!iIrcBuffer.closing())
			{
				if (iIrcBuffer.type() == irc::buffer::CHANNEL)
					static_cast<irc::channel_buffer&>(iIrcBuffer).observable<channel_buffer_observer>::remove_observer(static_cast<irc::channel_buffer_observer&>(*this));
			}
			iIrcBuffer.remove_observer(*this);
			iBufferSource.remove_buffer(this);
		}
		// from caw::i_buffer
	public:
		virtual neolib::i_plugin& plugin() const
		{
			return iPlugin;
		}
		virtual const neolib::i_string& id() const
		{
			return iId;
		}
		virtual const neolib::i_string& name() const
		{
			return iName;
		}
		virtual const neolib::i_string& title() const
		{
			return iTitle;
		}
		virtual const i_gui_buffer_info& info() const
		{
			return *this;
		}
		virtual const i_input_info& input_info() const
		{
			return *this;
		}
		virtual i_input_info& input_info()
		{
			return *this;
		}
		virtual size_type message_count() const
		{
			return iIrcBuffer.messages().size();
		}
		virtual const caw::i_buffer_message& message(size_type aMessageIndex) const
		{
			return iMessages.find(iIrcBuffer.messages()[aMessageIndex].id())->second;
		}
		virtual bool has_users() const
		{
			return iIrcBuffer.type() == irc::buffer::CHANNEL;
		}
		virtual caw::i_users& users()
		{
			return *this;
		}
		virtual caw::i_gui_buffer_paragraph* to_paragraph(const caw::i_buffer_message& aMessage) const
		{
			throw std::logic_error("not yet implemented");
		}
		virtual void send(const neolib::i_string& aMessage)
		{
			iIrcBuffer.new_message(aMessage.to_std_string());
		}
		virtual bool can_activate() const
		{
			irc::buffer* activeBuffer = iIrcBuffer.connection().connection_manager().active_buffer();
			if (iIrcBuffer.model().auto_join_watcher().is_auto_join() && activeBuffer != 0 && activeBuffer->type() == irc::buffer::CHANNEL)
				return false;
			bool canActivate = true;
			observable<i_buffer::i_subscriber>::notify_observers(i_buffer::i_subscriber::NotifyCanActivateBuffer, canActivate);
			return canActivate;
		}
		virtual void activate()
		{
			iIrcBuffer.activate();
		}
		virtual void close()
		{
			iIrcBuffer.close();
		}
		virtual bool can_add_favourite() const
		{
			return iIrcBuffer.type() == irc::buffer::CHANNEL || iIrcBuffer.type() == irc::buffer::USER;
		}
		virtual void add_favourite(caw::i_favourites& aFavourites) const
		{
		}
	public:
		virtual void subscribe(i_buffer::i_subscriber& aObserver)
		{
			observable<i_buffer::i_subscriber>::add_observer(aObserver);
			for (size_type i = 0; i < message_count(); ++i)
				aObserver.new_message(*this, message(i));
		}
		virtual void unsubscribe(i_buffer::i_subscriber& aObserver)
		{
			observable<i_buffer::i_subscriber>::remove_observer(aObserver);
			bool anyStrongObservers = false;
			observable<i_buffer::i_subscriber>::notify_observers(i_buffer::i_subscriber::NotifyIsWeakObserver, anyStrongObservers);
			if (!anyStrongObservers)
				iIrcBuffer.close();
		}
		// from caw::i_gui_buffer_info
	public:
		virtual uint32_t column_count() const
		{
			uint32_t columns = iSettings.find_setting("Formatting", "DisplayTimestamps").value().value_as_boolean() ? 1 : 0;
			switch (iIrcBuffer.type())
			{
			case irc::buffer::SERVER:
				columns += 1;
				break;
			case irc::buffer::CHANNEL:
				columns += 2;
				break;
			case irc::buffer::USER:
				columns += 2;
				break;
			case irc::buffer::NOTICE:
				columns += 2;
				break;
			}
			return columns;
		}
		virtual bool has_icon() const
		{
			return false;
		}
		virtual const neolib::i_string& icon_uri() const
		{
			switch (iIrcBuffer.type())
			{
			case irc::buffer::SERVER:
			{
				static const neolib::string sIconUri(":/irc/Resources/server.png");
				return sIconUri;
			}
			case irc::buffer::CHANNEL:
			{
				static const neolib::string sIconUri(":/irc/Resources/channel.png");
				return sIconUri;
			}
			case irc::buffer::USER:
			{
				static const neolib::string sIconUri(":/irc/Resources/user.png");
				return sIconUri;
			}
			case irc::buffer::NOTICE:
			{
				static const neolib::string sIconUri(":/irc/Resources/notice.png");
				return sIconUri;
			}
			default:
			{
				static const neolib::string sIconUri(":/irc/Resources/other.png");
				return sIconUri;
			}
			}
		}
		virtual bool supports_formatting() const
		{
			return true;
		}
		// from i_input_info
	public:
		virtual EModifierKey input_new_line_modifier_key() const
		{
			return static_cast<EModifierKey>(iSettings.find_setting("Formatting", "InputNewLineModifierKey").value().value_as_integer());
		}
		virtual EModifierKey input_history_modifier_key() const
		{
			return static_cast<EModifierKey>(iSettings.find_setting("Formatting", "InputHistoryModifierKey").value().value_as_integer());
		}
		virtual bool input_word_wrap() const
		{
			return iSettings.find_setting("Formatting", "InputWordwrap").value().value_as_boolean();
		}
		virtual uint32_t minimum_input_lines() const
		{
			return static_cast<uint32_t>(iSettings.find_setting("Formatting", "InputMin").value().value_as_integer());
		}
		virtual uint32_t maximum_input_lines() const
		{
			return static_cast<uint32_t>(iSettings.find_setting("Formatting", "InputMax").value().value_as_integer());
		}
		virtual bool supports_tab_completion() const
		{
			return iIrcBuffer.type() == irc::buffer::CHANNEL || iIrcBuffer.type() == irc::buffer::USER;
		}
		virtual void start_tab_completion(const neolib::i_string& aPrefix)
		{
			iDoingTabCompletion = true;
			iTabCompletionPrefix = aPrefix.to_std_string();
		}
		struct tab_completion_comparator_for_channel_view
		{
			irc::buffer& iBuffer;
			std::string iPreviousMatch;
			tab_completion_comparator_for_channel_view(irc::buffer& aBuffer, const std::string& aPreviousMatch) : iBuffer(aBuffer), iPreviousMatch(aPreviousMatch) {}
			bool operator()(const irc::user* aFirst, const irc::user* aSecond)
			{
				const irc::channel_user& firstUser = static_cast<const irc::channel_user&>(*aFirst);
				const irc::channel_user& secondUser = static_cast<const irc::channel_user&>(*aSecond);
				if (irc::make_string(iBuffer, firstUser.nick_name()) == irc::make_string(iBuffer, iPreviousMatch))
					return true;
				else if (irc::make_string(iBuffer, secondUser.nick_name()) == irc::make_string(iBuffer, iPreviousMatch))
					return false;
				else if (firstUser.has_last_message_id() && !secondUser.has_last_message_id())
					return true;
				else if (!firstUser.has_last_message_id() && secondUser.has_last_message_id())
					return false;
				else if (!firstUser.has_last_message_id() && !secondUser.has_last_message_id())
					return irc::make_string(iBuffer, firstUser.nick_name()) < irc::make_string(iBuffer, secondUser.nick_name());
				else
					return firstUser.last_message_id() > secondUser.last_message_id();
			}
		};
		virtual bool do_tab_completion(bool aReverse, neolib::i_string& aMatch)
		{
			std::vector<irc::string> matches;
			if (iIrcBuffer.connection().is_channel(iTabCompletionPrefix))
			{
				bool addedSelf = false;
				if (iIrcBuffer.type() == irc::buffer::CHANNEL && irc::make_string(iIrcBuffer, iIrcBuffer.name()).find(irc::make_string(iIrcBuffer, iTabCompletionPrefix)) == 0)
				{
					matches.push_back(irc::make_string(iIrcBuffer, iIrcBuffer.name()));
					addedSelf = true;
				}
				for (irc::connection::channel_buffer_list::const_iterator i = iIrcBuffer.connection().channel_buffers().begin(); i != iIrcBuffer.connection().channel_buffers().end(); ++i)
					if (irc::make_string(iIrcBuffer, i->first).find(irc::make_string(iIrcBuffer, iTabCompletionPrefix)) == 0)
					{
						if (iIrcBuffer.type() == irc::buffer::CHANNEL && irc::make_string(iIrcBuffer, iIrcBuffer.name()) == irc::make_string(iIrcBuffer, i->first))
						{
							if (!addedSelf)
								matches.push_back(i->first);
						}
						else
							matches.push_back(i->first);
					}
				std::sort(addedSelf ? matches.begin() + 1 : matches.begin(), matches.end());
			}
			else if (iIrcBuffer.type() == irc::buffer::CHANNEL)
			{
				std::vector<const irc::user*> userMatches;
				for (irc::channel_user_list::const_iterator i = static_cast<irc::channel_buffer&>(iIrcBuffer).users().begin(); i != static_cast<irc::channel_buffer&>(iIrcBuffer).users().end(); ++i)
					if (irc::make_string(iIrcBuffer, i->nick_name()) != irc::make_string(iIrcBuffer, iIrcBuffer.connection().identity().nick_name()) &&
						irc::make_string(iIrcBuffer, i->nick_name()).find(irc::make_string(iIrcBuffer, iTabCompletionPrefix)) == 0)
						userMatches.push_back(&*i);
				std::sort(userMatches.begin(), userMatches.end(), tab_completion_comparator_for_channel_view(iIrcBuffer, !iTabCompletionPrefix.empty() && irc::make_string(iIrcBuffer, iTabCompletionPreviousMatch).find(irc::make_string(iIrcBuffer, iTabCompletionPrefix)) == 0 ? iTabCompletionPreviousMatch : ""));
				for (std::vector<const irc::user*>::const_iterator i = userMatches.begin(); i != userMatches.end(); ++i)
					matches.push_back(irc::make_string(iIrcBuffer, (*i)->nick_name()));
			}
			else if (iIrcBuffer.type() == irc::buffer::USER)
			{
				if (irc::make_string(iIrcBuffer, static_cast<irc::user_buffer&>(iIrcBuffer).user().nick_name()).find(irc::make_string(iIrcBuffer, iTabCompletionPrefix)) == 0)
					matches.push_back(irc::make_string(iIrcBuffer, static_cast<irc::user_buffer&>(iIrcBuffer).user().nick_name()));
			}
			if (matches.empty())
			{
				iGui.beep();
				return false;
			}
			if (!iTabCompletionCounter)
				iTabCompletionCounter = aReverse ? matches.size() - 1 : 0;
			else if (aReverse)
				--*iTabCompletionCounter;
			else
				++*iTabCompletionCounter;
			if (*iTabCompletionCounter < 0)
				*iTabCompletionCounter = matches.size() - 1;
			else if (*iTabCompletionCounter >= static_cast<int>(matches.size()))
				*iTabCompletionCounter = 0;
			iTabCompletionMatch = irc::make_string(matches[*iTabCompletionCounter]);
			aMatch = iTabCompletionMatch;
			return true;
		}
		virtual bool doing_tab_completion() const
		{
			return iDoingTabCompletion;
		}
		virtual void end_tab_completion()
		{
			iTabCompletionPrefix = "";
			iTabCompletionCounter.reset();
			iTabCompletionPreviousMatch = iTabCompletionMatch;
			iTabCompletionMatch = "";
			iDoingTabCompletion = false;
		}
		virtual bool can_reset_tab_completion() const
		{
			return iTabCompletionPrefix != "" || iTabCompletionMatch != "" || iTabCompletionPreviousMatch != "" || iDoingTabCompletion;
		}
		virtual void reset_tab_completion()
		{
			end_tab_completion();
			iTabCompletionPreviousMatch = "";
		}
		// from caw::i_users
	public:
		virtual const user_list_type& user_list() const
		{
			return iUsers;
		}
		virtual user_list_type& user_list()
		{
			return iUsers;
		}
	public:
		virtual void subscribe(caw::i_users::i_subscriber& aObserver)
		{
			observable<i_users::i_subscriber>::add_observer(aObserver);
			for (concrete_user_list_type::iterator i = iUsers.begin(); i != iUsers.end(); ++i)
			{
				aObserver.user_added(**i);
			}
		}
		virtual void unsubscribe(caw::i_users::i_subscriber& aObserver)
		{
			observable<i_users::i_subscriber>::remove_observer(aObserver);
		}
		// from buffer_observer
	private:
		virtual void buffer_message(irc::buffer& aBuffer, const irc::message& aMessage) 
		{ 
			switch (aMessage.command())
			{
			default:
				if (aBuffer.type() != irc::buffer::SERVER)
					return;
				// fall through...
			case irc::message::UNKNOWN:
			case irc::message::PRIVMSG:
			case irc::message::NOTICE:
			case irc::message::TOPIC:
			case irc::message::RPL_TOPIC:
			case irc::message::RPL_TOPICAUTHOR:
			case irc::message::JOIN:
			case irc::message::PART:
			case irc::message::QUIT:
			case irc::message::KICK:
			case irc::message::NICK:
			case irc::message::MODE:
			case irc::message::RPL_WHOISUSER:
			case irc::message::RPL_WHOISCHANNELS:
			case irc::message::RPL_WHOISSERVER:
			case irc::message::RPL_WHOISOPERATOR:
			case irc::message::RPL_WHOISIDLE:
			case irc::message::RPL_WHOISEXTRA:
			case irc::message::RPL_WHOISREGNICK:
			case irc::message::RPL_WHOISADMIN:
			case irc::message::RPL_WHOISSADMIN:
			case irc::message::RPL_WHOISSVCMSG:
			case irc::message::RPL_AWAY:
			case irc::message::RPL_ENDOFWHOIS:
			case irc::message::RPL_WHOREPLY:
			case irc::message::RPL_ENDOFWHO:
			case irc::message::ERR_NOSUCHNICK:
			case irc::message::ERR_CANNOTSENDTOCHAN:
			case irc::message::ERR_NOSUCHSERVICE:
				iMessages.insert(std::make_pair(aMessage.id(), caw_irc_plugin::buffer_message(iSettings, iIrcBuffer, aMessage)));
				observable<i_buffer::i_subscriber>::notify_observers(i_buffer::i_subscriber::NotifyNewMessage, iMessages.find(aMessage.id())->second);
				break;
			}
		}
		virtual void buffer_message_updated(irc::buffer& aBuffer, const irc::message& aMessage) {}
		virtual void buffer_message_removed(irc::buffer& aBuffer, const irc::message& aMessage) {}
		virtual void buffer_message_failure(irc::buffer& aBuffer, const irc::message& aMessage) {}
		virtual void buffer_activate(irc::buffer& aBuffer) 
		{
			observable<i_buffer::i_subscriber>::notify_observers(i_buffer::i_subscriber::NotifyActivateBuffer);
		}
		virtual void buffer_reopen(irc::buffer& aBuffer) {}
		virtual void buffer_open(irc::buffer& aBuffer, const irc::message& aMessage) {}
		virtual void buffer_closing(irc::buffer& aBuffer) 
		{ 
			neolib::destroyable::destroyed_flag destroyed(*this);
			observable<i_buffer::i_subscriber>::notify_observers(i_buffer::i_subscriber::NotifyClosing);
			if (!destroyed)
				delete this;
		}
		virtual bool is_weak_observer(irc::buffer& aBuffer)
		{ 
			return false;
		}
		virtual void buffer_name_changed(irc::buffer& aBuffer, const std::string& aOldName)
		{
			iName = iIrcBuffer.name();
			neolib::string oldName = aOldName;
			observable<i_buffer::i_subscriber>::notify_observers(i_buffer::i_subscriber::NotifyNameChanged, oldName);
		}
		virtual void buffer_title_changed(irc::buffer& aBuffer, const std::string& aOldTitle) 
		{
			iTitle = iIrcBuffer.title();
			neolib::string oldTitle = aOldTitle;
			observable<i_buffer::i_subscriber>::notify_observers(i_buffer::i_subscriber::NotifyTitleChanged, oldTitle);
		}
		virtual void buffer_ready_changed(irc::buffer& aBuffer) {}
		virtual void buffer_reloaded(irc::buffer& aBuffer) {}
		virtual void buffer_cleared(irc::buffer& aBuffer) {}
		virtual void buffer_hide(irc::buffer& aBuffer) {}
		virtual void buffer_show(irc::buffer& aBuffer) {}
		// from irc::channel_buffer_observer
		virtual void joining_channel(irc::channel_buffer& aBuffer)
		{
		}
		virtual void user_added(irc::channel_buffer& aBuffer, irc::channel_user_list::iterator aUser)
		{
			user_map::iterator iterNewUser = iUserMap.insert(user_map::value_type(aUser, iUsers.container().insert(user::value_type(new user(*this, *aUser)))));
			observable<i_users::i_subscriber>::notify_observers(i_users::i_subscriber::NotifyUserAdded, **iterNewUser->second);
		}
		virtual void user_updated(irc::channel_buffer& aBuffer, irc::channel_user_list::iterator aOldUser, irc::channel_user_list::iterator aNewUser)
		{
			user_map::iterator iterNewUser = iUserMap.find(aNewUser);
			if (iterNewUser == iUserMap.end())
				iterNewUser = iUserMap.insert(user_map::value_type(aNewUser, iUsers.container().insert(user::value_type(new user(*this, *aNewUser)))));
			else
				(*iterNewUser->second)->update();
			user_map::iterator iterOldUser = iUserMap.find(aOldUser);
			neolib::auto_ref<caw::i_user> oldUser = *iterOldUser->second;
			if (aNewUser != aOldUser)
			{
				iUsers.container().erase(iterOldUser->second);
				iUserMap.erase(iterOldUser);
			}
			observable<i_users::i_subscriber>::notify_observers(i_users::i_subscriber::NotifyUserUpdated, *oldUser, **iterNewUser->second);
		}
		virtual void user_removed(irc::channel_buffer& aBuffer, irc::channel_user_list::iterator aUser)
		{
			user_map::iterator iterUser = iUserMap.find(aUser);
			if (iterUser != iUserMap.end())
			{
				neolib::auto_ref<caw::i_user> user = *iterUser->second;
				iUsers.container().erase(iterUser->second);
				iUserMap.erase(iterUser);
				observable<i_users::i_subscriber>::notify_observers(i_users::i_subscriber::NotifyUserRemoved, *user);
			}
		}
		virtual void user_host_info(irc::channel_buffer& aBuffer, irc::channel_user_list::iterator aUser)
		{
		}
		virtual void user_away_status(irc::channel_buffer& aBuffer, irc::channel_user_list::iterator aUser)
		{
		}
		virtual void user_list_updating(irc::channel_buffer& aBuffer)
		{
			observable<i_users::i_subscriber>::notify_observers(i_users::i_subscriber::NotifyUserListUpdating);
		}
		virtual void user_list_updated(irc::channel_buffer& aBuffer)
		{
			observable<i_users::i_subscriber>::notify_observers(i_users::i_subscriber::NotifyUserListUpdated);
		}
		// from neolib::i_settings::i_subscriber
		virtual void settings_changed(const neolib::i_string& aSettingCategory) {}
		virtual void setting_changed(const neolib::i_setting& aSetting)
		{
			if (aSetting.category() == "Formatting" && 
				(aSetting.name() == "InputWordwrap" || 
				aSetting.name() == "InputMin" || 
				aSetting.name() == "InputMax" ||
				aSetting.name() == "InputNewLineModifierKey" ||
				aSetting.name() == "InputHistoryModifierKey"))
			{
				observable<i_buffer::i_subscriber>::notify_observers(i_buffer::i_subscriber::NotifyInputInfoChanged);
			}
		}
		virtual void setting_deleted(const neolib::i_setting& aSetting) {}
		virtual bool interested_in_dirty_settings() const { return false; }

		// from neolib::observable<caw::i_buffer::i_subscriber>
	private:
		virtual void notify_observer(const i_buffer::i_subscriber& aObserver, i_buffer::i_subscriber::notify_type aType, const void* aParameter, const void* aParameter2) const
		{
			switch (aType)
			{
			case i_buffer::i_subscriber::NotifyCanActivateBuffer:
				aObserver.can_activate_buffer(*this, *static_cast<bool*>(const_cast<void*>(aParameter)));
				break;
			}
		}
		virtual void notify_observer(i_buffer::i_subscriber& aObserver, i_buffer::i_subscriber::notify_type aType, const void* aParameter, const void* aParameter2)
		{
			switch (aType)
			{
			case i_buffer::i_subscriber::NotifyNewMessage:
				aObserver.new_message(*this, *static_cast<const caw::i_buffer_message*>(aParameter));
				break;
			case i_buffer::i_subscriber::NotifyClosing:
				aObserver.closing(*this);
				break;
			case i_buffer::i_subscriber::NotifyNameChanged:
				aObserver.name_changed(*this, *static_cast<const neolib::i_string*>(aParameter));
				break;
			case i_buffer::i_subscriber::NotifyTitleChanged:
				aObserver.title_changed(*this, *static_cast<const neolib::i_string*>(aParameter));
				break;
			case i_buffer::i_subscriber::NotifyActivateBuffer:
				aObserver.activate_buffer(*this);
				break;
			case i_buffer::i_subscriber::NotifyInputInfoChanged:
				aObserver.input_info_changed(*this, *this);
				break;
			case i_buffer::i_subscriber::NotifyIsWeakObserver:
				if (!aObserver.is_weak_observer(*this))
					*static_cast<bool*>(const_cast<void*>(aParameter)) = true;
				break;
			}
		}
		// from neolib::observable<caw::i_users::i_subscriber>
	private:
		virtual void notify_observer(i_users::i_subscriber& aObserver, i_users::i_subscriber::notify_type aType, const void* aParameter, const void* aParameter2)
		{
			switch (aType)
			{
			case i_users::i_subscriber::NotifyUserAdded:
				aObserver.user_added(*static_cast<const caw::i_user*>(aParameter));
				break;
			case i_users::i_subscriber::NotifyUserUpdated:
				aObserver.user_updated(*static_cast<const caw::i_user*>(aParameter), *static_cast<const caw::i_user*>(aParameter2));
				break;
			case i_users::i_subscriber::NotifyUserRemoved:
				aObserver.user_removed(*static_cast<const caw::i_user*>(aParameter));
				break;
			case i_users::i_subscriber::NotifyUserListUpdating:
				aObserver.user_list_updating();
				break;
			case i_users::i_subscriber::NotifyUserListUpdated:
				aObserver.user_list_updated();
				break;
			}
		}

		// attributes
	private:
		neolib::i_plugin& iPlugin;
		caw::i_gui& iGui;
		neolib::i_settings& iSettings;
		irc::buffer& iIrcBuffer;
		caw::i_buffer_source& iBufferSource;
		neolib::string iId;
		neolib::string iName;
		neolib::string iTitle;
		message_list iMessages;
		bool iDoingTabCompletion;
		std::string iTabCompletionPrefix;
		neolib::optional<int> iTabCompletionCounter;
		std::string iTabCompletionMatch;
		std::string iTabCompletionPreviousMatch;
		concrete_user_list_type iUsers;
		user_map iUserMap;
	};
}