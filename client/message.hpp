// message.h
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

#ifndef IRC_CLIENT_MESSAGE
#define IRC_CLIENT_MESSAGE

#include <neolib/neolib.hpp>
#include <vector>
#include <neolib/string_utils.hpp>
#include "model.hpp"

namespace irc
{
	class connection_manager;
	class connection;
	class buffer;
	class user;

	class message_strings;

	class message
	{
		// types
	public:
		enum direction_e
		{
			INCOMING, OUTGOING
		};
		enum command_e
		{
			UNKNOWN = 0,
			PRIVMSG, 
			NOTICE,
			NICK,
			USER,
			PASS,
			QUIT, 
			JOIN, 
			PART,
			TOPIC,
			PING,
			PONG,
			MODE,
			WHOIS,
			WHO,
			KICK,
			LIST,
			INVITE,
			AWAY,
			RPL_UNKNOWN = 1000,
			RPL_WELCOME, RPL_YOURHOST, RPL_CREATED, RPL_MYINFO, RPL_BOUNCE, RPL_ISUPPORT = RPL_BOUNCE,
			RPL_USERHOST, RPL_ISON, RPL_AWAY, RPL_UNAWAY, RPL_NOWAWAY, RPL_WHOISUSER,
			RPL_WHOISSERVER, RPL_WHOISOPERATOR, RPL_WHOISIDLE, RPL_WHOISREGNICK, RPL_WHOISADMIN, 
			RPL_WHOISSADMIN, RPL_WHOISSVCMSG, RPL_ENDOFWHOIS,
			RPL_WHOISCHANNELS, RPL_WHOWASUSER, RPL_ENDOFWHOWAS, RPL_LISTSTART,
			RPL_LIST, RPL_LISTEND, RPL_UNIQOPIS, RPL_CHANNELMODEIS, RPL_CREATIONTIME, RPL_NOTOPIC,
			RPL_TOPIC, RPL_INVITING, RPL_SUMMONING, RPL_INVITELIST, RPL_ENDOFINVITELIST,
			RPL_EXCEPTLIST, RPL_ENDOFEXCEPTLIST, RPL_VERSION, RPL_WHOREPLY, RPL_ENDOFWHO,
			RPL_NAMREPLY, RPL_ENDOFNAMES, RPL_LINKS, RPL_ENDOFLINKS, RPL_BANLIST,
			RPL_ENDOFBANLIST, RPL_INFO, RPL_ENDOFINFO, RPL_MOTDSTART, RPL_MOTD,
			RPL_ENDOFMOTD, RPL_YOUREOPER, RPL_REHASHING, RPL_YOURESERVICE, RPL_TIME,
			RPL_USERSSTART, RPL_USERS, RPL_ENDOFUSERS, RPL_NOUSERS, RPL_TRACELINK,
			RPL_TRACECONNECTING, RPL_TRACEHANDSHAKE, RPL_TRACEUNKNOWN, RPL_TRACEOPERATOR,
			RPL_TRACEUSER, RPL_TRACESERVER, RPL_TRACESERVICE, RPL_TRACENEWTYPE,
			RPL_TRACECLASS, RPL_TRACERECONNECT, RPL_TRACELOG, RPL_TRACEEND,
			RPL_STATSLINKINFO, RPL_STATSCOMMANDS, RPL_ENDOFSTATS, RPL_STATSUPTIME,
			RPL_STATSOLINE, RPL_UMODEIS, RPL_SERVLIST, RPL_SERVLISTEND, RPL_LUSERCLIENT,
			RPL_LUSEROP, RPL_LUSERUNKNOWN, RPL_LUSERCHANNELS, RPL_LUSERME, RPL_ADMINME,
			RPL_ADMINLOC1, RPL_ADMINLOC2, RPL_ADMINEMAIL, RPL_TRYAGAIN, ERR_NOSUCHNICK,
			ERR_NOSUCHSERVER, ERR_NOSUCHCHANNEL, ERR_CANNOTSENDTOCHAN,
			ERR_TOOMANYCHANNELS, ERR_WASNOSUCHNICK, ERR_TOOMANYTARGETS,
			ERR_NOSUCHSERVICE, ERR_NOORIGIN, ERR_NORECIPIENT, ERR_NOTEXTTOSEND,
			ERR_NOTOPLEVEL, ERR_WILDTOPLEVEL, ERR_BADMASK, ERR_UNKNOWNCOMMAND,
			ERR_NOMOTD, ERR_NOADMININFO, ERR_FILEERROR, ERR_NONICKNAMEGIVEN,
			ERR_ERRONEUSNICKNAME, ERR_NICKNAMEINUSE, ERR_NICKCOLLISION, ERR_UNAVAILRESOURCE,
			ERR_USERNOTINCHANNEL, ERR_NOTONCHANNEL, ERR_USERONCHANNEL,
			ERR_NOLOGIN, ERR_SUMMONDISABLED, ERR_USERSDISABLED, ERR_NOTREGISTERED,
			ERR_NEEDMOREPARAMS, ERR_ALREADYREGISTRED, ERR_NOPERMFORHOST, ERR_PASSWDMISMATCH,
			ERR_YOUREBANNEDCREEP, ERR_YOUWILLBEBANNED, ERR_KEYSET, ERR_CHANNELISFULL,
			ERR_UNKNOWNMODE, ERR_INVITEONLYCHAN, ERR_BANNEDFROMCHAN, ERR_BADCHANNELKEY,
			ERR_BADCHANMASK, ERR_NOCHANMODES, ERR_BANLISTFULL, ERR_NOPRIVILEGES,
			ERR_CHANOPRIVSNEEDED, ERR_CANTKILLSERVER, ERR_RESTRICTED, ERR_UNIQOPPRIVSNEEDED,
			ERR_NOOPERHOST, ERR_UMODEUNKNOWNFLAG, ERR_USERSDONTMATCH,
			RPL_SERVICEINFO, RPL_ENDOFSERVICES, RPL_SERVICE, RPL_NONE, RPL_WHOISCHANOP,
			RPL_KILLDONE, RPL_CLOSING, RPL_CLOSEEND, RPL_INFOSTART, RPL_MYPORTIS,
			RPL_STATSCLINE, RPL_STATSNLINE, RPL_STATSILINE, RPL_STATSKLINE, RPL_STATSQLINE,
			RPL_STATSYLINE, RPL_STATSVLINE, RPL_STATSLLINE, RPL_STATSHLINE, RPL_STATSSLINE,
			RPL_STATSPING, RPL_STATSBLINE, RPL_STATSDLINE, ERR_NOSERVICEHOST,
			// NON-STANDARD-STANDARD
			RPL_TOPICAUTHOR,
			RPL_WHOISEXTRA,
		};
		enum 
		{
			MaxMessageSize = 512
		};
		typedef std::vector<std::string> parameters_t;
		static const std::size_t no_content = static_cast<std::size_t>(-1);
		enum string_span_type_e
		{
			NickName = 1, 
			Channel
		};

