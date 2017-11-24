// message.cpp
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
#include <array>
#include <ctime>
#include <neoirc/client/message.hpp>
#include <neoirc/client/message_strings.hpp>
#include <neoirc/client/user.hpp>
#include <neoirc/client/connection_manager.hpp>
#include <neoirc/client/connection.hpp>
#include <neoirc/client/buffer.hpp>
#include <neoirc/client/codes.hpp>
#include <neoirc/client/timestamp.hpp>

namespace irc
{
	message::message(connection_manager& aConnectionManager, direction_e aDirection, bool aFromLog) : 
		iId(aConnectionManager.next_message_id()), iFromLog(aFromLog), iBufferRequired(true), iTime(::time(0)), iDirection(aDirection), iCommand(UNKNOWN) 
	{
	}

	message::message(connection& aConnection, direction_e aDirection, bool aFromLog) : 
		iId(aConnection.next_message_id()), iFromLog(aFromLog), iBufferRequired(true), iTime(::time(0)), iDirection(aDirection), iCommand(UNKNOWN) 
	{
	}

	message::message(buffer& aBuffer, direction_e aDirection, bool aFromLog) :
		iId(aBuffer.next_message_id()), iFromLog(aFromLog), iBufferRequired(true), iTime(::time(0)), iDirection(aDirection), iCommand(UNKNOWN) 
	{
	}

	const struct string_command
	{
		const char* iString;
		message::command_e iCommand;
	} sStringCommandList[] =
	{
		{"PRIVMSG", message::PRIVMSG}, 
		{"NOTICE", message::NOTICE},
		{"NICK", message::NICK},
		{"USER", message::USER},
		{"PASS", message::PASS},
		{"QUIT", message::QUIT},
		{"JOIN", message::JOIN},
		{"PART", message::PART},
		{"TOPIC", message::TOPIC},
		{"PING", message::PING},
		{"PONG", message::PONG},
		{"MODE", message::MODE},
		{"WHOIS", message::WHOIS},
		{"WHO", message::WHO},
		{"KICK", message::KICK},
		{"LIST", message::LIST},
		{"INVITE", message::INVITE},
		{"AWAY", message::AWAY},
	};

