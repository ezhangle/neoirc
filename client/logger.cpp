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
#include "logger.hpp"
#include "model.hpp"

namespace irc
{
	namespace
	{
		enum { LoggerBufferSize = 4096 };
	}

	void logger::converter::task()
	{
		switch(iType)
		{
		case ToAnsi:
			{
				std::ofstream out((iFileName + ".tmp").c_str(), std::ios::out|std::ios::app|std::ios::binary);
				std::ifstream in(iFileName.c_str(), std::ios::in|std::ios::binary);
				in.seekg(0, std::ios::end);
				long count = static_cast<long>(in.tellg());
				in.seekg(0, std::ios::beg);
				unsigned char BOM[2];
				in.read(reinterpret_cast<char*>(BOM), 2);
				count -= 2;
				while(count > 0 && running())
				{
					char buffer[LoggerBufferSize];
					in.read(buffer, count > LoggerBufferSize ? LoggerBufferSize : count);
					std::wstring wideString;
					wideString.assign(reinterpret_cast<wchar_t*>(buffer), 
						reinterpret_cast<wchar_t*>(buffer) + ((count > LoggerBufferSize ? LoggerBufferSize : count) / sizeof(wchar_t)));
					std::string narrowString = neolib::wide_to_utf8(wideString);
					out.write(narrowString.c_str(), narrowString.size());
					count -= LoggerBufferSize;
				}
			}
			break;
		case ToUnicode:
			{
				std::ofstream out((iFileName + ".tmp").c_str(), std::ios::out|std::ios::app|std::ios::binary);
				wchar_t BOM = 0xFEFF;
				out.write(reinterpret_cast<char*>(&BOM), 2);
				std::ifstream in(iFileName.c_str(), std::ios::in|std::ios::binary);
				in.seekg(0, std::ios::end);
				long count = static_cast<long>(in.tellg());
				in.seekg(0, std::ios::beg);
				while(count > 0 && running())
				{
					char buffer[LoggerBufferSize];
					in.read(buffer, count > LoggerBufferSize ? LoggerBufferSize : count);
					std::string narrowString;
					narrowString.assign(buffer, buffer + (count > LoggerBufferSize ? LoggerBufferSize : count));
					std::wstring wideString = neolib::utf8_to_wide(narrowString);
					out.write(reinterpret_cast<const char*>(wideString.c_str()), wideString.size() * sizeof(wchar_t));
					count -= LoggerBufferSize;
				}
			}
			break;
		}
		if (running())
		{
			remove(iFileName.c_str());
			rename((iFileName + ".tmp").c_str(), iFileName.c_str());
		}
		else
			cleanup();
	}

	void logger::converter::cleanup()
	{
		remove((iFileName + ".tmp").c_str());
		iParent.cancel_conversion();
	}

	bool logger::reloader::is(irc::buffer& aBuffer) const
	{
		if (iBuffer.is<irc::buffer*>() && static_cast<irc::buffer*>(iBuffer) == &aBuffer)
			return true;
		else
			return false;
	}

	bool logger::reloader::is(dcc_buffer& aBuffer) const
	{
		if (iBuffer.is<dcc_buffer*>() && static_cast<dcc_buffer*>(iBuffer) == &aBuffer)
			return true;
		else
			return false;
	}

