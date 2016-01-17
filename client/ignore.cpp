// ignore_list.cpp
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
#include "ignore.hpp"

namespace irc
{
	namespace
	{
		template <typename List, typename Iter, typename Result>
		Iter find_impl(casemapping::type aCasemapping, List& aList, const server_key& aServer, const user& aUser, Result& aResult)
		{
			for (Iter i = aList.entries().begin(); i != aList.entries().end(); ++i)
			{
				const ignore_entry& theEntry = *i;
				if (theEntry.server().first != aServer.first ||
					(theEntry.server().second != aServer.second && theEntry.server().second != "*" && aServer.second != "*"))
					continue;
				bool anyNickName = theEntry.user().nick_name().empty() || theEntry.user().nick_name() == "*";
				bool anyUserName = theEntry.user().user_name().empty() || theEntry.user().user_name() == "*";
				bool anyHostName = theEntry.user().host_name().empty() || theEntry.user().host_name() == "*";
				string::traits_type::pusher context(aCasemapping);
				bool matchesNickName = neolib::wildcard_match(irc::make_string(aCasemapping, aUser.nick_name()), irc::make_string(aCasemapping, theEntry.user().nick_name()));
				bool matchesUserName = neolib::wildcard_match(irc::make_string(aCasemapping, aUser.user_name()), irc::make_string(aCasemapping, theEntry.user().user_name()));
				bool matchesHostName = neolib::wildcard_match(irc::make_string(aCasemapping, aUser.host_name()), irc::make_string(aCasemapping, theEntry.user().host_name()));
				if (matchesNickName && !anyNickName)
					aResult.push_back(i);
				else if (matchesUserName && matchesHostName && (!anyUserName || !anyHostName))
					aResult.push_back(i);
				else if (anyNickName && anyUserName && anyHostName)
					aResult.push_back(i);
			}
			if (!aResult.empty())
				return aResult.front();
			else
				return aList.entries().end();
		}
	}

