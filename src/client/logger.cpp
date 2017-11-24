// logger.cpp
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
#include <fstream>
#include <ctime>
#include <clocale>
#include <neolib/file.hpp>
#include <neoirc/client/logger.hpp>
#include <neoirc/client/model.hpp>

namespace irc
{
	namespace
	{
		enum { LoggerBufferSize = 4096 };
	}

	bool logger::scrollbacker::is(irc::buffer& aBuffer) const
	{
		if (iBuffer.is<irc::buffer*>() && static_cast<irc::buffer*>(iBuffer) == &aBuffer)
			return true;
		else
			return false;
	}

	bool logger::scrollbacker::is(dcc_buffer& aBuffer) const
	{
		if (iBuffer.is<dcc_buffer*>() && static_cast<dcc_buffer*>(iBuffer) == &aBuffer)
			return true;
		else
			return false;
	}

	void logger::scrollbacker::task()
	{
		std::string filename;
		std::string sep = "\r\n";
		bool isIRC = true;
		if (iBuffer.is<irc::buffer*>())
		{
			filename = iParent.filename(*static_cast<irc::buffer*>(iBuffer), logger::Scrollback);
			iMessages = buffer_messages();
		}
		else
		{
			filename = iParent.filename(*static_cast<dcc_buffer*>(iBuffer), logger::Scrollback);
			iMessages = dcc_messages();
			isIRC = false;
		}

		std::ifstream scrollbackerFile(filename.c_str(), std::ios::in|std::ios::binary);
		if (scrollbackerFile)
		{
			scrollbackerFile.seekg(0, std::ios_base::end);
			std::ifstream::pos_type pos = scrollbackerFile.tellg();
			bool chopFile = (static_cast<std::size_t>(pos) > iParent.iScrollbackSize * 1024);
			std::string text;
			bool finished = false;
			while(pos > 0 && running() && !finished)
			{
				std::ifstream::off_type offset = (pos < LoggerBufferSize ? pos : LoggerBufferSize);
				scrollbackerFile.seekg(-offset, std::ios_base::cur);
				char more[LoggerBufferSize];
				scrollbackerFile.read(more, offset);
				scrollbackerFile.seekg(-offset, std::ios_base::cur);
				pos = scrollbackerFile.tellg();
				text = std::string(more, static_cast<std::size_t>(offset)) + text;
				typedef std::pair<std::string::iterator, std::string::iterator> line_t;
				typedef std::vector<line_t> lines_t;
				lines_t lines;
				neolib::tokens(text.begin(), text.end(), sep.begin(), sep.end(), lines, 0, true, true);
				for (lines_t::reverse_iterator i = lines.rbegin(); i != lines.rend() && !finished;)
				{
					line_t line = *i++;
					if (i != lines.rend() || pos == std::ifstream::pos_type(0))
					{
						if (line.first != line.second)
						{
							if (isIRC)
							{
								message theMessage(*static_cast<irc::buffer*>(iBuffer), message::INCOMING, true);
								if (theMessage.parse_log(std::string(line.first, line.second)))
									static_cast<buffer_messages&>(iMessages).push_front(theMessage);
								if (static_cast<buffer_messages&>(iMessages).size() == iBufferSize)
									finished = true;
							}
							else
							{
								dcc_message theMessage(*static_cast<dcc_buffer*>(iBuffer), dcc_message::INCOMING, dcc_message::NORMAL, true);
								if (theMessage.parse_log(std::string(line.first, line.second)))
									static_cast<dcc_messages&>(iMessages).push_front(theMessage);
								if (static_cast<dcc_messages&>(iMessages).size() == iBufferSize)
									finished = true;
							}
						}
					}
					else
						text = std::string(line.first, line.second) + sep;
				}
			}
			if (chopFile)
			{
				scrollbackerFile.seekg(-(static_cast<std::ifstream::off_type>(iParent.iScrollbackSize) * 1024 / 2), std::ios_base::end);
				std::ofstream choppedScrollbackerFile((filename + ".tmp").c_str(), std::ios::out|std::ios::app|std::ios::binary);
				char buffer[LoggerBufferSize];
				long count = iParent.iScrollbackSize * 1024 / 2;
				bool hadNewline = false;
				while(count > 0 && running())
				{
					long readLength = count > LoggerBufferSize ? LoggerBufferSize : count;
					scrollbackerFile.read(buffer, readLength);
					if (!hadNewline)
					{
						char* newLine = std::find(buffer, buffer+readLength, '\n');
						if (newLine != buffer+readLength)
						{
							hadNewline = true;
							if (newLine != buffer+readLength-1)
								choppedScrollbackerFile.write(newLine+1, (buffer+readLength) - (newLine+1));
						}
					}
					else
					{
						choppedScrollbackerFile.write(buffer, readLength);
					}
					count -= LoggerBufferSize;
				}
				if (running())
				{
					scrollbackerFile.close();
					choppedScrollbackerFile.close();
					remove(filename.c_str());
					rename((filename + ".tmp").c_str(), filename.c_str());
				}
			}
		}
		if (!running())
			remove((filename + ".tmp").c_str());
	}

