// dcc_message.h
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

#ifndef IRC_CLIENT_DCC_MESSAGE
#define IRC_CLIENT_DCC_MESSAGE

namespace irc
{
	class dcc_chat_connection;
	class message_strings;

	class dcc_message
	{
	public:
		// types
		enum type_e
		{
			NORMAL, STATUS
		};
		enum direction_e
		{
			INCOMING, OUTGOING
		};

	public:
		// construction
		dcc_message(dcc_chat_connection& aConnection, direction_e aDirection, type_e aType = NORMAL, bool aFromLog = false);

	public:
		// operations
		model::id id() const { return iId; }
		void set_id(model::id aId) { iId = aId; }
		bool from_log() const { return iFromLog; }
		time_t time() const { return iTime; }
		bool parse_log(const std::string& aLogEntry);
		std::string to_string() const;
		std::string to_nice_string(const message_strings& aMessageStrings, const dcc_chat_connection& aConnection, const std::string& aAppendToContent = "", const void* aParameter = 0, bool aAddTimeStamp = true, bool aAppendCRLF = true) const;
		direction_e direction() const { return iDirection; }
		type_e type() const {return iType; }
		const std::string& content() const { return iContent; }
		direction_e& direction() { return iDirection; }
		type_e& type() {return iType; }
		std::string& content() { return iContent; }

	private:
		// attibutes
		model::id iId;
		bool iFromLog;
		time_t iTime;
		direction_e iDirection;
		type_e iType;
		std::string iContent;
	};
}

#endif //IRC_CLIENT_DCC_MESSAGE
