// contacts.cpp
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
#include <tuple>
#include <neolib/xml.hpp>
#include <neoirc/client/model.hpp>
#include <neoirc/client/contacts.hpp>

namespace irc
{
	namespace
	{
		template <typename List, typename Iter, typename Result>
		Iter find_impl(casemapping::type aCasemapping, List& theList, const server_key& aServer, const user& aUser, Result& aResult)
		{
			for (Iter i = theList.entries().begin(); i != theList.entries().end(); ++i)
			{
				const contact& theEntry = *i;
				if ((theEntry.server().first != aServer.first && theEntry.server().first != "*") ||
					(theEntry.server().second != aServer.second && theEntry.server().second != "*"))
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
			{
				struct match_value : std::tr1::tuple<int, int, int, int>
				{
					match_value(const std::string& aString, const std::string& aExactMatch = "")
					{
						std::tr1::get<0>(*this) = (aString == aExactMatch ? 0 : 1);
						std::tr1::get<2>(*this) = std::count(aString.begin(), aString.end(), '?');
						std::tr1::get<3>(*this) = std::count(aString.begin(), aString.end(), '*');
						std::tr1::get<1>(*this) = -(static_cast<int>(aString.size()) - (std::tr1::get<2>(*this) + std::tr1::get<3>(*this)));
					}
				};
				typedef std::tr1::tuple<match_value, match_value, match_value> match_values;
				std::multimap<match_values, Iter> sortedMatches;
				for (typename Result::const_iterator i = aResult.begin(); i != aResult.end(); ++i)
					sortedMatches.insert(std::make_pair(match_values(match_value((*i)->user().nick_name(), aUser.nick_name()), match_value((*i)->user().user_name(), aUser.user_name()), match_value((*i)->user().host_name(), aUser.host_name())), *i));
				return sortedMatches.begin()->second;
			}
			else
				return theList.entries().end();
		}
	}

	contacts::contacts(model& aModel, connection_manager& aConnectionManager)
	: iModel(aModel), iConnectionManager(aConnectionManager), iLoading(false)
	{
		iConnectionManager.add_observer(*this);
	}

	contacts::~contacts()
	{
		for (watched_connections::iterator i = iWatchedConnections.begin(); i != iWatchedConnections.end(); ++i)
			(*i)->remove_observer(*this);
		iConnectionManager.remove_observer(*this);
	}

	contacts::container_type::const_iterator contacts::find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const contacts, container_type::const_iterator>(aCasemapping, *this, aServer, aUser, result);
	}

	contacts::container_type::iterator contacts::find(casemapping::type aCasemapping, const server_key& aServer, const user& aUser)
	{
		std::vector<container_type::iterator> result;
		return find_impl<contacts, container_type::iterator>(aCasemapping, *this, aServer, aUser, result);
	}