	logger::logger(model& aModel, connection_manager& aConnectionManager, dcc_connection_manager& aDccConnectionManager) :
		iModel{ aModel }, iConnectionManager{ aConnectionManager }, iDccConnectionManager{ aDccConnectionManager },
		iEnabled{ false }, iEvents{ Message }, iServerLog{ false },
		iScrollbackLogs{ false }, iScrollbackSize{ 1000 }, iArchive{ false }, iArchiveSize{ 1000 },
		iUpdateTimer{ aModel.io_task(), [this](neolib::callback_timer&) { process_pending(); iUpdateTimer.again(); }, 10 }
	{
		iConnectionManager.add_observer(*this);
		iDccConnectionManager.add_observer(*this);
	}

	logger::~logger()
	{
		iScrollbackers.clear();
		for (logged_connections::iterator i = iLoggedConnections.begin(); i != iLoggedConnections.end(); ++i)
			(*i)->remove_observer(*this);
		for (logged_buffers::iterator i = iLoggedBuffers.begin(); i != iLoggedBuffers.end(); ++i)
			(*i)->remove_observer(*this);
		for (logged_dcc_buffers::iterator i = iLoggedDccChatConnections.begin(); i != iLoggedDccChatConnections.end(); ++i)
			(*i)->neolib::observable<dcc_buffer_observer>::remove_observer(*this);
		iConnectionManager.remove_observer(*this);
		iDccConnectionManager.remove_observer(*this);
	}

	bool logger::enable(bool aEnable)
	{
		if (iEnabled == aEnable)
			return false;
		iEnabled = aEnable;
		if (iEnabled)
		{
			create_directories();
			timestamp_all();
		}
		return true;
	}

	void logger::timestamp_all()
	{
		for (logged_buffers::iterator i = iLoggedBuffers.begin(); i != iLoggedBuffers.end(); ++i)
		{
			std::string timestamp;
			get_timestamp(timestamp, true);
			new_entry(**i, timestamp);
		}
	}

	void logger::timestamp_servers()
	{
		for (logged_buffers::iterator i = iLoggedBuffers.begin(); i != iLoggedBuffers.end(); ++i)
		{
			if ((**i).type() != buffer::SERVER)
				continue;
			std::string timestamp;
			get_timestamp(timestamp, true);
			new_entry(**i, timestamp);
		}
	}

	bool logger::set_directory(const std::string& aDirectory)
	{ 
		if (iDirectory == aDirectory)
			return false;
		iDirectory = aDirectory;
		return true;
	}
	 
	std::string logger::directory(buffer::type_e aBufferType) const
	{
		std::string ret;
		ret = iDirectory;
		ret = ret + model::sPathSeparator + (aBufferType == buffer::SERVER ? "servers" : aBufferType == buffer::CHANNEL ? "channels" : aBufferType == buffer::USER ? "users" : "notices") + model::sPathSeparator;
		return ret;
	}

	std::string logger::directory(dcc_connection::type_e aDccConnectionType) const
	{
		std::string ret;
		ret = iDirectory;
		ret = ret + model::sPathSeparator + (aDccConnectionType == dcc_connection::CHAT ? "chat" : "dcc") + model::sPathSeparator;
		return ret;
	}