	const struct numeric_reply
	{
		unsigned int iNumber;
		message::command_e iCommand;
	} sNumericReplyList[] = 
	{
		{1, message::RPL_WELCOME},
		{2, message::RPL_YOURHOST},
		{3, message::RPL_CREATED},
		{4, message::RPL_MYINFO},
		{5, message::RPL_BOUNCE},
		{302, message::RPL_USERHOST},
		{303, message::RPL_ISON},
		{301, message::RPL_AWAY},
		{305, message::RPL_UNAWAY},
		{306, message::RPL_NOWAWAY},
		{307, message::RPL_WHOISREGNICK},
		{308, message::RPL_WHOISADMIN},
		{309, message::RPL_WHOISSADMIN},
		{310, message::RPL_WHOISSVCMSG},
		{311, message::RPL_WHOISUSER},
		{312, message::RPL_WHOISSERVER},
		{313, message::RPL_WHOISOPERATOR},
		{314, message::RPL_WHOWASUSER},
		{317, message::RPL_WHOISIDLE},
		{318, message::RPL_ENDOFWHOIS},
		{319, message::RPL_WHOISCHANNELS},
		{320, message::RPL_WHOISEXTRA},
		{369, message::RPL_ENDOFWHOWAS},
		{321, message::RPL_LISTSTART},
		{322, message::RPL_LIST},
		{323, message::RPL_LISTEND},
		{325, message::RPL_UNIQOPIS},
		{324, message::RPL_CHANNELMODEIS},
		{329, message::RPL_CREATIONTIME},
		{331, message::RPL_NOTOPIC},
		{332, message::RPL_TOPIC},
		{333, message::RPL_TOPICAUTHOR},
		{341, message::RPL_INVITING},
		{342, message::RPL_SUMMONING},
		{346, message::RPL_INVITELIST},
		{347, message::RPL_ENDOFINVITELIST},
		{348, message::RPL_EXCEPTLIST},
		{349, message::RPL_ENDOFEXCEPTLIST},
		{351, message::RPL_VERSION},
		{352, message::RPL_WHOREPLY},
		{315, message::RPL_ENDOFWHO},
		{353, message::RPL_NAMREPLY},
		{366, message::RPL_ENDOFNAMES},
		{364, message::RPL_LINKS},
		{365, message::RPL_ENDOFLINKS},
		{367, message::RPL_BANLIST},
		{368, message::RPL_ENDOFBANLIST},
		{371, message::RPL_INFO},
		{374, message::RPL_ENDOFINFO},
		{375, message::RPL_MOTDSTART},
		{372, message::RPL_MOTD},
		{376, message::RPL_ENDOFMOTD},
		{381, message::RPL_YOUREOPER},
		{382, message::RPL_REHASHING},
		{383, message::RPL_YOURESERVICE},
		{391, message::RPL_TIME},
		{392, message::RPL_USERSSTART},
		{393, message::RPL_USERS},
		{394, message::RPL_ENDOFUSERS},
		{395, message::RPL_NOUSERS},
		{200, message::RPL_TRACELINK},
		{201, message::RPL_TRACECONNECTING},
		{202, message::RPL_TRACEHANDSHAKE},
		{203, message::RPL_TRACEUNKNOWN},
		{204, message::RPL_TRACEOPERATOR},
		{205, message::RPL_TRACEUSER},
		{206, message::RPL_TRACESERVER},
		{207, message::RPL_TRACESERVICE},
		{208, message::RPL_TRACENEWTYPE},
		{209, message::RPL_TRACECLASS},
		{210, message::RPL_TRACERECONNECT},
		{261, message::RPL_TRACELOG},
		{262, message::RPL_TRACEEND},
		{211, message::RPL_STATSLINKINFO},
		{212, message::RPL_STATSCOMMANDS},
		{219, message::RPL_ENDOFSTATS},
		{242, message::RPL_STATSUPTIME},
		{243, message::RPL_STATSOLINE},
		{221, message::RPL_UMODEIS},
		{234, message::RPL_SERVLIST},
		{235, message::RPL_SERVLISTEND},
		{251, message::RPL_LUSERCLIENT},
		{252, message::RPL_LUSEROP},
		{253, message::RPL_LUSERUNKNOWN},
		{254, message::RPL_LUSERCHANNELS},
		{255, message::RPL_LUSERME},
		{256, message::RPL_ADMINME},
		{257, message::RPL_ADMINLOC1},
		{258, message::RPL_ADMINLOC2},
		{259, message::RPL_ADMINEMAIL},
		{263, message::RPL_TRYAGAIN},
		{401, message::ERR_NOSUCHNICK},
		{402, message::ERR_NOSUCHSERVER},
		{403, message::ERR_NOSUCHCHANNEL},
		{404, message::ERR_CANNOTSENDTOCHAN},
		{405, message::ERR_TOOMANYCHANNELS},
		{406, message::ERR_WASNOSUCHNICK},
		{407, message::ERR_TOOMANYTARGETS},
		{408, message::ERR_NOSUCHSERVICE},
		{409, message::ERR_NOORIGIN},
		{411, message::ERR_NORECIPIENT},
		{412, message::ERR_NOTEXTTOSEND},
		{413, message::ERR_NOTOPLEVEL},
		{414, message::ERR_WILDTOPLEVEL},
		{415, message::ERR_BADMASK},
		{421, message::ERR_UNKNOWNCOMMAND},
		{422, message::ERR_NOMOTD},
		{423, message::ERR_NOADMININFO},
		{424, message::ERR_FILEERROR},
		{431, message::ERR_NONICKNAMEGIVEN},
		{432, message::ERR_ERRONEUSNICKNAME},
		{433, message::ERR_NICKNAMEINUSE},
		{436, message::ERR_NICKCOLLISION},
		{437, message::ERR_UNAVAILRESOURCE},
		{441, message::ERR_USERNOTINCHANNEL},
		{442, message::ERR_NOTONCHANNEL},
		{443, message::ERR_USERONCHANNEL},
		{444, message::ERR_NOLOGIN},
		{445, message::ERR_SUMMONDISABLED},
		{446, message::ERR_USERSDISABLED},
		{451, message::ERR_NOTREGISTERED},
		{461, message::ERR_NEEDMOREPARAMS},
		{462, message::ERR_ALREADYREGISTRED},
		{463, message::ERR_NOPERMFORHOST},
		{464, message::ERR_PASSWDMISMATCH},
		{465, message::ERR_YOUREBANNEDCREEP},
		{466, message::ERR_YOUWILLBEBANNED},
		{467, message::ERR_KEYSET},
		{471, message::ERR_CHANNELISFULL},
		{472, message::ERR_UNKNOWNMODE},
		{473, message::ERR_INVITEONLYCHAN},
		{474, message::ERR_BANNEDFROMCHAN},
		{475, message::ERR_BADCHANNELKEY},
		{476, message::ERR_BADCHANMASK},
		{477, message::ERR_NOCHANMODES},
		{478, message::ERR_BANLISTFULL},
		{481, message::ERR_NOPRIVILEGES},
		{482, message::ERR_CHANOPRIVSNEEDED},
		{483, message::ERR_CANTKILLSERVER},
		{484, message::ERR_RESTRICTED},
		{485, message::ERR_UNIQOPPRIVSNEEDED},
		{491, message::ERR_NOOPERHOST},
		{501, message::ERR_UMODEUNKNOWNFLAG},
		{502, message::ERR_USERSDONTMATCH},
		{231, message::RPL_SERVICEINFO},
		{232, message::RPL_ENDOFSERVICES},
		{233, message::RPL_SERVICE},
		{300, message::RPL_NONE},
		{316, message::RPL_WHOISCHANOP},
		{361, message::RPL_KILLDONE},
		{362, message::RPL_CLOSING},
		{363, message::RPL_CLOSEEND},
		{373, message::RPL_INFOSTART},
		{384, message::RPL_MYPORTIS},
		{213, message::RPL_STATSCLINE},
		{214, message::RPL_STATSNLINE},
		{215, message::RPL_STATSILINE},
		{216, message::RPL_STATSKLINE},
		{217, message::RPL_STATSQLINE},
		{218, message::RPL_STATSYLINE},
		{240, message::RPL_STATSVLINE},
		{241, message::RPL_STATSLLINE},
		{244, message::RPL_STATSHLINE},
		{244, message::RPL_STATSSLINE},
		{246, message::RPL_STATSPING},
		{247, message::RPL_STATSBLINE},
		{250, message::RPL_STATSDLINE},
		{492, message::ERR_NOSERVICEHOST}
	};