		// construction
	public:
		message(connection_manager& aConnectionManager, direction_e aDirection, bool aFromLog = false);
		message(connection& aConnection, direction_e aDirection, bool aFromLog = false);
		message(buffer& aBuffer, direction_e aDirection, bool aFromLog = false);

		// operations
	public:
		static std::size_t max_size() { return MaxMessageSize; }
		model::id id() const { return iId; }
		void set_id(model::id aId) { iId = aId; }
		bool from_log() const { return iFromLog; }
		bool buffer_required() const { return iBufferRequired; }
		void set_buffer_required(bool aBufferRequired) { iBufferRequired = aBufferRequired; }
		time_t time() const { return iTime; }
		void parse_command(const std::string& aMessage);
		void parse_parameters(const std::string& aMessage, bool aHasTarget = false, bool aFromServer = false);
		bool parse_log(const std::string& aLogEntry);
		const std::string& content() const;
		std::size_t content_param() const;
		std::string to_string(const message_strings& aMessageStrings, bool aAddPrefix = false, bool aAddTarget = false) const;
		std::string to_nice_string(const message_strings& aMessageStrings, const buffer* aBuffer = 0, const std::string& aAppendToContent = "", bool aIsSelf = false, bool aAddTimeStamp = true, bool aAppendCRLF = true, neolib::string_spans* aStringSpans = 0) const;
		bool is_ctcp() const;
		const direction_e direction() const { return iDirection; }
		const std::string& origin() const { return iOrigin; }
		const irc::user& user(const buffer& aBuffer, const user& aOrigin) const;
		command_e command() const { return iCommand; }
		const std::string& command_string() const { return iCommandString; }
		bool is_numeric_reply() const { return 	neolib::string_to_integer(iCommandString) != 0; }
		const std::string& target() const { return iTarget; }
		const parameters_t&  parameters() const { return iParameters; }
		void set_direction(direction_e aDirection) { iDirection = aDirection; }
		void set_origin(const std::string& aOrigin) { iOrigin = aOrigin; }
		void set_command(command_e aCommand);
		void set_command(const std::string& aCommand);
		void set_target(const std::string& aTarget) { iTarget = aTarget; }
		parameters_t& parameters() { return iParameters; }
		static bool is_channel(const std::string& aName) { return is_channel(0, aName); }
		bool operator==(const message& aRhs) const;

		// implementation
	private:
		static command_e string_to_command(const std::string& aCommand);
		static const std::string& command_to_string(command_e aCommand);
		static const std::string& chantypes(const buffer* aBuffer);
		static bool is_channel(const buffer* aBuffer, const std::string& aName);

		// attibutes
	private:
		model::id iId;
		bool iFromLog;
		bool iBufferRequired;
		time_t iTime;
		direction_e iDirection;
		std::string iOrigin;
		command_e iCommand;
		std::string iCommandString;
		std::string iTarget;
		parameters_t iParameters;
	};
}

#endif //IRC_CLIENT_MESSAGE