	void logger::create_directories() const
	{
		if (!iEnabled)
			return;
		neolib::create_path(directory(buffer::CHANNEL));
		neolib::create_path(directory(buffer::USER));
		if (iServerLog)
			neolib::create_path(directory(buffer::SERVER));
		if (iModel.connection_manager().use_notice_buffer())
			neolib::create_path(directory(buffer::NOTICE));
		neolib::create_path(directory(dcc_connection::CHAT));
		if (iArchive)
		{
			neolib::create_path(directory(buffer::CHANNEL) + "archive");
			neolib::create_path(directory(buffer::USER) + "archive");
			if (iServerLog)
				neolib::create_path(directory(buffer::SERVER) + "archive");
			if (iModel.connection_manager().use_notice_buffer())
				neolib::create_path(directory(buffer::NOTICE) + "archive");
			neolib::create_path(directory(dcc_connection::CHAT) + "archive");
		}
	}

	void logger::process_pending()
	{
		for (scrollbackers::iterator i = iScrollbackers.begin(); i != iScrollbackers.end();)
		{
			scrollbacker& theScrollbacker = **i;
			if (!theScrollbacker.running())
			{
				if (theScrollbacker.buffer().is<buffer*>())
				{
					buffer& theBuffer = static_cast<buffer&>(*static_cast<buffer*>(theScrollbacker.buffer()));
					scrollbacker::messages_t& theMessages = theScrollbacker.messages();
					theBuffer.scrollback(static_cast<scrollbacker::buffer_messages&>(theMessages));
					scrollbacker::messages_t& theNewMessages = theScrollbacker.new_messages();
					for (scrollbacker::buffer_messages::iterator j = static_cast<scrollbacker::buffer_messages&>(theNewMessages).begin(); 
						j != static_cast<scrollbacker::buffer_messages&>(theNewMessages).end(); ++j)
					{
						message& theMessage = *j;
						std::string line = neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(theMessage.time()));
						line += (theMessage.direction() == message::INCOMING ? " < " : " > ");
						line += theMessage.to_string(iModel.message_strings(), true, true);
						new_entry(filename(theBuffer, Scrollback), line, Scrollback);
					}
				}
				else
				{
					dcc_buffer& theBuffer = static_cast<dcc_buffer&>(*static_cast<dcc_buffer*>(theScrollbacker.buffer()));
					scrollbacker::messages_t& theMessages = theScrollbacker.messages();
					theBuffer.scrollback(static_cast<scrollbacker::dcc_messages&>(theMessages));
					scrollbacker::messages_t& theNewMessages = theScrollbacker.new_messages();
					for (scrollbacker::dcc_messages::iterator j = static_cast<scrollbacker::dcc_messages&>(theNewMessages).begin(); 
						j != static_cast<scrollbacker::dcc_messages&>(theNewMessages).end(); ++j)
					{
						dcc_message& theMessage = *j;
						std::string line = neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(theMessage.time()));
						line += (theMessage.direction() == dcc_message::INCOMING ? " < " : " > ");
						line += theMessage.to_string();
						new_entry(filename(theBuffer, Scrollback), line, Scrollback);
					}
				}
				i = iScrollbackers.erase(i);
			}
			else
				++i;
		}
	}

	void logger::get_timestamp(std::string& aTimeStamp, bool aContinuation)
	{
		std::time_t ttTime = std::time(0);
		std::tm* tmTime = std::localtime(&ttTime);
		std::setlocale(LC_TIME, "");
		std::vector<char> strTime;
		strTime.resize(LoggerBufferSize);
		if (!aContinuation)
			std::strftime(&*strTime.begin(), LoggerBufferSize, "-- %a %b %#d %H:%M:%S %Y --\r\n", tmTime);
		else
			std::strftime(&*strTime.begin(), LoggerBufferSize, "++ %a %b %#d %H:%M:%S %Y ++\r\n", tmTime);
		aTimeStamp = std::string(&*strTime.begin());
	}

	std::string logger::filename(const buffer& aBuffer, filename_type_e aType)
	{
		std::string fileName;
		if (aBuffer.type() != buffer::SERVER)
			fileName = aBuffer.name();
		else
			fileName = "Server";
		fileName = fileName + " (" + aBuffer.connection().server().network()/* + " - " + aBuffer.connection().server().name()*/ + (aType == Scrollback ? ").irc" : ").txt");
		for (std::string::iterator i = fileName.begin(); i != fileName.end(); ++i)
		{
			switch(*i)
			{
			case ':':
			case '\\':
			case '/':
			case '<':
			case '>':
			case '*':
			case '?':
			case '|':
				*i = ' ';
				break;
			default:
				break;
			}
		}
		return neolib::create_file(directory(aBuffer.type()) + fileName);
	}

	void logger::new_entry(buffer& aBuffer, const std::string& aText)
	{
		if (aBuffer.type() == buffer::SERVER && !iServerLog)
			return;

		new_entry(filename(aBuffer), aText);
	}

	std::string logger::filename(const dcc_buffer& aBuffer, filename_type_e aType)
	{
		std::string fileName = aBuffer.name() + (aType == Scrollback ? ".irc" : ".txt");
		for (std::string::iterator i = fileName.begin(); i != fileName.end(); ++i)
		{
			switch(*i)
			{
			case ':':
			case '\\':
			case '/':
			case '<':
			case '>':
			case '*':
			case '?':
			case '|':
				*i = ' ';
				break;
			default:
				break;
			}
		}
		return neolib::create_file(directory(aBuffer.type()) + fileName);
	}

	void logger::new_entry(dcc_buffer& aBuffer, const std::string& aText)
	{
		new_entry(filename(aBuffer), aText);
	}

	void logger::new_entry(const std::string& aFileName, const std::string& aText, filename_type_e aType)
	{
		if (!iEnabled)
			return;
		if (aType == Normal)
		{
			std::ifstream logfile(aFileName.c_str(), std::ios::in|std::ios::binary);
			wchar_t BOM;
			logfile.read(reinterpret_cast<char*>(&BOM), 2);
			if (logfile && (BOM == 0xFEFF || BOM == 0xFFFE))
			{
				throw utf16_logfile_unsupported();
			}
		}
		std::ofstream::pos_type length;
		{
			std::ofstream logfile(aFileName.c_str(), std::ios::out|std::ios::app|std::ios::binary);
			logfile.write(aText.c_str(), aText.size());
			length = logfile.tellp();
		}
		if (aType == Normal && iArchive && length > static_cast<std::ofstream::pos_type>(iArchiveSize * 1024))
		{
			int n = 1;
			std::string archiveFileName = aFileName;
			std::string::size_type sep = archiveFileName.find_last_of(model::sPathSeparator);
			archiveFileName.insert(sep, std::string(1, model::sPathSeparator) + "archive");
			neolib::replace_string(archiveFileName, std::string(".txt"), std::string(" (%n%).txt"));
			for (std::size_t tryNumber = 1; tryNumber < 10000; ++tryNumber)
			{
				std::string tryFileName = archiveFileName;
				neolib::replace_string(tryFileName, std::string("%n%"), neolib::unsigned_integer_to_string<char>(tryNumber));
				if (!neolib::file_exists(tryFileName))
				{
					if (neolib::move_file(aFileName, tryFileName))
					{
						std::string timestamp;
						get_timestamp(timestamp, true);
						new_entry(aFileName, timestamp, aType);
					}
					break;
				}
			}
		}
	}

	logger::scrollbackers::iterator logger::scrollbacker_for_buffer(buffer& aBuffer)
	{
		for (scrollbackers::iterator i = iScrollbackers.begin(); i != iScrollbackers.end(); ++i)
			if ((*i)->is(aBuffer))
				return i;
		return iScrollbackers.end();
	}

	logger::scrollbackers::iterator logger::scrollbacker_for_buffer(dcc_buffer& aBuffer)
	{
		for (scrollbackers::iterator i = iScrollbackers.begin(); i != iScrollbackers.end(); ++i)
			if ((*i)->is(aBuffer))
				return i;
		return iScrollbackers.end();
	}

	void logger::connection_added(connection& aConnection)
	{
		iLoggedConnections.push_back(&aConnection);
		aConnection.add_observer(*this);
	}

	void logger::connection_removed(connection& aConnection)
	{
		iLoggedConnections.remove(&aConnection);
	}

	void logger::buffer_added(buffer& aBuffer)
	{
		iLoggedBuffers.push_back(&aBuffer);
		aBuffer.add_observer(*this);
		std::string timestamp;
		get_timestamp(timestamp);
		new_entry(aBuffer, timestamp);
		if (iScrollbackLogs && (aBuffer.type() != buffer::SERVER || iServerLog))
		{
			iScrollbackers.push_back(scrollbacker_pointer(new scrollbacker(*this, aBuffer)));
			iScrollbackers.back()->start();
		}
	}

	void logger::buffer_removed(buffer& aBuffer)
	{
		iLoggedBuffers.remove(&aBuffer);
		for (scrollbackers::iterator i = iScrollbackers.begin(); i != iScrollbackers.end(); ++i)
			if ((*i)->is(aBuffer))
			{
				iScrollbackers.erase(i);
				break;
			}
	}

	void logger::buffer_message(buffer& aBuffer, const message& aMessage)
	{
		if (aBuffer.type() == buffer::SERVER && !iServerLog)
			return;
		if (iScrollbackLogs)
		{
			scrollbackers::iterator i = scrollbacker_for_buffer(aBuffer);
			if (i == iScrollbackers.end())
			{
				std::string line = neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(aMessage.time()));
				line += (aMessage.direction() == message::INCOMING ? " < " : " > ");
				line += aMessage.to_string(iModel.message_strings(), true, true);
				new_entry(filename(aBuffer, Scrollback), line, Scrollback);
			}
			else
			{
				scrollbacker::messages_t& theMessages = (*i)->new_messages();
				static_cast<scrollbacker::buffer_messages&>(theMessages).push_back(aMessage);
			}
		}
		switch(aMessage.command())
		{
		case message::PRIVMSG:
			if (!(iEvents & Message))
				return;
			break;
		case message::NOTICE:
			if (!(iEvents & Notice))
				return;
			break;
		case message::JOIN:
			if (!(iEvents & JoinPartQuit))
				return;
			break;
		case message::PART:
			if (!(iEvents & JoinPartQuit))
				return;
			break;
		case message::QUIT:
			if (!(iEvents & JoinPartQuit))
				return;
			break;
		case message::KICK:
			if (!(iEvents & Kick))
				return;
			break;
		case message::MODE:
			if (!(iEvents & Mode))
				return;
			break;
		default:
			if (!(iEvents & Other))
				return;
			break;
		}
		new_entry(aBuffer, aMessage.to_nice_string(iModel.message_strings(), &aBuffer));
	}

	void logger::dcc_connection_added(dcc_connection& aConnection)
	{
		if (aConnection.type() != dcc_connection::CHAT)
			return;
		if (static_cast<dcc_buffer&>(aConnection).origin() == dcc_buffer::Listen)
			return;
		iLoggedDccChatConnections.push_back(static_cast<dcc_buffer*>(&aConnection));
		static_cast<dcc_buffer&>(aConnection).neolib::observable<dcc_buffer_observer>::add_observer(*this);
		std::string timestamp;
		get_timestamp(timestamp);
		new_entry(static_cast<dcc_buffer&>(aConnection), timestamp);
		if (iScrollbackLogs)
		{
			iScrollbackers.push_back(scrollbacker_pointer(new scrollbacker(*this, static_cast<dcc_buffer&>(aConnection))));
			iScrollbackers.back()->start();
		}
	}

	void logger::dcc_connection_removed(dcc_connection& aConnection)
	{
		if (aConnection.type() != dcc_connection::CHAT)
			return;
		iLoggedDccChatConnections.remove(static_cast<dcc_buffer*>(&aConnection));
		for (scrollbackers::iterator i = iScrollbackers.begin(); i != iScrollbackers.end(); ++i)
			if ((*i)->is(static_cast<dcc_buffer&>(aConnection)))
			{
				iScrollbackers.erase(i);
				break;
			}
	}

	void logger::dcc_chat_message(dcc_buffer& aBuffer, const dcc_message& aMessage)
	{
		if (aMessage.type() == dcc_message::STATUS)
			return;
		if (iScrollbackLogs)
		{
			scrollbackers::iterator i = scrollbacker_for_buffer(aBuffer);
			if (i == iScrollbackers.end())
			{
				std::string line = neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(aMessage.time()));
				line += (aMessage.direction() == dcc_message::INCOMING ? " < " : " > ");
				line += aMessage.to_string();
				new_entry(filename(aBuffer, Scrollback), line, Scrollback);
			}
			else
			{
				scrollbacker::messages_t theMessages = (**i).new_messages();
				static_cast<scrollbacker::dcc_messages&>(theMessages).push_back(aMessage);
			}
		}
		new_entry(aBuffer, aMessage.to_nice_string(iModel.message_strings(), aBuffer));
	}
}