	void message::parse_command(const std::string& aMessage)
	{
		iTarget = "";

		std::vector<std::string> words;
		neolib::tokens(aMessage, std::string(" \r\n"), words, 3);
		if (words.empty())
			return;

		bool havePrefix = words[0][0] == ':';
		if (havePrefix && words.size() < 2)
			return;

		if (havePrefix)
			iOrigin = words[0].substr(1);

		set_command(havePrefix ? words[1] : words[0]);
		if (is_numeric_reply())
		{
			if (words.size() >= static_cast<std::size_t>((havePrefix ? 3 : 2)))
				iTarget = havePrefix ? words[2] : words[1];
		}
	}

	void message::parse_parameters(const std::string& aMessage, bool aHasTarget, bool aFromServer)
	{
		iParameters.clear();

		typedef std::vector<std::pair<std::string::const_iterator, std::string::const_iterator> > words_t;
		words_t words;
		std::string space(" \r\n");
		neolib::tokens(aMessage.begin(), aMessage.end(), space.begin(), space.end(), words, 17);
		bool gotCommand = false;
		bool gotTarget = iTarget.empty() && !aHasTarget;
		std::size_t is_content = content_param();
		for (words_t::iterator i = words.begin(); i != words.end(); ++i)
		{
			if (*(i->first) == ':' && aFromServer)
			{
				if (i != words.begin())
				{
					iParameters.push_back(std::string(i->first+1, aMessage.end()));
					neolib::remove_trailing(iParameters.back(), std::string("\r\n"));
					break;
				}
			}
			else if (gotCommand && gotTarget)
			{
				if (is_content-- == 0 && !aFromServer)
				{
					iParameters.push_back(std::string(i->first, aMessage.end()));
					neolib::remove_trailing(iParameters.back(), std::string("\r\n"));
					break;
				}
				iParameters.push_back(std::string(i->first, i->second));
			}
			else if (gotCommand)
			{
				if (aHasTarget)
				{
					iTarget = std::string(i->first, i->second);
					iParameters.push_back(iTarget);
					--is_content;
				}
				gotTarget = true;
			}
			else
				gotCommand = true;
		}
	}

	bool message::parse_log(const std::string& aLogEntry)
	{
		typedef std::pair<std::string::const_iterator, std::string::const_iterator> word_t;
		neolib::vecarray<word_t, 3> words;
		std::string sep(" ");
		neolib::tokens(aLogEntry.begin(), aLogEntry.end(), sep.begin(), sep.end(), words, 3, false);
		if (words.size() != 3 || words[0].first == words[0].second || words[1].first == words[1].second || words[2].first == words[2].second)
			return false;
		iTime = neolib::string_to_unsigned_integer(std::string(words[0].first, words[0].second));
		iDirection = (std::string(words[1].first, words[1].second) == "<" ? INCOMING : OUTGOING);
		std::string actual(words[2].first, aLogEntry.end());
		parse_command(actual);
		parse_parameters(actual, false, true);
		return true;
	}

	std::size_t message::content_param() const
	{
		switch(iCommand)
		{
		case QUIT:
			return 0;
		case NOTICE:
		case PRIVMSG:
		case TOPIC:
		case RPL_TOPIC:
		case RPL_AWAY:
		case PART:
			return 1;
		case KICK:
		case RPL_LIST:
			return 2;
		case RPL_WHOISUSER:
			return 4;
		default:
			return no_content;
		}
	}

	const std::string& message::content() const
	{
		static const std::string none;
		std::size_t contentParam = content_param();
		if (contentParam < iParameters.size())
			return iParameters[contentParam];
		else
			return none;
	}