	ignore_list::container_type::const_iterator ignore_list::find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const ignore_list, container_type::const_iterator>(aCasemapping, *this, aServer, aUser, result);
	}

	ignore_list::container_type::iterator ignore_list::find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser)
	{
		std::vector<container_type::iterator> result;
		return find_impl<ignore_list, container_type::iterator>(aCasemapping, *this, aServer, aUser, result);
	}

	ignore_list::container_type::const_iterator ignore_list::find(casemapping::type aCasemapping, const ignore_entry& aEntry) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const ignore_list, container_type::const_iterator>(aCasemapping, *this, aEntry.server(), aEntry.user(), result);
	}

	ignore_list::container_type::iterator ignore_list::find(casemapping::type aCasemapping, const ignore_entry& aEntry)
	{
		std::vector<container_type::iterator> result;
		return find_impl<ignore_list, container_type::iterator>(aCasemapping, *this, aEntry.server(), aEntry.user(), result);
	}

	bool ignore_list::ignored(casemapping::type aCasemapping, const server_key& aServer, const user& aUser) const
	{
		return find(aCasemapping, aServer, aUser) != iEntries.end();
	}

	bool ignore_list::add(const server_key& aServer, const user& aUser)
	{
		if (ignored(casemapping::ascii, aServer, aUser))
		{
			ignore_entry& existingEntry = *find(casemapping::ascii, aServer, aUser);
			if (existingEntry.user() != aUser ||
				existingEntry.user().user_name() != aUser.user_name() ||
				existingEntry.user().host_name() != aUser.host_name())
			{
				existingEntry.user() = aUser;
				if (!iLoading)
					notify_observers(ignore_list_observer::NotifyUpdated, existingEntry);
			}
			return false;
		}

		iEntries.push_back(ignore_entry(aServer, aUser));
		if (!iLoading)
			notify_observers(ignore_list_observer::NotifyAdded, iEntries.back());

		return true;
	}

	bool ignore_list::add(const ignore_entry& aEntry)
	{
		return add(aEntry.server(), aEntry.user());
	}

	void ignore_list::update(ignore_entry& aExistingEntry, const ignore_entry& aNewEntry)
	{
		aExistingEntry = aNewEntry;
		if (!iLoading)
			notify_observers(ignore_list_observer::NotifyUpdated, aExistingEntry);
	}

	void ignore_list::update_user(casemapping::type aCasemapping, const server_key& aServer, const user& aOldUser, const user& aNewUser)
	{
		std::vector<container_type::iterator> result;
		find_impl<ignore_list, container_type::iterator>(aCasemapping, *this, aServer, aOldUser, result);
		for (std::vector<container_type::iterator>::iterator i = result.begin(); i != result.end(); ++i)
		{
			ignore_entry& existingEntry = **i;
			if (existingEntry.user().nick_name().empty() || existingEntry.user().nick_name() == "*")
				continue;
			if (!existingEntry.user().has_user_name() && !existingEntry.user().has_host_name())
			{
				ignore_entry newEntry(existingEntry);
				newEntry.user().host_name() = aNewUser.host_name();
				newEntry.user().user_name() = aNewUser.user_name();
				update(existingEntry, newEntry);
			}
		}
	}

	void ignore_list::remove(const server_key& aServer, const user& aUser)
	{
		container_type::iterator item = find(casemapping::ascii, aServer, aUser);
		if (item == iEntries.end())
			return;

		container_type temp;
		temp.splice(temp.begin(), iEntries, item);
		notify_observers(ignore_list_observer::NotifyRemoved, *temp.begin());
	}

	void ignore_list::remove(const ignore_entry& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (&*i == &aEntry)
			{
				container_type temp;
				temp.splice(temp.begin(), iEntries, i);
				notify_observers(ignore_list_observer::NotifyRemoved, *temp.begin());
				break;
			}
	}

	void ignore_list::notify_observer(ignore_list_observer& aObserver, ignore_list_observer::notify_type aType, const void* aParameter, const void*)
	{
		switch(aType)
		{
		case ignore_list_observer::NotifyAdded:
			aObserver.ignore_added(*static_cast<const ignore_entry*>(aParameter));
			break;
		case ignore_list_observer::NotifyUpdated:
			aObserver.ignore_updated(*static_cast<const ignore_entry*>(aParameter));
			break;
		case ignore_list_observer::NotifyRemoved:
			aObserver.ignore_removed(*static_cast<const ignore_entry*>(aParameter));
			break;
		}
	}

	void read_ignore_list(model& aModel, std::function<bool()> aErrorFunction)
	{
		ignore_list& theIgnoreList = aModel.ignore_list();

		neolib::xml xmlIgnoreList(true);
		std::ifstream input((aModel.root_path() + "ignore_list.xml").c_str());
		xmlIgnoreList.read(input);

		if (xmlIgnoreList.error() && aErrorFunction && aErrorFunction())
		{
			theIgnoreList.entries().clear();
			write_ignore_list(aModel);
			return;
		}

		theIgnoreList.loading(true);

		for (neolib::xml::element::const_iterator i = xmlIgnoreList.root().begin(); i != xmlIgnoreList.root().end(); ++i)
		{
			if (i->name() == "entry")
			{
				ignore_entry e;
				for (neolib::xml::element::const_iterator j = i->begin(); j != i->end(); ++j)
				{
					if (j->name() == "user")
						e.user() = user(j->attribute_value("value"), casemapping::rfc1459, false, true);
					if (j->name() == "server_network" || j->name() == "server_group")
						e.server().first = j->attribute_value("value");
					if (j->name() == "server_name")
						e.server().second = j->attribute_value("value");
				}
				theIgnoreList.entries().push_back(e);
			}
		}

		theIgnoreList.loading(false);
	}

	void write_ignore_list(const model& aModel)
	{
		const ignore_list& theIgnoreList = aModel.ignore_list();

		neolib::xml xmlIgnoreList(true);

		xmlIgnoreList.root().name() = "ignore_list";

		for (ignore_list::container_type::const_iterator i = theIgnoreList.entries().begin(); i != theIgnoreList.entries().end(); ++i)
		{
			const ignore_entry& entry = *i;
			neolib::xml::element& xmlEntry = xmlIgnoreList.append(xmlIgnoreList.root(), "entry");
			xmlIgnoreList.append(xmlEntry, "user").set_attribute("value", entry.user().msgto_form());
			xmlIgnoreList.append(xmlEntry, "server_network").set_attribute("value", entry.server().first);
			xmlIgnoreList.append(xmlEntry, "server_name").set_attribute("value", entry.server().second);
		}

		std::ofstream output((aModel.root_path() + "ignore_list.xml").c_str());
		xmlIgnoreList.write(output);
	}
}