	contacts::container_type::const_iterator contacts::find(casemapping::type aCasemapping, const contact& aEntry) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const contacts, container_type::const_iterator>(aCasemapping, *this, aEntry.server(), aEntry.user(), result);
	}

	contacts::container_type::iterator contacts::find(casemapping::type aCasemapping, const contact& aEntry)
	{
		std::vector<container_type::iterator> result;
		return find_impl<contacts, container_type::iterator>(aCasemapping, *this, aEntry.server(), aEntry.user(), result);
	}

	bool contacts::contains(casemapping::type aCasemapping, const server_key& aServer, const user& aUser) const
	{
		return find(aCasemapping, aServer, aUser) != iEntries.end();
	}

	bool contacts::add(const contact& aEntry)
	{
		iEntries.push_back(aEntry);
		if (!iLoading)
			notify_observers(contacts_observer::NotifyAdded, iEntries.back());
		return true;
	}

	void contacts::update(contact& aExistingEntry, const contact& aNewEntry)
	{
		contact oldEntry = aExistingEntry;
		aExistingEntry = aNewEntry;
		if (!iLoading)
			notify_observers(contacts_observer::NotifyUpdated, aExistingEntry, oldEntry);
	}

	void contacts::update_user(casemapping::type aCasemapping, const server_key& aServer, const user& aOldUser, const user& aNewUser)
	{
		std::vector<container_type::iterator> result;
		find_impl<contacts, container_type::iterator>(aCasemapping, *this, aServer, aOldUser, result);
		for (std::vector<container_type::iterator>::iterator i = result.begin(); i != result.end(); ++i)
		{
			contact& existingEntry = **i;
			if (existingEntry.user().nick_name().empty() || existingEntry.user().nick_name() == "*")
				continue;
			if (!existingEntry.user().has_user_name() && !existingEntry.user().has_host_name())
			{
				contact newEntry(existingEntry);
				user updatedUser = newEntry.user();
				updatedUser.host_name() = aNewUser.host_name();
				updatedUser.user_name() = aNewUser.user_name();
				newEntry.set_user(updatedUser);
				update(existingEntry, newEntry);
			}
		}
	}

	void contacts::remove(const contact& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (&*i == &aEntry)
			{
				container_type temp;
				temp.splice(temp.begin(), iEntries, i);
				notify_observers(contacts_observer::NotifyRemoved, *temp.begin());
				break;
			}
	}

	void contacts::notify_observer(contacts_observer& aObserver, contacts_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case contacts_observer::NotifyAdded:
			aObserver.contact_added(*static_cast<const contact*>(aParameter));
			break;
		case contacts_observer::NotifyUpdated:
			aObserver.contact_updated(*static_cast<const contact*>(aParameter), *static_cast<const contact*>(aParameter2));
			break;
		case contacts_observer::NotifyRemoved:
			aObserver.contact_removed(*static_cast<const contact*>(aParameter));
			break;
		}
	}

	void contacts::connection_added(connection& aConnection)
	{
		aConnection.add_observer(*this);
		iWatchedConnections.push_back(&aConnection);
	}

	void contacts::connection_removed(connection& aConnection)
	{
		iWatchedConnections.remove(&aConnection);
	}

	void contacts::incoming_message(connection& aConnection, const message& aMessage)
	{
		if (aMessage.command() == message::NICK)
		{
			user oldUser(aMessage.origin(), aConnection);
			user newUser(oldUser);
			if (!aMessage.parameters().empty())
				newUser.nick_name() = aMessage.parameters()[0];
			update_user(aConnection, aConnection.server().key(), oldUser, newUser);
		}
	}

	void contacts::filter_message(connection& aConnection, const message& aMessage, bool& aFiltered)
	{
		buffer& msgBuffer = aConnection.connection_manager().active_buffer() != 0 ? 
			*aConnection.connection_manager().active_buffer() : aConnection.server_buffer();
		if (neolib::make_ci_string(aMessage.command_string()) == "contact" ||
			neolib::make_ci_string(aMessage.command_string()) == "group" || 
			neolib::make_ci_string(aMessage.command_string()) == "buddy")
		{
			if (aMessage.parameters().size() == 2)
			{
				user theUser(aMessage.parameters()[0]);
				if (aConnection.has_user(theUser))
					theUser = aConnection.user(theUser);
				if (contains(aConnection, aConnection.server(), theUser))
					msgBuffer.new_message("/echo User already in contacts.");
				else
				{
					add(contact("", aMessage.parameters()[1], any_server(aConnection.server()), theUser));
					msgBuffer.new_message("/echo User added to contacts.");
				}
			}
			else
				msgBuffer.new_message("/echo Usage: /contact <nickname> <group>");
			aFiltered = true;
		}
	}

	void read_contacts_list(model& aModel, std::function<bool()> aErrorFunction)
	{
		contacts& theList = aModel.contacts();

		neolib::xml xmlGroupList(true);
		std::ifstream input((aModel.root_path() + "contacts.xml").c_str());
		if (input)
			xmlGroupList.read(input);
		else
		{
			std::ifstream input((aModel.root_path() + "group_list.xml").c_str());
			xmlGroupList.read(input);
		}

		if (xmlGroupList.error() && aErrorFunction && aErrorFunction())
		{
			theList.entries().clear();
			write_contacts_list(aModel);
			return;
		}

		theList.loading(true);

		for (neolib::xml::element::const_iterator i = xmlGroupList.root().begin(); i != xmlGroupList.root().end(); ++i)
		{
			if (i->name() == "entry")
			{
				contact e;
				server_key sk;
				for (neolib::xml::element::const_iterator j = i->begin(); j != i->end(); ++j)
				{
					if (j->name() == "name")
						e.set_name(j->attribute_value("value"));
					if (j->name() == "group")
						e.set_group(j->attribute_value("value"));
					if (j->name() == "user")
						e.set_user(user(j->attribute_value("value"), casemapping::rfc1459, false, true));
					if (j->name() == "server_network" || j->name() == "server_group")
						sk.first = j->attribute_value("value");
					if (j->name() == "server_name")
						sk.second = j->attribute_value("value");
				}
				e.set_server(sk);
				theList.entries().push_back(e);
			}
		}

		theList.loading(false);
	}

	void write_contacts_list(const model& aModel)
	{
		const contacts& theList = aModel.contacts();

		neolib::xml xmlGroupList(true);

		xmlGroupList.root().name() = "contacts_list";

		for (contacts::container_type::const_iterator i = theList.entries().begin(); i != theList.entries().end(); ++i)
		{
			const contact& entry = *i;
			neolib::xml::element& xmlEntry = xmlGroupList.append(xmlGroupList.root(), "entry");
			xmlGroupList.append(xmlEntry, "name").set_attribute("value", entry.name());
			xmlGroupList.append(xmlEntry, "group").set_attribute("value", entry.group());
			xmlGroupList.append(xmlEntry, "user").set_attribute("value", entry.user().msgto_form());
			xmlGroupList.append(xmlEntry, "server_network").set_attribute("value", entry.server().first);
			xmlGroupList.append(xmlEntry, "server_name").set_attribute("value", entry.server().second);
		}

		std::ofstream output((aModel.root_path() + "contacts.xml").c_str());
		xmlGroupList.write(output);
		remove((aModel.root_path() + "group_list.xml").c_str());
	}
}