	std::string message::to_string(const message_strings& aMessageStrings, bool aAddPrefix, bool aAddTarget) const
	{
		std::string ret;

		if (aAddPrefix)
		{
			if (!iOrigin.empty())
				ret = ret + ":" + iOrigin + " ";
		}

		if (iCommand != UNKNOWN && iCommand != RPL_UNKNOWN)
			ret += command_to_string(iCommand);
		else
			ret += (iCommandString.size() != 0 ? iCommandString : std::string("#"));

		if (aAddTarget && !iTarget.empty())
		{
			ret += " ";
			ret += iTarget;
		}

		for (parameters_t::const_iterator i = iParameters.begin(); i != iParameters.end();)
		{
			const std::string& parameter = *i++;
			if (parameter.empty() && i != iParameters.end())
				continue;
			ret += " ";
			if (i == iParameters.end() && (parameter.empty() || parameter.find(' ') != std::string::npos || parameter[0] == ':'))
				ret += ":";
			ret += parameter;
		}

		if (iCommand == QUIT && iDirection == OUTGOING && iParameters.empty() && !aMessageStrings.own_quit_message().empty())
		{
			ret += " :" + aMessageStrings.own_quit_message();
		}

		if (ret.empty() || ret[ret.size() - 1] != '\n')
			ret += "\r\n";
		return ret;
	}

	namespace
	{
		std::string timestamp_bit(const char* aParameter, const tm* aTime)
		{
			std::tr1::array<char, 256> tempBit;
			strftime(&tempBit[0], tempBit.size(), aParameter, aTime);
			return std::string(&tempBit[0]);
		}
	}

