// channel_modes.cpp
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
#include "channel_modes.hpp"
#include "mode.hpp"

namespace irc
{
	channel_modes::channel_modes(channel_buffer& aChannel) : iParent(aChannel), 
		iGotBanList(true), iGotExceptList(true), iGotInviteList(true), iModes(None), iGotModes(false)
	{
		iParent.connection().add_observer(*this);
		message modeMessage(iParent, message::OUTGOING);
		modeMessage.set_command(message::MODE);
		modeMessage.parameters().push_back(aChannel.name());
		iParent.send_message(modeMessage);
	}

	channel_modes::~channel_modes()
	{
		iParent.connection().remove_observer(*this);
	}

	void channel_modes::notify_observer(channel_modes_observer& aObserver, channel_modes_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case channel_modes_observer::NotifyBanListEntryAdded:
			aObserver.channel_modes_ban_list_entry_added(*this, *static_cast<const channel_modes_entry*>(aParameter));
			break;
		case channel_modes_observer::NotifyBanListEntryRemoved:
			aObserver.channel_modes_ban_list_entry_removed(*this, *static_cast<const channel_modes_entry*>(aParameter));
			break;
		case channel_modes_observer::NotifyBanListReset:
			aObserver.channel_modes_ban_list_reset(*this);
			break;
		case channel_modes_observer::NotifyExceptListEntryAdded:
			aObserver.channel_modes_except_list_entry_added(*this, *static_cast<const channel_modes_entry*>(aParameter));
			break;
		case channel_modes_observer::NotifyExceptListEntryRemoved:
			aObserver.channel_modes_except_list_entry_removed(*this, *static_cast<const channel_modes_entry*>(aParameter));
			break;
		case channel_modes_observer::NotifyExceptListReset:
			aObserver.channel_modes_except_list_reset(*this);
			break;
		case channel_modes_observer::NotifyInviteListEntryAdded:
			aObserver.channel_modes_invite_list_entry_added(*this, *static_cast<const channel_modes_entry*>(aParameter));
			break;
		case channel_modes_observer::NotifyInviteListEntryRemoved:
			aObserver.channel_modes_invite_list_entry_removed(*this, *static_cast<const channel_modes_entry*>(aParameter));
			break;
		case channel_modes_observer::NotifyInviteListReset:
			aObserver.channel_modes_invite_list_reset(*this);
			break;
		case channel_modes_observer::NotifyListDone:
			aObserver.channel_modes_list_done(*this);
			break;
		case channel_modes_observer::NotifyModesUpdated:
			aObserver.channel_modes_modes_updated(*this);
			break;
		}
	}

	struct compare_mode_entry
	{
		string iSearchTerm;
		compare_mode_entry(const string& aSearchTerm) : iSearchTerm(aSearchTerm) {}
		bool operator()(const channel_modes_entry& aEntry) const
		{
			return iSearchTerm == aEntry.iUser;
		}
	};

	void channel_modes::incoming_message(connection& aConnection, const message& aMessage)
	{
		switch(aMessage.command())
		{
		case message::RPL_BANLIST:
			if (iGotBanList)
			{
				iGotBanList = false;
				iBanList.clear();
				notify_observers(channel_modes_observer::NotifyBanListReset);
			}
			if (aMessage.parameters().size() < 2)
				break;
			if (irc::make_string(aConnection, aMessage.parameters()[0]) != iParent.name())
				break;
			iBanList.push_back(channel_modes_entry(irc::make_string(aConnection, aMessage.parameters()[1]), 
				aMessage.parameters().size() >= 3 ? irc::make_string(aConnection, aMessage.parameters()[2]) : irc::make_string(aConnection, std::string("")),
				aMessage.parameters().size() >= 4 ? channel_modes_entry::date(neolib::string_to_integer(aMessage.parameters()[3])) : channel_modes_entry::date()));
			notify_observers(channel_modes_observer::NotifyBanListEntryAdded, iBanList.back());
			break;
		case message::RPL_ENDOFBANLIST:
			iGotBanList = true;
			notify_observers(channel_modes_observer::NotifyListDone);
			break;
		case message::RPL_EXCEPTLIST:
			if (iGotExceptList)
			{
				iGotExceptList = false;
				iExceptList.clear();
				notify_observers(channel_modes_observer::NotifyExceptListReset);
			}
			if (aMessage.parameters().size() < 2)
				break;
			if (irc::make_string(aConnection, aMessage.parameters()[0]) != iParent.name())
				break;
			iExceptList.push_back(channel_modes_entry(irc::make_string(aConnection, aMessage.parameters()[1]), 
				aMessage.parameters().size() >= 3 ? irc::make_string(aConnection, aMessage.parameters()[2]) : irc::make_string(aConnection, std::string("")),
				aMessage.parameters().size() >= 4 ? channel_modes_entry::date(neolib::string_to_integer(aMessage.parameters()[3])) : channel_modes_entry::date()));
			notify_observers(channel_modes_observer::NotifyExceptListEntryAdded, iExceptList.back());
			break;
		case message::RPL_ENDOFEXCEPTLIST:
			iGotExceptList = true;
			notify_observers(channel_modes_observer::NotifyListDone);
			break;
		case message::RPL_INVITELIST:
			if (iGotInviteList)
			{
				iGotInviteList = false;
				iInviteList.clear();
				notify_observers(channel_modes_observer::NotifyInviteListReset);
			}
			if (aMessage.parameters().size() < 2)
				break;
			if (irc::make_string(aConnection, aMessage.parameters()[0]) != iParent.name())
				break;
			iInviteList.push_back(channel_modes_entry(irc::make_string(aConnection, aMessage.parameters()[1]), 
				aMessage.parameters().size() >= 3 ? irc::make_string(aConnection, aMessage.parameters()[2]) : irc::make_string(aConnection, std::string("")),
				aMessage.parameters().size() >= 4 ? channel_modes_entry::date(neolib::string_to_integer(aMessage.parameters()[3])) : channel_modes_entry::date()));
			notify_observers(channel_modes_observer::NotifyInviteListEntryAdded, iInviteList.back());
			break;
		case message::RPL_ENDOFINVITELIST:
			iGotInviteList = true;
			notify_observers(channel_modes_observer::NotifyListDone);
			break;
		case message::RPL_CHANNELMODEIS:
			iGotModes = true;
			iModes = None;
			// fall through...
		case message::MODE:
			{
				if (aMessage.parameters().size() < 2)
					break;
				if (!aConnection.is_channel(aMessage.parameters()[0]))
					break;
				if (irc::make_string(aConnection, aMessage.parameters()[0]) != iParent.name())
					break;
				bool modesUpdated = false;
				mode::list modes;
				parse_mode(aMessage, modes);
				if (modes.empty())
					modesUpdated = true;
				for (mode::list::const_iterator i = modes.begin(); i != modes.end(); ++i)
				{
					const mode& currentMode = *i;
					switch(currentMode.iType)
					{
					case mode::BAN:
						if (currentMode.iDirection == mode::ADD)
						{
							iBanList.push_back(channel_modes_entry(irc::make_string(aConnection, currentMode.iParameter), 
								irc::make_string(aConnection, aMessage.origin()), channel_modes_entry::date(time(0))));
							notify_observers(channel_modes_observer::NotifyBanListEntryAdded, iBanList.back());
						}
						else
						{
							container_type::iterator i = std::find_if(iBanList.begin(), iBanList.end(), compare_mode_entry(irc::make_string(aConnection, currentMode.iParameter)));
							if (i != iBanList.end())
							{								
								notify_observers(channel_modes_observer::NotifyBanListEntryRemoved, *i);
								iBanList.erase(i);
							}
						}
						break;
					case mode::EXCEPT:
						if (currentMode.iDirection == mode::ADD)
						{
							iExceptList.push_back(channel_modes_entry(irc::make_string(aConnection, currentMode.iParameter), 
								irc::make_string(aConnection, aMessage.origin()), channel_modes_entry::date(time(0))));
							notify_observers(channel_modes_observer::NotifyExceptListEntryAdded, iExceptList.back());
						}
						else
						{
							container_type::iterator i = std::find_if(iExceptList.begin(), iExceptList.end(), compare_mode_entry(irc::make_string(aConnection, currentMode.iParameter)));
							if (i != iExceptList.end())
							{								
								notify_observers(channel_modes_observer::NotifyExceptListEntryRemoved, *i);
								iExceptList.erase(i);
							}
						}
						break;
					case mode::INVITE:
						if (currentMode.iDirection == mode::ADD)
						{
							iInviteList.push_back(channel_modes_entry(irc::make_string(aConnection, currentMode.iParameter), 
								irc::make_string(aConnection, aMessage.origin()), channel_modes_entry::date(time(0))));
							notify_observers(channel_modes_observer::NotifyInviteListEntryAdded, iInviteList.back());
						}
						else
						{
							container_type::iterator i = std::find_if(iInviteList.begin(), iInviteList.end(), compare_mode_entry(irc::make_string(aConnection, currentMode.iParameter)));
							if (i != iInviteList.end())
							{								
								notify_observers(channel_modes_observer::NotifyInviteListEntryRemoved, *i);
								iInviteList.erase(i);
							}
						}
						break;
					case mode::ANONYMOUS:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | Anonymous);
							else 
								iModes = static_cast<modes_e>(iModes & ~Anonymous);
							modesUpdated = true;
						}
						break;
					case mode::INVITEONLY:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | InviteOnly);
							else 
								iModes = static_cast<modes_e>(iModes & ~InviteOnly);
							modesUpdated = true;
						}
						break;
					case mode::MODERATED:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | Moderated);
							else 
								iModes = static_cast<modes_e>(iModes & ~Moderated);
							modesUpdated = true;
						}
						break;
					case mode::NOOUTSIDEMESSAGES:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | NoOutsideMessages);
							else 
								iModes = static_cast<modes_e>(iModes & ~NoOutsideMessages);
							modesUpdated = true;
						}
						break;
					case mode::QUIET:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | Quiet);
							else 
								iModes = static_cast<modes_e>(iModes & ~Quiet);
							modesUpdated = true;
						}
						break;
					case mode::PRIVATE:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | Private);
							else 
								iModes = static_cast<modes_e>(iModes & ~Private);
							modesUpdated = true;
						}
						break;
					case mode::SECRET:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | Secret);
							else 
								iModes = static_cast<modes_e>(iModes & ~Secret);
							modesUpdated = true;
						}
						break;
					case mode::SERVERREOP:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | ServerReop);
							else 
								iModes = static_cast<modes_e>(iModes & ~ServerReop);
							modesUpdated = true;
						}
						break;
					case mode::TOPICOPCHANGE:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
								iModes = static_cast<modes_e>(iModes | Topic);
							else 
								iModes = static_cast<modes_e>(iModes & ~Topic);
							modesUpdated = true;
						}
						break;
					case mode::KEY:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
							{
								iModes = static_cast<modes_e>(iModes | ChannelKey);
								iChannelKey = currentMode.iParameter;
							}
							else 
								iModes = static_cast<modes_e>(iModes & ~ChannelKey);
							modesUpdated = true;
						}
						break;
					case mode::USERLIMIT:
						if (iGotModes)
						{
							if (currentMode.iDirection == mode::ADD)
							{
								iModes = static_cast<modes_e>(iModes | UserLimit);
								iUserLimit = currentMode.iParameter;
							}
							else 
								iModes = static_cast<modes_e>(iModes & ~UserLimit);
							modesUpdated = true;
						}
						break;
					}
				}
				if (modesUpdated)
					notify_observers(channel_modes_observer::NotifyModesUpdated);
			}
			break;
		default:
			break;
		}
	}

	void channel_modes::set_modes(modes_e aNewModes, const std::string& aChannelKey, const std::string& aUserLimit)
	{
		message modeMessage(iParent, message::OUTGOING);
		modeMessage.set_command(message::MODE);
		modeMessage.parameters().push_back(iParent.name());
		if ((aNewModes & UserLimit) != (iModes & UserLimit) || aUserLimit != iUserLimit)
		{
			if (aNewModes & UserLimit)
			{
				modeMessage.parameters().push_back("+l");
				modeMessage.parameters().push_back(aUserLimit);
				iParent.send_message(modeMessage);
				modeMessage.parameters().pop_back();
				modeMessage.parameters().pop_back();
			}
			else
			{
				modeMessage.parameters().push_back("-l");
				iParent.send_message(modeMessage);
				modeMessage.parameters().pop_back();
			}
		}
		if ((aNewModes & ChannelKey) != (iModes & ChannelKey) || aChannelKey != iChannelKey)
		{
			if (aNewModes & ChannelKey)
			{
				modeMessage.parameters().push_back("+k");
				modeMessage.parameters().push_back(aChannelKey);
				iParent.send_message(modeMessage);
				modeMessage.parameters().pop_back();
				modeMessage.parameters().pop_back();
			}
			else
			{
				modeMessage.parameters().push_back("-k");
				iParent.send_message(modeMessage);
				modeMessage.parameters().pop_back();
			}
		}
		modes_e checkModes[] = 
		{ 
			ServerReop, Secret, Quiet, Private, NoOutsideMessages,
			Moderated, InviteOnly, Anonymous, Topic
		};
		mode::type checkTypes[] = 
		{
			mode::SERVERREOP, mode::SECRET, mode::QUIET,
			mode::PRIVATE, mode::NOOUTSIDEMESSAGES, mode::MODERATED,
			mode::INVITEONLY, mode::ANONYMOUS, mode::TOPICOPCHANGE
		};
		for (int i = 0; i < sizeof(checkModes)/sizeof(checkModes[0]); ++i)
		{
			if ((aNewModes & checkModes[i]) != (iModes & checkModes[i]))
			{
				std::string modeParam;
				if (aNewModes & checkModes[i])
					modeParam = "+";
				else
					modeParam = "-";
				modeParam.append(1, checkTypes[i]);
				modeMessage.parameters().push_back(modeParam);
				iParent.send_message(modeMessage);
				modeMessage.parameters().pop_back();
			}
		}
		iModes = aNewModes;
		iChannelKey = aChannelKey;
		iUserLimit = aUserLimit;
	}
}