	void logger::reloader::task()
	{
		std::string filename;
		std::string sep = "\r\n";
		bool isIRC = true;
		if (iBuffer.is<irc::buffer*>())
		{
			filename = iParent.filename(*static_cast<irc::buffer*>(iBuffer), logger::Reload);
			iMessages = buffer_messages();
		}
		else
		{
			filename = iParent.filename(*static_cast<dcc_buffer*>(iBuffer), logger::Reload);
			iMessages = dcc_messages();
			isIRC = false;
		}

		std::ifstream reloaderFile(filename.c_str(), std::ios::in|std::ios::binary);
		if (reloaderFile)
		{
			reloaderFile.seekg(0, std::ios_base::end);
			std::ifstream::pos_type pos = reloaderFile.tellg();
			bool chopFile = (static_cast<std::size_t>(pos) > iParent.iReloadSize * 1024);
			std::string text;
			bool finished = false;
			while(pos > 0 && running() && !finished)
			{
				std::ifstream::off_type offset = (pos < LoggerBufferSize ? pos : LoggerBufferSize);
				reloaderFile.seekg(-offset, std::ios_base::cur);
				char more[LoggerBufferSize];
				reloaderFile.read(more, offset);
				reloaderFile.seekg(-offset, std::ios_base::cur);
				pos = reloaderFile.tellg();
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
				reloaderFile.seekg(-(static_cast<std::ifstream::off_type>(iParent.iReloadSize) * 1024 / 2), std::ios_base::end);
				std::ofstream choppedReloaderFile((filename + ".tmp").c_str(), std::ios::out|std::ios::app|std::ios::binary);
				char buffer[LoggerBufferSize];
				long count = iParent.iReloadSize * 1024 / 2;
				bool hadNewline = false;
				while(count > 0 && running())
				{
					long readLength = count > LoggerBufferSize ? LoggerBufferSize : count;
					reloaderFile.read(buffer, readLength);
					if (!hadNewline)
					{
						char* newLine = std::find(buffer, buffer+readLength, '\n');
						if (newLine != buffer+readLength)
						{
							hadNewline = true;
							if (newLine != buffer+readLength-1)
								choppedReloaderFile.write(newLine+1, (buffer+readLength) - (newLine+1));
						}
					}
					else
					{
						choppedReloaderFile.write(buffer, readLength);
					}
					count -= LoggerBufferSize;
				}
				if (running())
				{
					reloaderFile.close();
					choppedReloaderFile.close();
					remove(filename.c_str());
					rename((filename + ".tmp").c_str(), filename.c_str());
				}
			}
		}
		if (!running())
			remove((filename + ".tmp").c_str());
	}

	std::string logger::sConvertStartMessage;
	std::string logger::sConvertEndMessage;

	logger::logger(model& aModel, connection_manager& aConnectionManager, dcc_connection_manager& aDccConnectionManager) :
		iModel(aModel), iConnectionManager(aConnectionManager), iDccConnectionManager(aDccConnectionManager),
		iEnabled(false), iCancelConversion(false), iEvents(Message), iServerLog(false),
		iReloading(false), iReloadSize(1000), iArchive(false), iArchiveSize(1000),
		iUpdateTimer(aModel.owner_thread(), [this](neolib::timer_callback&){ process_pending(); iUpdateTimer.again(); })
	{
		iConnectionManager.add_observer(*this);
		iDccConnectionManager.add_observer(*this);
	}