	std::string message::to_nice_string(const message_strings& aMessageStrings, const buffer* aBuffer, const std::string& aAppendToContent, bool aIsSelf, bool aAddTimeStamp, bool aAppendCRLF, neolib::string_spans* aStringSpans) const
	{
		std::string ret;
		parameters_t::const_iterator trailing = iParameters.end();
		bool replyMessage = true;
		bool appendContent = false;

		std::string theContent = content() + aAppendToContent;

		codes theCodes;

		irc::user userOrigin(iOrigin);

		if (iCommand < RPL_UNKNOWN)
		{
			if (iCommand != UNKNOWN)
			{
				switch(iCommand)
				{
				case NOTICE:
					{
						if (theContent.find("\001PING ") == 0 ||
							theContent.find("\001PONG ") == 0)
						{
							ret = std::string("%N% ping: %T%s");
							neolib::replace_string(ret, std::string("%N%"), userOrigin.nick_name(), aStringSpans, NickName);
							neolib::vecarray<std::string, 2> bits;
							neolib::tokens(theContent, std::string(" "), bits, 2);
							neolib::replace_string(ret, std::string("%T%"), bits.size() > 1 ? neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(iTime) - neolib::string_to_unsigned_integer(bits[1])): "???", aStringSpans);
						}
						else if (theContent.find("\001VERSION ") == 0)
						{
							ret = std::string("%N% version: %V%");
							neolib::replace_string(ret, std::string("%N%"), userOrigin.nick_name(), aStringSpans, NickName);
							neolib::vecarray<std::string, 2> bits;
							neolib::tokens(theContent.substr(9), std::string("\001"), bits, 1);
							neolib::replace_string(ret, std::string("%V%"), bits.size() == 1 ? bits[0] : "???", aStringSpans);
						}
						else if (theContent.find("\001TIME ") == 0)
						{
							ret = std::string("%N% time: %T%");
							neolib::replace_string(ret, std::string("%N%"), userOrigin.nick_name(), aStringSpans, NickName);
							neolib::vecarray<std::string, 2> bits;
							neolib::tokens(theContent.substr(6), std::string("\001"), bits, 1);
							neolib::replace_string(ret, std::string("%T%"), bits.size() == 1 ? bits[0] : "???", aStringSpans);
						}
						else if (theContent.find("\001FINGER ") == 0)
						{
							ret = std::string("%N% finger: %F%");
							neolib::replace_string(ret, std::string("%N%"), userOrigin.nick_name(), aStringSpans, NickName);
							neolib::vecarray<std::string, 2> bits;
							neolib::tokens(theContent.substr(8), std::string("\001"), bits, 1);
							neolib::replace_string(ret, std::string("%F%"), bits.size() == 1 ? bits[0] : "???", aStringSpans);
						}
						else if (theContent.find("\001SOURCE ") == 0)
						{
							ret = std::string("%N% source: %S%");
							neolib::replace_string(ret, std::string("%N%"), userOrigin.nick_name(), aStringSpans, NickName);
							neolib::vecarray<std::string, 2> bits;
							neolib::tokens(theContent.substr(8), std::string("\001"), bits, 1);
							neolib::replace_string(ret, std::string("%S%"), bits.size() == 1 ? bits[0] : "???", aStringSpans);
						}
						else if (theContent.find("\001USERINFO ") == 0)
						{
							ret = std::string("%N% userinfo: %U%");
							neolib::replace_string(ret, std::string("%N%"), userOrigin.nick_name(), aStringSpans, NickName);
							neolib::vecarray<std::string, 2> bits;
							neolib::tokens(theContent.substr(10), std::string("\001"), bits, 1);
							neolib::replace_string(ret, std::string("%U%"), bits.size() == 1 ? bits[0] : "???", aStringSpans);
						}
						else
						{
							ret = (iDirection == INCOMING ? aMessageStrings.notice_message() : aMessageStrings.sent_notice_message());
							if (!parameters().empty() && is_channel(aBuffer, parameters()[0]))
								theCodes["%C%"].first = parameters()[0];
							theCodes["%N%"] = std::make_pair(userOrigin.nick_name(), NickName);
							theCodes["%N!%"] =std::make_pair(userOrigin.msgto_form(), NickName);
							theCodes["%QN%"] = std::make_pair(aBuffer ? user(*aBuffer, userOrigin).qualified_name(userOrigin) : userOrigin.nick_name(), NickName);
							theCodes["%M%"].first = theContent;
							std::string target;
							if (!parameters().empty())
								target = parameters()[0];
							theCodes["%DN%"] = std::make_pair(target, is_channel(aBuffer, target) ? neolib::string_span::type() : NickName);
							replyMessage = false;
						}
						break;
					}
				case PRIVMSG:
					{
						if (is_ctcp())
							ret = aMessageStrings.ctcp_message();
						else if (neolib::to_upper(theContent).find("\001ACTION ") == 0)
						{
							ret = aMessageStrings.action_message();
							theContent = theContent.substr(8, theContent.size() - 9);
						}
						else
							ret = aMessageStrings.standard_message();
						theCodes["%N%"] = std::make_pair(userOrigin.nick_name(), NickName);
						theCodes["%N!%"] = std::make_pair(userOrigin.msgto_form(), NickName);
						theCodes["%QN%"] = std::make_pair(aBuffer ? user(*aBuffer, userOrigin).qualified_name(userOrigin) : userOrigin.nick_name(), NickName);
						theCodes["%M%"].first = theContent;
						if (is_ctcp())
						{
							neolib::vecarray<std::string, 1> request;
							neolib::tokens(theContent, std::string(" \001"), request, 1);
							theCodes["%R%"].first = request.empty() ? std::string("") : request[0];
						}
						replyMessage = false;
						break;
					}
				case NICK:
					{
						ret = aIsSelf ? aMessageStrings.nick_self_message() : aMessageStrings.nick_message();
						theCodes["%O%"].first = userOrigin.nick_name();
						theCodes["%O!%"].first = userOrigin.msgto_form();
						theCodes["%QN%"].first = userOrigin.qualified_name();
						std::string target;
						if (!parameters().empty())
							target = parameters()[0];
						theCodes["%N%"] = std::make_pair(target, NickName);
						break;
					}
				case KICK:
					{
						if (iParameters.size() >= 2)
						{
							ret = aIsSelf ? aMessageStrings.kick_self_message( ): aMessageStrings.kick_message();
							theCodes["%N%"] = std::make_pair(userOrigin.nick_name(), NickName);
							theCodes["%N!%"] = std::make_pair(userOrigin.msgto_form(), NickName);
							theCodes["%N1%"] = std::make_pair(userOrigin.nick_name(), NickName);
							theCodes["%QN%"] = std::make_pair(aBuffer ? user(*aBuffer, userOrigin).qualified_name(userOrigin) : userOrigin.nick_name(), NickName);
							theCodes["%N2%"].first = irc::user(iParameters[1]).nick_name();
							theCodes["%C%"].first = iParameters[0];
							theCodes["%R%"].first = theContent;			
						}
						else
						{
							ret += command_to_string(iCommand);
							trailing = iParameters.begin();
						}
						break;
					}
				case JOIN:
				case PART:
					{
						ret = (iCommand == JOIN ? aIsSelf ? aMessageStrings.join_self_message() : aMessageStrings.join_message() : aMessageStrings.part_message());
						theCodes["%N%"] = std::make_pair(userOrigin.nick_name(), NickName);
						theCodes["%N!%"] = std::make_pair(userOrigin.msgto_form(), NickName);
						theCodes["%QN%"] = std::make_pair(aBuffer ? user(*aBuffer, userOrigin).qualified_name(userOrigin) : userOrigin.nick_name(), NickName);
						std::string target;
						for (parameters_t::const_iterator i = parameters().begin(); i != parameters().end(); ++i)
							if (is_channel(aBuffer, *i))
							{
								target = *i;
								break;
							}
						theCodes["%C%"].first = target;
						if (iCommand == PART)
						{
							if (ret.find("%R%") != std::string::npos)
								theCodes["%R%"].first = theContent;
							else
								appendContent = true;
						}	
						break;
					}
				case QUIT:
					{
						ret = aMessageStrings.quit_message();
						theCodes["%N%"] = std::make_pair(userOrigin.nick_name(), NickName);
						theCodes["%N!%"] = std::make_pair(userOrigin.msgto_form(), NickName);
						theCodes["%QN%"] = std::make_pair(aBuffer ? user(*aBuffer, userOrigin).qualified_name(userOrigin) : userOrigin.nick_name(), NickName);
						theCodes["%R%"].first = theContent;	
						break;
					}
				case TOPIC:
					{
						ret = aMessageStrings.topic_change_message();
						theCodes["%T%"].first = theContent;
						theCodes["%N%"] = std::make_pair(userOrigin.nick_name(), NickName);
						theCodes["%N!%"] = std::make_pair(userOrigin.msgto_form(), NickName);
						theCodes["%QN%"] = std::make_pair(aBuffer ? user(*aBuffer, userOrigin).qualified_name(userOrigin) : userOrigin.nick_name(), NickName);
						break;
					}
				case MODE:
					if (iParameters.size() > 0)
					{
						ret = aMessageStrings.mode_message();
						theCodes["%N%"] = std::make_pair(userOrigin.nick_name(), NickName);
						theCodes["%N!%"] = std::make_pair(userOrigin.msgto_form(), NickName);
						theCodes["%QN%"] = std::make_pair(aBuffer ? user(*aBuffer, userOrigin).qualified_name(userOrigin) : userOrigin.nick_name(), NickName);
						std::string rest;
						if (is_channel(aBuffer, iParameters[0]))
						{
							for (parameters_t::const_iterator i = iParameters.begin() + 1; i != iParameters.end(); ++i)
							{
								const std::string& parameter = *i;
								if (!rest.empty())
									rest += " ";
								rest += parameter;
							}
						}
						else
						{
							rest = iParameters[1];
						}
						theCodes["%M%"].first = rest;
					}
					else
					{
						ret += command_to_string(iCommand);
						trailing = iParameters.begin();
					}
					break;
				default:
					ret += command_to_string(iCommand);
					trailing = iParameters.begin();
					break;
				}
			}
			else
			{
				if (iCommandString != "#")
					ret += iCommandString;
				trailing = iParameters.begin();
			}
		}
		else
		{
			if (iCommand != RPL_UNKNOWN)
			{
				switch(iCommand)
				{
				case RPL_TOPIC:
					{
						ret = aMessageStrings.topic_message();
						theCodes["%T%"].first = theContent;
						break;
					}
				case RPL_TOPICAUTHOR:
					{
						if (iParameters.size() < 2)
							return "";
						ret = aMessageStrings.topic_author_message();
						theCodes["%N%"].first = irc::user(iParameters[1]).nick_name();
						if (iParameters.size() < 3)
							theCodes["%D%"].first = std::string("");
						else
						{
							time_t ttTime = neolib::string_to_integer(iParameters[2]);
							tm* tmTime = localtime(&ttTime);
							std::tr1::array<char, 256> strTime;
							setlocale(LC_TIME, "");
							strftime(&strTime[0], strTime.size(), "%a %b %#d %H:%M:%S %Y", tmTime);
							theCodes["%D%"].first = std::string(&strTime[0]);
						}
						break;
					}
				case message::RPL_WHOISCHANNELS:
					{
						if (iParameters.size() < 2)
							return "";
						ret = std::string("%N% is on channel(s)");
						theCodes["%N%"].first = iParameters[0];
						for (parameters_t::const_iterator i = iParameters.begin() + 1; i != iParameters.end(); ++i)
						{
							std::vector<std::string> channels;
							neolib::tokens(*i, std::string(" "), channels);
							for (std::vector<std::string>::const_iterator j = channels.begin(); j != channels.end(); ++j)
							{
								std::string::size_type offset = j->find_first_of(chantypes(aBuffer));
								if (!j->empty() && offset != std::string::npos)
								{
									ret += " ";
									ret += *j;
									if (aStringSpans)
										aStringSpans->push_back(neolib::string_span(ret.size() - j->size() + offset, ret.size(), Channel));
								}
							}
						}
						break;
					}
				case message::RPL_WHOISUSER:
					{
						if (iParameters.size() < 3)
							return "";
						ret = std::string("%N% is %U%@%H%%?FN% (%FN%)%?%");
						theCodes["%N%"].first = iParameters[0];
						theCodes["%U%"].first = iParameters[1];
						theCodes["%H%"].first = iParameters[2];
						if (iParameters.size() > 4)
							theCodes["%FN%"].first =  iParameters[4] + aAppendToContent;
						break;
					}
				case message::RPL_WHOISSERVER:
					{
						if (iParameters.size() < 2)
							return "";
						ret = std::string("%N% is using server %S%%?I% (%I%)%?%");
						theCodes["%N%"].first = iParameters[0];
						theCodes["%S%"].first = iParameters[1];
						if (iParameters.size() > 2)
							theCodes["%I%"].first = iParameters[2];
						break;
					}
				case message::RPL_WHOISIDLE:
					{
						std::string idleTime;
						if (iParameters.size() >= 2)
						{
							unsigned long left = neolib::string_to_unsigned_integer(iParameters[1]);
							if (left % 60 || left == 0)
							{
								if (!idleTime.empty())
									idleTime = " " + idleTime;
								idleTime = aMessageStrings.seconds_string() + idleTime;
								idleTime = " " + idleTime;
								idleTime = neolib::unsigned_integer_to_string<char>(left % 60) + idleTime;
							}
							left /= 60;
							if (left % 60)
							{
								if (!idleTime.empty())
									idleTime = " " + idleTime;
								idleTime = aMessageStrings.minutes_string() + idleTime;
								idleTime = " " + idleTime;
								idleTime = neolib::unsigned_integer_to_string<char>(left % 60) + idleTime;
							}
							left /= 60;
							if (left % 24)
							{
								if (!idleTime.empty())
									idleTime = " " + idleTime;
								idleTime = aMessageStrings.hours_string() + idleTime;
								idleTime = " " + idleTime;
								idleTime = neolib::unsigned_integer_to_string<char>(left % 24) + idleTime;
							}
							left /= 24;
							if (left)
							{
								if (!idleTime.empty())
									idleTime = " " + idleTime;
								idleTime = aMessageStrings.days_string() + idleTime;
								idleTime = " " + idleTime;
								idleTime = neolib::unsigned_integer_to_string<char>(left) + idleTime;
							}
						}
						if (iParameters.size() == 2)
						{
							ret = std::string("%N% idle: %I%");
							theCodes["%N%"].first = iParameters[0];
							theCodes["%I%"].first = idleTime;
						}
						else if (iParameters.size() > 2)
						{
							ret = std::string("%N% idle: %I%, signon time: %S%");
							theCodes["%N%"].first = iParameters[0];
							theCodes["%I%"].first = idleTime;
							time_t ttTime = neolib::string_to_integer(iParameters[2]);
							tm* tmTime = localtime(&ttTime);
							std::tr1::array<char, 256> strTime;
							setlocale(LC_TIME, "");
							strftime(&strTime[0], strTime.size(), "%a %b %#d %H:%M:%S %Y", tmTime);
							theCodes["%S%"].first = std::string(&strTime[0]);
						}
						else
						{
							trailing = iParameters.begin();
						}
						break;
					}
				case message::RPL_AWAY:
					if (iParameters.size() >= 1)
					{
							ret = std::string("%N% is away");
							theCodes["%N%"].first = iParameters[0];
							if (!content().empty())
							{
								ret += std::string(", reason: %R%");
								theCodes["%R%"].first = theContent;
							}
					}
					else
						trailing = iParameters.begin();
					break;
				case message::RPL_WELCOME:
				case message::RPL_MOTDSTART:
				case message::RPL_MOTD:
				case message::RPL_ENDOFMOTD:
					replyMessage = false;
					// fall through...
				case message::RPL_WHOISOPERATOR:
				case message::RPL_WHOISEXTRA:
				case message::RPL_WHOISREGNICK:
				case message::RPL_WHOISADMIN:
				case message::RPL_WHOISSADMIN:
				case message::RPL_WHOISSVCMSG:
				case message::RPL_ENDOFWHOIS:
				case message::ERR_NOSUCHNICK:
				default:
					trailing = iParameters.begin();
					break;
				}
			}
			else
			{
				trailing = iParameters.begin();
			}
		}

