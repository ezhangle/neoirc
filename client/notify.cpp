// notify.cpp
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
#include "model.hpp"
#include "notify.hpp"

namespace irc
{
	namespace
	{
		template <typename List, typename Iter, typename Result>
		Iter find_impl(const connection& aConnection, List& aList, const server_key& aServer, const user& aUser, const std::string& aChannel, Result& aResult)
		{
			for (Iter i = aList.entries().begin(); i != aList.entries().end(); ++i)
			{
				const notify_entry& theEntry = **i;
				if (aList.matches(aConnection, theEntry, aServer, aUser, aChannel))
					aResult.push_back(i);
			}
			if (!aResult.empty())
				return aResult.front();
			else
				return aList.entries().end();
		}
	}

	notify::container_type::const_iterator notify::find(const connection& aConnection, const server_key& aServer, const user& aUser, const std::string& aChannel) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const notify, container_type::const_iterator>(aConnection, *this, aServer, aUser, aChannel, result);
	}

	notify::container_type::iterator notify::find(const connection& aConnection, const server_key& aServer, const user& aUser, const std::string& aChannel)
	{
		std::vector<container_type::iterator> result;
		return find_impl<notify, container_type::iterator>(aConnection, *this, aServer, aUser, aChannel, result);
	}

	notify::container_type::const_iterator notify::find(const connection& aConnection, const notify_entry& aEntry) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const notify, container_type::const_iterator>(aConnection, *this, aEntry.server(), aEntry.user(), aEntry.channel(), result);
	}

	notify::container_type::iterator notify::find(const connection& aConnection, const notify_entry& aEntry)
	{
		std::vector<container_type::iterator> result;
		return find_impl<notify, container_type::iterator>(aConnection, *this, aEntry.server(), aEntry.user(), aEntry.channel(), result);
	}

	bool notify::matches(const connection& aConnection, const notify_entry& aEntry, const server_key& aServer, const user& aUser, const std::string& aChannel) const
	{
		if ((aEntry.server().first != aServer.first && aEntry.server().first != "*") ||
			(aEntry.server().second != aServer.second && aEntry.server().second != "*"))
			return false;
		if (irc::make_string(aConnection, aEntry.channel()) != aChannel && aEntry.channel() != "*" && aChannel != "*"
			&& (aEntry.channel() != "~" || aConnection.is_channel(aChannel)))
			return false;
		bool anyNickName = aEntry.user().nick_name().empty() || aEntry.user().nick_name() == "*";
		bool anyUserName = aEntry.user().user_name().empty() || aEntry.user().user_name() == "*";
		bool anyHostName = aEntry.user().host_name().empty() || aEntry.user().host_name() == "*";
		string::traits_type::pusher context(aConnection.casemapping());
		bool matchesNickName = neolib::wildcard_match(irc::make_string(aConnection.casemapping(), aUser.nick_name()), irc::make_string(aConnection.casemapping(), aEntry.user().nick_name()));
		bool matchesUserName = neolib::wildcard_match(irc::make_string(aConnection.casemapping(), aUser.user_name()), irc::make_string(aConnection.casemapping(), aEntry.user().user_name()));
		bool matchesHostName = neolib::wildcard_match(irc::make_string(aConnection.casemapping(), aUser.host_name()), irc::make_string(aConnection.casemapping(), aEntry.user().host_name()));
		if (matchesNickName && !anyNickName)
			return true;
		else if (matchesUserName && matchesHostName && (!anyUserName || !anyHostName))
			return true;
		else if (anyNickName && anyUserName && anyHostName)
			return true;
		return false;
	}

	notify_entry_ptr notify::share(const connection& aConnection, const notify_entry& aEntry)
	{
		notify::container_type::iterator i = find(aConnection, aEntry);
		if (i != iEntries.end())
			return *i;
		else
			return notify_entry_ptr();
	}

	bool notify::notified(const connection& aConnection, const server_key& aServer, const user& aUser, const std::string& aChannel) const
	{
		return find(aConnection, aServer, aUser, aChannel) != iEntries.end();
	}

	bool notify::notified(const connection& aConnection, const server_key& aServer, const user& aUser, const std::string& aChannel, message::command_e aCommand) const
	{
		std::vector<container_type::const_iterator> result;
		find_impl<const notify, container_type::const_iterator>(aConnection, *this, aServer, aUser, aChannel, result);
		if (result.empty())
			return false;
		for (std::vector<container_type::const_iterator>::const_iterator i = result.begin(); i != result.end(); ++i)
		{
			const notify_entry& entry = ***i;
			switch(aCommand)
			{
			case message::PRIVMSG:
			case message::NOTICE:
				if (entry.event() & notify_entry::Message)
					return true;
			case message::JOIN:
				if (entry.event() & notify_entry::Join)
					return true;
			case message::PART:
			case message::QUIT:
				if (entry.event() & notify_entry::PartQuit)
					return true;
			}
		}
		return false;
	}

	bool notify::add(const notify_entry& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (**i == aEntry)
				return false;
		iEntries.push_back(notify_entry_ptr(new notify_entry(aEntry)));
		if (!iLoading)
			notify_observers(notify_list_observer::NotifyAdded, *iEntries.back());
		return true;
	}

	void notify::update(notify_entry& aExistingEntry, const notify_entry& aNewEntry)
	{
		aExistingEntry = aNewEntry;
		if (!iLoading)
			notify_observers(notify_list_observer::NotifyUpdated, aExistingEntry);
	}

	void notify::update_user(const connection& aConnection, const server_key& aServer, const user& aOldUser, const user& aNewUser)
	{
		std::vector<container_type::iterator> result;
		find_impl<notify, container_type::iterator>(aConnection, *this, aServer, aOldUser, "*", result);
		for (std::vector<container_type::iterator>::iterator i = result.begin(); i != result.end(); ++i)
		{
			notify_entry& existingEntry = ***i;
			if (existingEntry.user().nick_name().empty() || existingEntry.user().nick_name() == "*")
				continue;
			if (!existingEntry.user().has_user_name() && !existingEntry.user().has_host_name())
			{
				notify_entry newEntry(existingEntry);
				newEntry.user().host_name() = aNewUser.host_name();
				newEntry.user().user_name() = aNewUser.user_name();
				update(existingEntry, newEntry);
			}
		}
	}

	void notify::remove(const notify_entry& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (&**i == &aEntry)
			{
				notify_entry_ptr temp = *i;
				iEntries.erase(i);
				notify_observers(notify_list_observer::NotifyRemoved, *temp);
				break;
			}
	}

	void notify::notify_observer(notify_list_observer& aObserver, notify_list_observer::notify_type aType, const void* aParameter, const void*)
	{
		switch(aType)
		{
		case notify_list_observer::NotifyAdded:
			aObserver.notify_added(*static_cast<const notify_entry*>(aParameter));
			break;
		case notify_list_observer::NotifyUpdated:
			aObserver.notify_updated(*static_cast<const notify_entry*>(aParameter));
			break;
		case notify_list_observer::NotifyRemoved:
			aObserver.notify_removed(*static_cast<const notify_entry*>(aParameter));
			break;
		}
	}

	void read_notify_list(model& aModel, std::function<bool()> aErrorFunction)
	{
		notify& theNotifyList = aModel.notify_list();

		neolib::xml xmlNotifyList(true);
		std::ifstream input((aModel.root_path() + "notify_list.xml").c_str());
		xmlNotifyList.read(input);

		if (xmlNotifyList.error() && aErrorFunction && aErrorFunction())
		{
			theNotifyList.entries().clear();
			write_notify_list(aModel);
			return;
		}

		theNotifyList.loading(true);

		for (neolib::xml::element::const_iterator i = xmlNotifyList.root().begin(); i != xmlNotifyList.root().end(); ++i)
		{
			if (i->name() == "entry")
			{
				notify_entry_ptr e(new notify_entry);
				for (neolib::xml::element::const_iterator j = i->begin(); j != i->end(); ++j)
				{
					if (j->name() == "user")
						e->user() = user(j->attribute_value("value"), casemapping::rfc1459, false, true);
					if (j->name() == "server_network" || j->name() == "server_group")
						e->server().first = j->attribute_value("value");
					if (j->name() == "server_name")
						e->server().second = j->attribute_value("value");
					if (j->name() == "channel")
						e->channel() = j->attribute_value("value");
					if (j->name() == "action")
						e->action() = static_cast<notify_entry::action_e>(neolib::string_to_integer<char>(j->attribute_value("value")));
					if (j->name() == "event")
						e->event() = static_cast<notify_entry::event_e>(neolib::string_to_integer<char>(j->attribute_value("value")));
					if (j->name() == "data")
						e->data() = j->attribute_value("value");
				}
				if (e->user().nick_name().empty())
					e->user().nick_name() = "*";
				if (e->channel().empty())
					e->channel() = "*";
				theNotifyList.entries().push_back(e);
			}
		}

		theNotifyList.loading(false);
	}

	void write_notify_list(const model& aModel)
	{
		const notify& theNotifyList = aModel.notify_list();

		neolib::xml xmlNotifyList(true);

		xmlNotifyList.root().name() = "notify_list";

		for (notify::container_type::const_iterator i = theNotifyList.entries().begin(); i != theNotifyList.entries().end(); ++i)
		{
			const notify_entry& entry = **i;
			neolib::xml::element& xmlEntry = xmlNotifyList.append(xmlNotifyList.root(), "entry");
			xmlNotifyList.append(xmlEntry, "user").set_attribute("value", entry.user().msgto_form());
			xmlNotifyList.append(xmlEntry, "server_network").set_attribute("value", entry.server().first);
			xmlNotifyList.append(xmlEntry, "server_name").set_attribute("value", entry.server().second);
			xmlNotifyList.append(xmlEntry, "channel").set_attribute("value", entry.channel());
			xmlNotifyList.append(xmlEntry, "action").set_attribute("value", neolib::integer_to_string<char>(entry.action()));
			xmlNotifyList.append(xmlEntry, "event").set_attribute("value", neolib::integer_to_string<char>(entry.event()));
			xmlNotifyList.append(xmlEntry, "data").set_attribute("value", entry.data());
		}

		std::ofstream output((aModel.root_path() + "notify_list.xml").c_str());
		xmlNotifyList.write(output);
	}
}