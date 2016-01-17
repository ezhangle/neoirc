// auto_joins.cpp
/*
 *  Copyright (c) 2010 - 2014 Leigh Johnston.
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
#include "auto_joins.hpp"

namespace irc
{
	namespace
	{
		template <typename List, typename Iter, typename Result>
		Iter find_impl(casemapping::type aCasemapping, List& aList, const server_key& aServer, const std::string& aNickName, Result& aResult)
		{
			for (Iter i = aList.entries().begin(); i != aList.entries().end(); ++i)
			{
				const auto_join& theEntry = *i;
				if (aList.matches(aCasemapping, theEntry, aServer, aNickName))
					aResult.push_back(i);
			}
			if (!aResult.empty())
				return aResult.front();
			else
				return aList.entries().end();
		}
	}

	auto_joins::auto_joins(identities& aIdentities) : iIdentities(aIdentities), iLoading(false)
	{
		iIdentities.add_observer(*this);
	}

	auto_joins::~auto_joins()
	{
		iIdentities.remove_observer(*this);
	}

	auto_joins::container_type::const_iterator auto_joins::find(casemapping::type aCasemapping, const server_key& aServer, const std::string& aNickName) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const auto_joins, container_type::const_iterator>(aCasemapping, *this, aServer, aNickName, result);
	}

	auto_joins::container_type::iterator auto_joins::find(casemapping::type aCasemapping, const server_key& aServer, const std::string& aNickName)
	{
		std::vector<container_type::iterator> result;
		return find_impl<auto_joins, container_type::iterator>(aCasemapping, *this, aServer, aNickName, result);
	}

	auto_joins::container_type::const_iterator auto_joins::find(casemapping::type aCasemapping, const auto_join& aEntry) const
	{
		std::vector<container_type::const_iterator> result;
		return find_impl<const auto_joins, container_type::const_iterator>(aCasemapping, *this, aEntry.server(), aEntry.nick_name(), result);
	}

	auto_joins::container_type::iterator auto_joins::find(casemapping::type aCasemapping, const auto_join& aEntry)
	{
		std::vector<container_type::iterator> result;
		return find_impl<auto_joins, container_type::iterator>(aCasemapping, *this, aEntry.server(), aEntry.nick_name(), result);
	}

	bool auto_joins::matches(casemapping::type aCasemapping, const auto_join& aEntry, const server_key& aServer, const std::string& aNickName) const
	{
		if (aEntry.server().first != aServer.first ||
			(aEntry.server().second != aServer.second && aEntry.server().second != "*"))
			return false;
		if (irc::make_string(aCasemapping, aEntry.nick_name()) != aNickName)
			return false;
		return true;
	}

	bool auto_joins::has_auto_join(const server_key& aServer, const std::string& aNickName) const
	{
		return find(casemapping::ascii, aServer, aNickName) != iEntries.end();
	}

	bool auto_joins::has_auto_join(const server_key& aServer, const std::string& aNickName, const std::string& aChannel) const
	{
		std::vector<container_type::const_iterator> result;
		find_impl<const auto_joins, container_type::const_iterator>(casemapping::ascii, *this, aServer, aNickName, result);
		for (std::vector<container_type::const_iterator>::const_iterator i = result.begin(); i != result.end(); ++i)
			if ((**i).channel() == aChannel)
				return true;
		return false;
	}

	bool auto_joins::add(const server_key& aServer, const std::string& aNickName, const std::string& aChannel)
	{
		if (has_auto_join(aServer, aNickName, aChannel))
			return false;

		iEntries.push_back(auto_join(aServer, aNickName, aChannel));
		if (!iLoading)
			notify_observers(auto_joins_observer::NotifyAdded, iEntries.back());

		return true;
	}

	bool auto_joins::add(const auto_join& aEntry)
	{
		return add(aEntry.server(), aEntry.nick_name(), aEntry.channel());
	}

	void auto_joins::update(auto_join& aExistingEntry, const auto_join& aNewEntry)
	{
		aExistingEntry = aNewEntry;
		if (!iLoading)
			notify_observers(auto_joins_observer::NotifyUpdated, aExistingEntry);
	}

	void auto_joins::remove(const server_key& aServer, const std::string& aNickName, const std::string& aChannel)
	{
		std::vector<container_type::iterator> result;
		find_impl<auto_joins, container_type::iterator>(casemapping::ascii, *this, aServer, aNickName, result);
		for (std::vector<container_type::iterator>::iterator i = result.begin(); i != result.end(); ++i)
			if ((**i).channel() == aChannel)
			{
				container_type temp;
				temp.splice(temp.begin(), iEntries, *i);
				notify_observers(auto_joins_observer::NotifyRemoved, *temp.begin());
				break;
			}
	}

	void auto_joins::remove(const auto_join& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (&*i == &aEntry)
			{
				container_type temp;
				temp.splice(temp.begin(), iEntries, i);
				notify_observers(auto_joins_observer::NotifyRemoved, *temp.begin());
				break;
			}
	}

	void auto_joins::clear()
	{
		iEntries.clear();
		notify_observers(auto_joins_observer::NotifyCleared);
	}

	void auto_joins::reset(const container_type& aEntries)
	{
		iEntries = aEntries;
		notify_observers(auto_joins_observer::NotifyReset);
	}

	void auto_joins::notify_observer(auto_joins_observer& aObserver, auto_joins_observer::notify_type aType, const void* aParameter, const void*)
	{
		switch(aType)
		{
		case auto_joins_observer::NotifyAdded:
			aObserver.auto_join_added(*static_cast<const auto_join*>(aParameter));
			break;
		case auto_joins_observer::NotifyUpdated:
			aObserver.auto_join_updated(*static_cast<const auto_join*>(aParameter));
			break;
		case auto_joins_observer::NotifyRemoved:
			aObserver.auto_join_removed(*static_cast<const auto_join*>(aParameter));
			break;
		case auto_joins_observer::NotifyCleared:
			aObserver.auto_join_cleared();
			break;
		case auto_joins_observer::NotifyReset:
			aObserver.auto_join_reset();
			break;
		}
	}

	void auto_joins::identity_added(const identity& aEntry)
	{
	}

	void auto_joins::identity_updated(const identity& aEntry, const identity& aOldEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end(); ++i)
			if (i->nick_name() == aOldEntry.nick_name() &&
				i->nick_name() != aEntry.nick_name())
			{
				auto_join newEntry(*i);
				newEntry.set_nick_name(aEntry.nick_name());
				update(*i, newEntry);
			}
	}

	void auto_joins::identity_removed(const identity& aEntry)
	{
		for (container_type::iterator i = iEntries.begin(); i != iEntries.end();)
			if (i->nick_name() == aEntry.nick_name())
				remove(*i++);
			else
				++i;
	}

	void read_auto_joins(model& aModel, std::function<bool()> aErrorFunction)
	{
		auto_joins& theAutoJoinList = aModel.auto_joins();

		neolib::xml xmlAutoJoinList(true);
		std::ifstream input((aModel.root_path() + "auto_joins.xml").c_str());
		xmlAutoJoinList.read(input);

		if (xmlAutoJoinList.error() && aErrorFunction && aErrorFunction())
		{
			theAutoJoinList.entries().clear();
			write_auto_joins(aModel);
			return;
		}

		theAutoJoinList.loading(true);

		for (neolib::xml::element::const_iterator i = xmlAutoJoinList.root().begin(); i != xmlAutoJoinList.root().end(); ++i)
		{
			if (i->name() == "entry")
			{
				theAutoJoinList.entries().push_back(auto_join());
				auto_join& e = theAutoJoinList.entries().back();
				for (neolib::xml::element::const_iterator j = i->begin(); j != i->end(); ++j)
				{
					if (j->name() == "server_network" || j->name() == "server_group")
						e.set_server(server_key(j->attribute_value("value"), e.server().second));
					if (j->name() == "server_name")
						e.set_server(server_key(e.server().first, j->attribute_value("value")));
					if (j->name() == "nick_name")
						e.set_nick_name(j->attribute_value("value"));
					if (j->name() == "channel")
						e.set_channel(j->attribute_value("value"));
				}
			}
		}

		theAutoJoinList.loading(false);
	}

	void write_auto_joins(const model& aModel)
	{
		const auto_joins& theAutoJoinList = aModel.auto_joins();

		neolib::xml xmlAutoJoinList(true);

		xmlAutoJoinList.root().name() = "auto_joins";

		for (auto_joins::container_type::const_iterator i = theAutoJoinList.entries().begin(); i != theAutoJoinList.entries().end(); ++i)
		{
			const auto_join& entry = *i;
			neolib::xml::element& xmlEntry = xmlAutoJoinList.append(xmlAutoJoinList.root(), "entry");
			xmlAutoJoinList.append(xmlEntry, "server_network").set_attribute("value", entry.server().first);
			xmlAutoJoinList.append(xmlEntry, "server_name").set_attribute("value", entry.server().second);
			xmlAutoJoinList.append(xmlEntry, "nick_name").set_attribute("value", entry.nick_name());
			xmlAutoJoinList.append(xmlEntry, "channel").set_attribute("value", entry.channel());
		}

		std::ofstream output((aModel.root_path() + "auto_joins.xml").c_str());
		xmlAutoJoinList.write(output);
	}
}