		parse_codes(ret, theCodes, aStringSpans);
		theCodes.clear();	

		for (; trailing != iParameters.end(); ++trailing)
		{
			const std::string& parameter = *trailing;
			if (!ret.empty() && ret[ret.size()-1] != ' ')
				ret += " ";
			ret += parameter;
		}

		if (!theContent.empty() && appendContent)
		{
			if (!ret.empty())
				ret += " ";
			ret += theContent;
		}

		if (replyMessage && (!iCommandString.empty() && iCommandString != "#"))
		{
			std::string reply = aMessageStrings.reply_message();
			std::string::size_type pos = reply.find("%M%");
			neolib::replace_string(reply, std::string("%M%"), ret);
			if (aStringSpans != 0)
			{
				for (neolib::string_spans::iterator i = aStringSpans->begin(); i != aStringSpans->end(); ++i)
				{
					i->first += pos;
					i->second += pos;
				}
			}
			ret = reply;
		}

		if (aMessageStrings.mode() == message_strings::Column && ret.find('\t') == std::string::npos)
		{
			ret = '\t' + ret;
			if (aStringSpans != 0)
			{
				for (neolib::string_spans::iterator i = aStringSpans->begin(); i != aStringSpans->end(); ++i)
				{
					++i->first;
					++i->second;
				}
			}
		}