	logger::~logger()
	{
		iReloaders.clear();
		cancel_conversion();
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
			if (!iModel.is_unicode(**i))
			{
				std::string timestamp;
				get_timestamp(timestamp, true);
				new_entry(**i, timestamp);
			}
			else
			{
				std::wstring timestamp;
				get_timestamp(timestamp, true);
				new_entry(**i, timestamp);
			}
		}
	}

	void logger::timestamp_servers()
	{
		for (logged_buffers::iterator i = iLoggedBuffers.begin(); i != iLoggedBuffers.end(); ++i)
		{
			if ((**i).type() != buffer::SERVER)
				continue;
			if (!iModel.is_unicode(**i))
			{
				std::string timestamp;
				get_timestamp(timestamp, true);
				new_entry(**i, timestamp);
			}
			else
			{
				std::wstring timestamp;
				get_timestamp(timestamp, true);
				new_entry(**i, timestamp);
			}
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
		for (reloaders::iterator i = iReloaders.begin(); i != iReloaders.end();)
		{
			reloader& theReloader = **i;
			if (!theReloader.running())
			{
				if (theReloader.buffer().is<buffer*>())
				{
					buffer& theBuffer = static_cast<buffer&>(*static_cast<buffer*>(theReloader.buffer()));
					reloader::messages_t& theMessages = theReloader.messages();
					theBuffer.reload(static_cast<reloader::buffer_messages&>(theMessages));
					reloader::messages_t& theNewMessages = theReloader.new_messages();
					for (reloader::buffer_messages::iterator j = static_cast<reloader::buffer_messages&>(theNewMessages).begin(); 
						j != static_cast<reloader::buffer_messages&>(theNewMessages).end(); ++j)
					{
						message& theMessage = *j;
						std::string line = neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(theMessage.time()));
						line += (theMessage.direction() == message::INCOMING ? " < " : " > ");
						line += theMessage.to_string(iModel.message_strings(), true, true);
						new_entry(filename(theBuffer, Reload), line, Reload);
					}
				}
				else
				{
					dcc_buffer& theBuffer = static_cast<dcc_buffer&>(*static_cast<dcc_buffer*>(theReloader.buffer()));
					reloader::messages_t& theMessages = theReloader.messages();
					theBuffer.reload(static_cast<reloader::dcc_messages&>(theMessages));
					reloader::messages_t& theNewMessages = theReloader.new_messages();
					for (reloader::dcc_messages::iterator j = static_cast<reloader::dcc_messages&>(theNewMessages).begin(); 
						j != static_cast<reloader::dcc_messages&>(theNewMessages).end(); ++j)
					{
						dcc_message& theMessage = *j;
						std::string line = neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(theMessage.time()));
						line += (theMessage.direction() == dcc_message::INCOMING ? " < " : " > ");
						line += theMessage.to_string();
						new_entry(filename(theBuffer, Reload), line, Reload);
					}
				}
				i = iReloaders.erase(i);
			}
			else
				++i;
		}

		if (converting() && iConverter->finished())
			iConverter.reset();
		if (!pending() || converting())
			return;
		while(!converting() && pending())
		{
			if (iPendingEntries.front().second.is<std::string>())
				new_entry(iPendingEntries.front().first, static_cast<const std::string&>(iPendingEntries.front().second));
			else
				new_entry(iPendingEntries.front().first, static_cast<const std::wstring&>(iPendingEntries.front().second));
			if (converting())
				iPendingEntries.pop_back(); // pop the entry just added with new_entry
			else
				iPendingEntries.pop_front();
		}
	}

	void logger::cancel_conversion()
	{
		if (converting() && iConverter->running())
			iConverter.reset();
		iCancelConversion = true;
		process_pending();
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

	void logger::get_timestamp(std::wstring& aTimeStamp, bool aContinuation)
	{
		std::time_t ttTime = std::time(0);
		std::tm* tmTime = std::localtime(&ttTime);
		std::setlocale(LC_TIME, "");
		std::vector<wchar_t> strTime;
		strTime.resize(LoggerBufferSize);
		if (!aContinuation)
			wcsftime(&*strTime.begin(), LoggerBufferSize, L"-- %a %b %#d %H:%M:%S %Y --\r\n", tmTime);
		else
			wcsftime(&*strTime.begin(), LoggerBufferSize, L"++ %a %b %#d %H:%M:%S %Y ++\r\n", tmTime);
		aTimeStamp = std::wstring(&*strTime.begin());
	}

	std::string logger::filename(const buffer& aBuffer, filename_type_e aType)
	{
		std::string fileName;
		if (aBuffer.type() != buffer::SERVER)
			fileName = aBuffer.name();
		else
			fileName = "Server";
		fileName = fileName + " (" + aBuffer.connection().server().network()/* + " - " + aBuffer.connection().server().name()*/ + (aType == Reload ? ").irc" : ").txt");
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

	void logger::new_entry(buffer& aBuffer, const std::wstring& aText)
	{
		if (aBuffer.type() == buffer::SERVER && !iServerLog)
			return;

		new_entry(filename(aBuffer), aText);
	}

	std::string logger::filename(const dcc_buffer& aBuffer, filename_type_e aType)
	{
		std::string fileName = aBuffer.name() + (aType == Reload ? ".irc" : ".txt");
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

	void logger::new_entry(dcc_buffer& aBuffer, const std::wstring& aText)
	{
		new_entry(filename(aBuffer), aText);
	}

	void logger::new_entry(const std::string& aFileName, const std::string& aText, filename_type_e aType)
	{
		if (!iEnabled)
			return;
		bool convert = false;
		if (aType == Normal)
		{
			if (converting() && iConverter->file_name() == aFileName)
			{
				if (iConverter->type() == converter::ToAnsi)
					iPendingEntries.push_back(std::make_pair(aFileName, line(aText)));
				else
					iPendingEntries.push_back(std::make_pair(aFileName, line(neolib::utf8_to_wide(aText))));
				return;
			}
			convert = true;
			std::ifstream logfile(aFileName.c_str(), std::ios::in|std::ios::binary);
			wchar_t BOM;
			logfile.read(reinterpret_cast<char*>(&BOM), 2);
			if (!logfile || BOM != 0xFEFF)
				convert = false;
		}
		if (convert)
		{
			if (converting())
				iPendingEntries.push_back(std::make_pair(aFileName, line(aText)));
			else if (!iCancelConversion)
			{
				iPendingEntries.push_back(std::make_pair(aFileName, line(aText)));
				iConverter.reset(new converter(*this, converter::ToAnsi, aFileName));
				iConverter->start();
			}
			else
				new_entry(aFileName, neolib::utf8_to_wide(aText));
		}
		else
		{
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
	}

	void logger::new_entry(const std::string& aFileName, const std::wstring& aText, filename_type_e aType)
	{
		if (!iEnabled)
			return;
		bool convert = false;
		if (aType == Normal)
		{
			if (converting() && iConverter->file_name() == aFileName)
			{
				if (iConverter->type() == converter::ToUnicode)
					iPendingEntries.push_back(std::make_pair(aFileName, line(aText)));
				else
					iPendingEntries.push_back(std::make_pair(aFileName, line(neolib::wide_to_utf8(aText))));
				return;
			}
			std::ifstream logfile(aFileName.c_str(), std::ios::in|std::ios::binary);
			wchar_t BOM;
			logfile.read(reinterpret_cast<char*>(&BOM), 2);
			if (!logfile || BOM != 0xFEFF)
				convert = true;
		}
		if (convert)
		{
			if (converting())
				iPendingEntries.push_back(std::make_pair(aFileName, line(aText)));
			else if (!iCancelConversion)
			{
				iPendingEntries.push_back(std::make_pair(aFileName, line(aText)));
				iConverter.reset(new converter(*this, converter::ToUnicode, aFileName));
				iConverter->start();
			}
			else
				new_entry(aFileName, neolib::wide_to_utf8(aText));
		}
		else
		{
			std::ofstream::pos_type length;
			{
				std::ofstream logfile(aFileName.c_str(), std::ios::out|std::ios::app|std::ios::binary);
				logfile.write(reinterpret_cast<const char*>(aText.c_str()), aText.size() * sizeof(wchar_t));
				length = logfile.tellp();
			}
			if (aType == Normal && iArchive && length > (static_cast<std::ofstream::pos_type>(iArchiveSize) * 1024))
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
							std::wstring timestamp;
							get_timestamp(timestamp, true);
							new_entry(aFileName, timestamp, aType);
						}
						break;
					}
				}
			}
		}
	}

	logger::reloaders::iterator logger::reloader_for_buffer(buffer& aBuffer)
	{
		for (reloaders::iterator i = iReloaders.begin(); i != iReloaders.end(); ++i)
			if ((*i)->is(aBuffer))
				return i;
		return iReloaders.end();
	}

	logger::reloaders::iterator logger::reloader_for_buffer(dcc_buffer& aBuffer)
	{
		for (reloaders::iterator i = iReloaders.begin(); i != iReloaders.end(); ++i)
			if ((*i)->is(aBuffer))
				return i;
		return iReloaders.end();
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
		if (!iModel.is_unicode(aBuffer))
		{
			std::string timestamp;
			get_timestamp(timestamp);
			new_entry(aBuffer, timestamp);
		}
		else
		{
			std::wstring timestamp;
			get_timestamp(timestamp);
			new_entry(aBuffer, timestamp);
		}
		if (iReloading && (aBuffer.type() != buffer::SERVER || iServerLog))
		{
			iReloaders.push_back(reloader_pointer(new reloader(*this, aBuffer)));
			iReloaders.back()->start();
		}
	}

	void logger::buffer_removed(buffer& aBuffer)
	{
		iLoggedBuffers.remove(&aBuffer);
		for (reloaders::iterator i = iReloaders.begin(); i != iReloaders.end(); ++i)
			if ((*i)->is(aBuffer))
			{
				iReloaders.erase(i);
				break;
			}
	}

	void logger::buffer_message(buffer& aBuffer, const message& aMessage)
	{
		if (aBuffer.type() == buffer::SERVER && !iServerLog)
			return;
		if (iReloading)
		{
			reloaders::iterator i = reloader_for_buffer(aBuffer);
			if (i == iReloaders.end())
			{
				std::string line = neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(aMessage.time()));
				line += (aMessage.direction() == message::INCOMING ? " < " : " > ");
				line += aMessage.to_string(iModel.message_strings(), true, true);
				new_entry(filename(aBuffer, Reload), line, Reload);
			}
			else
			{
				reloader::messages_t& theMessages = (*i)->new_messages();
				static_cast<reloader::buffer_messages&>(theMessages).push_back(aMessage);
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
		if (!iModel.is_unicode(aBuffer))
			new_entry(aBuffer, aMessage.to_nice_string(iModel.message_strings(), &aBuffer));
		else
			new_entry(aBuffer, neolib::utf8_to_wide(aMessage.to_nice_string(iModel.message_strings(), &aBuffer)));
	}

	void logger::dcc_connection_added(dcc_connection& aConnection)
	{
		if (aConnection.type() != dcc_connection::CHAT)
			return;
		if (static_cast<dcc_buffer&>(aConnection).origin() == dcc_buffer::Listen)
			return;
		iLoggedDccChatConnections.push_back(static_cast<dcc_buffer*>(&aConnection));
		static_cast<dcc_buffer&>(aConnection).neolib::observable<dcc_buffer_observer>::add_observer(*this);
		if (!iModel.is_unicode(aConnection))
		{
			std::string timestamp;
			get_timestamp(timestamp);
			new_entry(static_cast<dcc_buffer&>(aConnection), timestamp);
		}
		else
		{
			std::wstring timestamp;
			get_timestamp(timestamp);
			new_entry(static_cast<dcc_buffer&>(aConnection), timestamp);
		}
		if (iReloading)
		{
			iReloaders.push_back(reloader_pointer(new reloader(*this, static_cast<dcc_buffer&>(aConnection))));
			iReloaders.back()->start();
		}
	}

	void logger::dcc_connection_removed(dcc_connection& aConnection)
	{
		if (aConnection.type() != dcc_connection::CHAT)
			return;
		iLoggedDccChatConnections.remove(static_cast<dcc_buffer*>(&aConnection));
		for (reloaders::iterator i = iReloaders.begin(); i != iReloaders.end(); ++i)
			if ((*i)->is(static_cast<dcc_buffer&>(aConnection)))
			{
				iReloaders.erase(i);
				break;
			}
	}

	void logger::dcc_chat_message(dcc_buffer& aBuffer, const dcc_message& aMessage)
	{
		if (aMessage.type() == dcc_message::STATUS)
			return;
		if (iReloading)
		{
			reloaders::iterator i = reloader_for_buffer(aBuffer);
			if (i == iReloaders.end())
			{
				std::string line = neolib::unsigned_integer_to_string<char>(static_cast<unsigned long>(aMessage.time()));
				line += (aMessage.direction() == dcc_message::INCOMING ? " < " : " > ");
				line += aMessage.to_string();
				new_entry(filename(aBuffer, Reload), line, Reload);
			}
			else
			{
				reloader::messages_t theMessages = (**i).new_messages();
				static_cast<reloader::dcc_messages&>(theMessages).push_back(aMessage);
			}
		}
		if (!iModel.is_unicode(aBuffer))
			new_entry(aBuffer, aMessage.to_nice_string(iModel.message_strings(), aBuffer));
		else
			new_entry(aBuffer, neolib::utf8_to_wide(aMessage.to_nice_string(iModel.message_strings(), aBuffer)));
	}
}