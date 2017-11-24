// message_strings.h
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

#ifndef IRC_CLIENT_MESSAGE_STRINGS
#define IRC_CLIENT_MESSAGE_STRINGS

namespace irc
{
	class message_strings
	{
		// types
	public:
		enum mode_e 
		{ 
			Normal = 0, 
			Column = 1,
			ModeCount
		};

		// construction
		message_strings() : iMode(Normal) {}

		// operations
	public:
		mode_e mode() const { return iMode; }
		const std::string& reply_message() const { return iReplyMessage[iMode]; }
		const std::string& topic_message() const { return iTopicMessage[iMode]; }
		const std::string& topic_change_message() const { return iTopicChangeMessage[iMode]; }
		const std::string& topic_author_message() const { return iTopicAuthorMessage[iMode]; }
		const std::string& join_message() const { return iJoinMessage[iMode]; }
		const std::string& join_self_message() const { return iJoinSelfMessage[iMode]; }
		const std::string& part_message() const { return iPartMessage[iMode]; }
		const std::string& quit_message() const { return iQuitMessage[iMode]; }
		const std::string& kick_message() const { return iKickMessage[iMode]; }
		const std::string& kick_self_message() const { return iKickSelfMessage[iMode]; }
		const std::string& nick_message() const { return iNickMessage[iMode]; }
		const std::string& nick_self_message() const { return iNickSelfMessage[iMode]; }
		const std::string& standard_message() const { return iStandardMessage[iMode]; }
		const std::string& action_message() const { return iActionMessage[iMode]; }
		const std::string& notice_message() const { return iNoticeMessage[iMode]; }
		const std::string& sent_notice_message() const { return iSentNoticeMessage[iMode]; }
		const std::string& notice_origin_message() const { return iNoticeOriginMessage[iMode]; }
		const std::string& mode_message() const { return iModeMessage[iMode]; }
		const std::string& ctcp_message() const { return iCtcpMessage[iMode]; }
		bool display_timestamps() const { return iDisplayTimestamps; }
		const std::string& timestamp_format() const { return iTimestampFormat; }
		const std::string& own_quit_message() const { return iOwnQuitMessage; }
		const std::string& seconds_string() const { return iSecondsString; }
		const std::string& minutes_string() const { return iMinutesString; }
		const std::string& hours_string() const { return iHoursString; }
		const std::string& days_string() const { return iDaysString; }

		void set_mode(mode_e aMode) { iMode = aMode; }
		void set_reply_message(mode_e aMode, const std::string& aNewValue) { iReplyMessage[aMode] = aNewValue; }
		void set_topic_message(mode_e aMode, const std::string& aNewValue) { iTopicMessage[aMode] = aNewValue; }
		void set_topic_change_message(mode_e aMode, const std::string& aNewValue) { iTopicChangeMessage[aMode] = aNewValue; }
		void set_topic_author_message(mode_e aMode, const std::string& aNewValue) { iTopicAuthorMessage[aMode] = aNewValue; }
		void set_join_message(mode_e aMode, const std::string& aNewValue) { iJoinMessage[aMode] = aNewValue; }
		void set_join_self_message(mode_e aMode, const std::string& aNewValue) { iJoinSelfMessage[aMode] = aNewValue; }
		void set_part_message(mode_e aMode, const std::string& aNewValue) { iPartMessage[aMode] = aNewValue; }
		void set_quit_message(mode_e aMode, const std::string& aNewValue) { iQuitMessage[aMode] = aNewValue; }
		void set_kick_message(mode_e aMode, const std::string& aNewValue) { iKickMessage[aMode] = aNewValue; }
		void set_kick_self_message(mode_e aMode, const std::string& aNewValue) { iKickSelfMessage[aMode] = aNewValue; }
		void set_nick_message(mode_e aMode, const std::string& aNewValue) { iNickMessage[aMode] = aNewValue; }
		void set_nick_self_message(mode_e aMode, const std::string& aNewValue) { iNickSelfMessage[aMode] = aNewValue; }
		void set_standard_message(mode_e aMode, const std::string& aNewValue) { iStandardMessage[aMode] = aNewValue; }
		void set_action_message(mode_e aMode, const std::string& aNewValue) { iActionMessage[aMode] = aNewValue; }
		void set_notice_message(mode_e aMode, const std::string& aNewValue) { iNoticeMessage[aMode] = aNewValue; }
		void set_sent_notice_message(mode_e aMode, const std::string& aNewValue) { iSentNoticeMessage[aMode] = aNewValue; }
		void set_notice_origin_message(mode_e aMode, const std::string& aNewValue) { iNoticeOriginMessage[aMode] = aNewValue; }
		void set_mode_message(mode_e aMode, const std::string& aNewValue) { iModeMessage[aMode] = aNewValue; }
		void set_ctcp_message(mode_e aMode, const std::string& aNewValue) { iCtcpMessage[aMode] = aNewValue; }
		void set_display_timestamps(bool aNewValue) { iDisplayTimestamps = aNewValue; }
		void set_timestamp_format(const std::string& aNewValue) { iTimestampFormat = aNewValue; }
		void set_own_quit_message(const std::string& aNewValue) { iOwnQuitMessage = aNewValue; }
		void set_seconds_string(const std::string& aNewValue) { iSecondsString = aNewValue; }
		void set_minutes_string(const std::string& aNewValue) { iMinutesString = aNewValue; }
		void set_hours_string(const std::string& aNewValue) { iHoursString = aNewValue; }
		void set_days_string(const std::string& aNewValue) { iDaysString = aNewValue; }

		// attibutes
	private:
		mode_e iMode;
		std::string iReplyMessage[ModeCount];
		std::string iTopicMessage[ModeCount];
		std::string iTopicChangeMessage[ModeCount];
		std::string iTopicAuthorMessage[ModeCount];
		std::string iJoinMessage[ModeCount];
		std::string iJoinSelfMessage[ModeCount];
		std::string iPartMessage[ModeCount];
		std::string iQuitMessage[ModeCount];
		std::string iKickMessage[ModeCount];
		std::string iKickSelfMessage[ModeCount];
		std::string iNickMessage[ModeCount];
		std::string iNickSelfMessage[ModeCount];
		std::string iStandardMessage[ModeCount];
		std::string iActionMessage[ModeCount];
		std::string iNoticeMessage[ModeCount];
		std::string iSentNoticeMessage[ModeCount];
		std::string iNoticeOriginMessage[ModeCount];
		std::string iModeMessage[ModeCount];
		std::string iCtcpMessage[ModeCount];
		bool iDisplayTimestamps;
		std::string iTimestampFormat;
		std::string iOwnQuitMessage;
		std::string iSecondsString;
		std::string iMinutesString;
		std::string iHoursString;
		std::string iDaysString;

		// temporary mode changer
	public:
		struct temp_mode_changer
		{
			temp_mode_changer(message_strings& aParent, mode_e aNewMode) : iParent(aParent), iOldMode(aParent.iMode)
			{
				iParent.set_mode(aNewMode);
			}
			~temp_mode_changer()
			{
				iParent.set_mode(iOldMode);
			}
			message_strings& iParent;
			mode_e iOldMode;
		};
	};
}

#endif //IRC_CLIENT_MESSAGE_STRINGS