		if (aAppendCRLF && (ret.empty() || ret[ret.size() - 1] != '\n'))
			ret += "\r\n";

		if (aMessageStrings.display_timestamps() && aAddTimeStamp)
		{
			std::string format = aMessageStrings.timestamp_format();
			if (aMessageStrings.mode() == message_strings::Column)
			{
				neolib::remove_leading(format, std::string(" "));
				neolib::remove_trailing(format, std::string(" "));
				format += '\t';
			}
			else
			{
				if (!format.empty() && format[0] != ' ' && format[format.size() - 1] != ' ')
					format += ' ';
			}
			format = timestamp(iTime, format);
			ret = format + ret;
			if (aStringSpans != 0)
			{
				for (neolib::string_spans::iterator i = aStringSpans->begin(); i != aStringSpans->end(); ++i)
				{
					i->first += format.size();
					i->second += format.size();
				}
			}
		}
		return ret;
	}

	bool message::is_ctcp() const
	{
		bool isCtcp = content().size() > 2 && content()[0] == '\001' && content()[1] != ' ' && content()[content().size()-1] == '\001';
		return isCtcp && neolib::to_upper(content()).find("\001ACTION ") != 0;
	}

	const user& message::user(const buffer& aBuffer, const irc::user& aOrigin) const
	{
		return iDirection == INCOMING ? 
			aBuffer.has_user(aOrigin) ? aBuffer.user(aOrigin) : aOrigin : 
			aBuffer.has_user(aBuffer.connection().nick_name()) ? aBuffer.user(aBuffer.connection().nick_name()) : aBuffer.connection().user();
	}

	bool message::operator==(const message& aRhs) const
	{
		return iId == aRhs.iId;
	}

	message::command_e message::string_to_command(const std::string& aCommand)
	{
		typedef std::map<neolib::ci_string, message::command_e> string_commands;
		static string_commands sStringCommands;
		typedef std::map<unsigned int, message::command_e> numeric_replies;
		static numeric_replies sNumericReplies;

		if (sStringCommands.empty())
		{
			for (int i = 0; i < sizeof(sStringCommandList) / sizeof(sStringCommandList[0]); ++i)
				sStringCommands[sStringCommandList[i].iString] = sStringCommandList[i].iCommand;
			for (int i = 0; i < sizeof(sNumericReplyList) / sizeof(sNumericReplyList[0]); ++i)
				sNumericReplies[sNumericReplyList[i].iNumber] = sNumericReplyList[i].iCommand;
		}

		long n = neolib::string_to_integer(aCommand);

		if (n == 0)
		{
			string_commands::const_iterator it = sStringCommands.find(neolib::make_ci_string(aCommand));
			return it != sStringCommands.end() ? it->second : UNKNOWN;
		}
		else
		{
			numeric_replies::const_iterator it = sNumericReplies.find(n);
			return it != sNumericReplies.end() ? it->second : RPL_UNKNOWN;
		}
	}

	const std::string& message::command_to_string(command_e aCommand)
	{
		typedef std::map<command_e, std::string> command_strings;
		static command_strings sCommandStrings;

		if (sCommandStrings.empty())
		{
			for (int i = 0; i < sizeof(sStringCommandList) / sizeof(sStringCommandList[0]); ++i)
				sCommandStrings[sStringCommandList[i].iCommand] = sStringCommandList[i].iString;
			for (int i = 0; i < sizeof(sNumericReplyList) / sizeof(sNumericReplyList[0]); ++i)
				sCommandStrings[sNumericReplyList[i].iCommand] = neolib::integer_to_string<char>(sNumericReplyList[i].iNumber, 10, 3);
		}

		static const std::string unknownCommand;
		return aCommand != UNKNOWN && aCommand != RPL_UNKNOWN ? sCommandStrings[aCommand] : unknownCommand;
	}

	void message::set_command(command_e aCommand)
	{
		iCommand = aCommand;
		iCommandString = command_to_string(aCommand);
	}

	void message::set_command(const std::string& aCommand)
	{
		iCommand = string_to_command(aCommand);
		iCommandString = aCommand;
	}

	bool message::is_channel(const buffer* aBuffer, const std::string& aName)
	{
		if (aName.empty())
			return false;

		if (aBuffer != 0)
			return aBuffer->connection().is_channel(aName);

		switch(aName[0])
		{
		case '&':
		case '#':
		case '+':
		case '!':
			return true;
		default:
			return false;
		}
	}

	const std::string& message::chantypes(const buffer* aBuffer)
	{
		static std::string defaultChantypes("&#+!");
		if (aBuffer != 0)
			return aBuffer->connection().chantypes();
		else
			return defaultChantypes;
	}
}