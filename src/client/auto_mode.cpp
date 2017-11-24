// auto_mode.cpp
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
#include <neolib/xml.hpp>
#include <fstream>
#include <neoirc/client/model.hpp>
#include <neoirc/client/auto_mode.hpp>

namespace irc
{
	namespace
	{
		template <typename List, typename Iter, typename Result>
		Iter find_impl(casemapping::type aCasemapping, List& aList, const server_key& aServer, const user& aUser, const std::string& aChannel, Result& aResult)
		{
			for (Iter i = aList.entries().begin(); i != aList.entries().end(); ++i)
			{
				const auto_mode_entry& theEntry = *i;
				if (aList.matches(aCasemapping, theEntry, aServer, aUser, aChannel))
					aResult.push_back(i);
			}
			if (!aResult.empty())
				return aResult.front();
			else
				return aList.entries().end();
		}
	}

	auto_mode::container_type::const_iterator auto_mode::find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser, const std::string& aChannel) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const auto_mode, container_type::const_iterator>(aCasemapping, *this, aServer, aUser, aChannel, result);
	}

	auto_mode::container_type::iterator auto_mode::find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser, const std::string& aChannel)
	{
		std::vector<container_type::iterator> result;
		return find_impl<auto_mode, container_type::iterator>(aCasemapping, *this, aServer, aUser, aChannel, result);
	}

	auto_mode::container_type::const_iterator auto_mode::find(casemapping::type aCasemapping, const auto_mode_entry& aEntry) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const auto_mode, container_type::const_iterator>(aCasemapping, *this, aEntry.server(), aEntry.user(), aEntry.channel(), result);
	}

	auto_mode::container_type::iterator auto_mode::find(casemapping::type aCasemapping, const auto_mode_entry& aEntry)
	{
		std::vector<container_type::iterator> result;
		return find_impl<auto_mode, container_type::iterator>(aCasemapping, *this, aEntry.server(), aEntry.user(), aEntry.channel(), result);
	}

	bool auto_mode::matches(casemapping::type aCasemapping, const auto_mode_entry& aEntry, const server_key& aServer, const user& aUser, const std::string& aChannel) const
	{
		if (aEntry.server().first != aServer.first ||
			(aEntry.server().second != aServer.second && aEntry.server().second != "*"))
			return false;
		if (irc::make_string(aCasemapping, aEntry.channel()) != aChannel && aEntry.channel() != "*" && aChannel != "*")
			return false;
		bool anyNickName = aEntry.user().nick_name().empty() || aEntry.user().nick_name() == "*";
		bool anyUserName = aEntry.user().user_name().empty() || aEntry.user().user_name() == "*";
		bool anyHostName = aEntry.user().host_name().empty() || aEntry.user().host_name() == "*";
		string::traits_type::pusher context(aCasemapping);
		bool matchesNickName = neolib::wildcard_match(irc::make_string(aCasemapping, aUser.nick_name()), irc::make_string(aCasemapping, aEntry.user().nick_name()));
		bool matchesUserName = neolib::wildcard_match(irc::make_string(aCasemapping, aUser.user_name()), irc::make_string(aCasemapping, aEntry.user().user_name()));
		bool matchesHostName = neolib::wildcard_match(irc::make_string(aCasemapping, aUser.host_name()), irc::make_string(aCasemapping, aEntry.user().host_name()));
		if (matchesNickName && !anyNickName)
			return true;
		else if (matchesUserName && matchesHostName && (!anyUserName || !anyHostName))
			return true;
		else if (anyNickName && anyUserName && anyHostName)
			return true;
		return false;
	}

	bool auto_mode::has_auto_mode(casemapping::type aCasemapping, const server_key& aServer, const user& aUser, const std::string& aChannel) const
	{
		return find(aCasemapping, aServer, aUser, aChannel) != iEntries.end();
	}

	bool auto_mode::has_auto_mode(casemapping::type aCasemapping, const server_key& aServer, const user& aUser, const std::string& aChannel, auto_mode_entry::type_e aType) const
	{
		std::vector<container_type::const_iterator> result;
		find_impl<const auto_mode, container_type::const_iterator>(aCasemapping, *this, aServer, aUser, aChannel, result);
		for (std::vector<container_type::const_iterator>::const_iterator i = result.begin(); i != result.end(); ++i)
			if ((**i).type() == aType)
				return true;
		return false;
	}

	bool auto_mode::add(const server_key& aServer, const user& aUser, const std::string& aChannel, auto_mode_entry::type_e aType, const std::string& aData)
	{
		if (has_auto_mode(casemapping::ascii, aServer, aUser, aChannel, aType))
		{
			auto_mode_entry& existingEntry = *find(casemapping::ascii, aServer, aUser, aChannel);
			existingEntry.type() = aType;
			existingEntry.data() = aData;
			if (existingEntry.user() != aUser ||
				existingEntry.user().user_name() != aUser.user_name() ||
				existingEntry.user().host_name() != aUser.host_name())
			{
				existingEntry.user() = aUser;
				if (!iLoading)
					notify_observers(auto_mode_list_observer::NotifyUpdated, existingEntry);
			}
			return false;
		}

		iEntries.push_back(auto_mode_entry(aServer, aUser, aChannel, aType, aData));
		if (!iLoading)
			notify_observers(auto_mode_list_observer::NotifyAdded, iEntries.back());

		return true;
	}

	bool auto_mode::add(const auto_mode_entry& aEntry)
	{
		return add(aEntry.server(), aEntry.user(), aEntry.channel(), aEntry.type(), aEntry.data());
	}

	void auto_mode::update(auto_mode_entry& aExistingEntry, const auto_mode_entry& aNewEntry)
	{
		aExistingEntry = aNewEntry;
		if (!iLoading)
			notify_observers(auto_mode_list_observer::NotifyUpdated, aExistingEntry);
	}

	void auto_mode::update_user(casemapping::type aCasemapping, const server_key& aServer, const user& aOldUser, const user& aNewUser)
	{
		std::vector<container_type::iterator> result;
		find_impl<auto_mode, container_type::iterator>(aCasemapping, *this, aServer, aOldUser, "*", result);
		for (std::vector<container_type::iterator>::iterator i = result.begin(); i != result.end(); ++i)
		{
			auto_mode_entry& existingEntry = **i;
			if (existingEntry.user().nick_name().empty() || existingEntry.user().nick_name() == "*")
				continue;
			if (!existingEntry.user().has_user_name() && !existingEntry.user().has_host_name())
			{
				auto_mode_entry newEntry(existingEntry);
				newEntry.user().host_name() = aNewUser.host_name();
				newEntry.user().user_name() = aNewUser.user_name();
				update(existingEntry, newEntry);
			}
		}
	}

	void auto_mode::remove(const server_key& aServer, const user& aUser, const std::string& aChannel)
	{
		container_type::iterator item = find(casemapping::ascii, aServer, aUser, aChannel);
		if (item == iEntries.end())
			return;

		container_type temp;
		temp.splice(temp.begin(), iEntries, item);
		notify_observers(auto_mode_list_observer::NotifyRemoved, *temp.begin());
	}

	void auto_mode::remove(const auto_mode_entry& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (&*i == &aEntry)
			{
				container_type temp;
				temp.splice(temp.begin(), iEntries, i);
				notify_observers(auto_mode_list_observer::NotifyRemoved, *temp.begin());
				break;
			}
	}

	void auto_mode::notify_observer(auto_mode_list_observer& aObserver, auto_mode_list_observer::notify_type aType, const void* aParameter, const void*)
	{
		switch(aType)
		{
		case auto_mode_list_observer::NotifyAdded:
			aObserver.auto_mode_added(*static_cast<const auto_mode_entry*>(aParameter));
			break;
		case auto_mode_list_observer::NotifyUpdated:
			aObserver.auto_mode_updated(*static_cast<const auto_mode_entry*>(aParameter));
			break;
		case auto_mode_list_observer::NotifyRemoved:
			aObserver.auto_mode_removed(*static_cast<const auto_mode_entry*>(aParameter));
			break;
		}
	}

	void read_auto_mode_list(model& aModel, std::function<bool()> aErrorFunction)
	{
		auto_mode& theAutoModeList = aModel.auto_mode_list();

		neolib::xml xmlAutoModeList(true);
		std::ifstream input((aModel.root_path() + "auto_mode_list.xml").c_str());
		xmlAutoModeList.read(input);

		if (xmlAutoModeList.error() && aErrorFunction && aErrorFunction())
		{
			theAutoModeList.entries().clear();
			write_auto_mode_list(aModel);
			return;
		}

		theAutoModeList.loading(true);

		for (neolib::xml::element::const_iterator i = xmlAutoModeList.root().begin(); i != xmlAutoModeList.root().end(); ++i)
		{
			if (i->name() == "entry")
			{
				theAutoModeList.entries().push_back(auto_mode_entry());
				auto_mode_entry& e = theAutoModeList.entries().back();
				for (neolib::xml::element::const_iterator j = i->begin(); j != i->end(); ++j)
				{
					if (j->name() == "user")
						e.user() = user(j->attribute_value("value"), casemapping::rfc1459, false, true);
					if (j->name() == "server_network" || j->name() == "server_group")
						e.server().first = j->attribute_value("value");
					if (j->name() == "server_name")
						e.server().second = j->attribute_value("value");
					if (j->name() == "channel")
						e.channel() = j->attribute_value("value");
					if (j->name() == "type")
						e.type() = static_cast<auto_mode_entry::type_e>(neolib::string_to_integer<char>(j->attribute_value("value")));
					if (j->name() == "data")
						e.data() = j->attribute_value("value");
				}
			}
		}

		theAutoModeList.loading(false);
	}

	void write_auto_mode_list(const model& aModel)
	{
		const auto_mode& theAutoModeList = aModel.auto_mode_list();

		neolib::xml xmlAutoModeList(true);

		xmlAutoModeList.root().name() = "auto_mode_list";

		for (auto_mode::container_type::const_iterator i = theAutoModeList.entries().begin(); i != theAutoModeList.entries().end(); ++i)
		{
			const auto_mode_entry& entry = *i;
			neolib::xml::element& xmlEntry = xmlAutoModeList.append(xmlAutoModeList.root(), "entry");
			xmlAutoModeList.append(xmlEntry, "user").set_attribute("value", entry.user().msgto_form());
			xmlAutoModeList.append(xmlEntry, "server_network").set_attribute("value", entry.server().first);
			xmlAutoModeList.append(xmlEntry, "server_name").set_attribute("value", entry.server().second);
			xmlAutoModeList.append(xmlEntry, "channel").set_attribute("value", entry.channel());
			xmlAutoModeList.append(xmlEntry, "type").set_attribute("value", neolib::integer_to_string<char>(entry.type()));
			xmlAutoModeList.append(xmlEntry, "data").set_attribute("value", entry.data());
		}

		std::ofstream output((aModel.root_path() + "auto_mode_list.xml").c_str());
		xmlAutoModeList.write(output);
	}
}