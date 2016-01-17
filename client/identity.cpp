// identity.cpp
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
#include "identity.hpp"
#include "model.hpp"

namespace irc
{
	namespace
	{
		std::string scramble_password(const std::string& aPassword, const std::string& aKey)
		{
			std::string ret;
			std::string::size_type keyPos = 0;
			for (std::string::const_iterator i = aPassword.begin(); i != aPassword.end(); ++i)
			{
				char ch = *i ^ aKey[keyPos];
				ret += neolib::integer_to_string<char>(ch, 16, 2);
				keyPos = (keyPos + 1) % aKey.size();
			}
			return ret;
		}
		std::string unscramble_password(const std::string& aPassword, const std::string& aKey)
		{
			std::string ret;
			std::string::size_type keyPos = 0;
			for (std::string::const_iterator i = aPassword.begin(); i != aPassword.end();)
			{
				std::string hex;
				hex += *i++;
				if (i != aPassword.end())
					hex += *i++;
				ret += static_cast<char>((neolib::string_to_integer(hex, 16) ^ aKey[keyPos]));
				keyPos = (keyPos + 1) % aKey.size();
			}
			return ret;
		}
	}

	identity& identities::add_item(const identity& aItem)
	{
		iIdentities.push_back(aItem);
		identity& ret = iIdentities.back();
		iIdentities.sort();
		notify_observers(identities_observer::NotifyAdded, ret);
		return ret;
	}

	void identities::update_item(identity& aExistingItem, const identity& aUpdatedItem)
	{
		if (&aExistingItem != &aUpdatedItem)
		{
			identity oldItem(aExistingItem);
			aExistingItem = aUpdatedItem;
			iIdentities.sort();
			notify_observers(identities_observer::NotifyUpdated, aExistingItem, oldItem);
		}
		else
		{
			iIdentities.sort();
			notify_observers(identities_observer::NotifyUpdated, aExistingItem, aExistingItem);
		}
	}

	void identities::delete_item(identity& aItem)
	{
		for (identity_list::iterator i = iIdentities.begin(); i != iIdentities.end(); ++i)
			if (&*i == &aItem)
			{
				container_type temp;
				temp.splice(temp.begin(), iIdentities, i);
				notify_observers(identities_observer::NotifyRemoved, *temp.begin());
				break;
			}
	}

	identity_list::iterator identities::find_item(const std::string& aNickName)
	{
		for (identity_list::iterator i = iIdentities.begin(); i != iIdentities.end(); ++i)
		if (i->nick_name() == aNickName)
			return i;
		return iIdentities.end();
	}

	identity_list::iterator identities::find_item(const identity& aIdentity)
	{
		return find_item(aIdentity.nick_name());
	}

	void identities::notify_observer(identities_observer& aObserver, identities_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch(aType)
		{
		case identities_observer::NotifyAdded:
			aObserver.identity_added(*static_cast<const identity*>(aParameter));
			break;
		case identities_observer::NotifyUpdated:
			aObserver.identity_updated(*static_cast<const identity*>(aParameter), *static_cast<const identity*>(aParameter2));
			break;
		case identities_observer::NotifyRemoved:
			aObserver.identity_removed(*static_cast<const identity*>(aParameter));
			break;
		}
	}

	void identities::read(const model& aModel, std::string& aDefaultIdentity, std::function<bool()> aErrorFunction)
	{
		iIdentities.clear();

		neolib::xml xmlIdentities(true);
		
		std::ifstream input((aModel.root_path() + "identities.xml").c_str());
		xmlIdentities.read(input);

		if (xmlIdentities.error() && aErrorFunction && aErrorFunction())
		{
			iIdentities.clear();
			write(aModel, aDefaultIdentity);
			return;
		}

		for (neolib::xml::element::const_iterator i = xmlIdentities.root().begin(); i != xmlIdentities.root().end(); ++i)
		{
			const neolib::xml::element& idElement = *i;
			if (idElement.name() == "identity")
			{
				identity id;
				for (neolib::xml::element::const_iterator j = idElement.begin(); j != idElement.end(); ++j)
				{
					const neolib::xml::element& element = *j;
					if (element.name() == "nick")
						id.set_nick_name(element.attribute_value("value"));
					else if (element.name() == "name")
						id.set_full_name(element.attribute_value("value"));
					else if (element.name() == "email")
						id.set_email_address(element.attribute_value("value"));
					else if (element.name() == "invisible")
						id.set_invisible(element.attribute_value("value") == "1" ? true : false);
					else if (element.name() == "last_server_used")
						id.set_last_server_used(std::make_pair(element.attribute_value("network", "group"), element.attribute_value("name")));
					else if (element.name() == "alternate_nick")
						id.alternate_nick_names().push_back(element.attribute_value("value"));
					else if (element.name() == "password")
					{
						server_key theServer;
						theServer.first = element.attribute_value("server_network");
						theServer.second = element.attribute_value("server_name");
						id.passwords()[theServer] = unscramble_password(element.attribute_value("password"), id.nick_name());
					}
				}
				iIdentities.push_back(id);
			}
			else if (i->name() == "default")
			{
				aDefaultIdentity = i->attribute_value("value");
			}
		}

		iIdentities.sort();
	}

	void identities::write(const model& aModel, const std::string& aDefaultIdentity) const
	{
		neolib::xml xmlIdentities(true);

		xmlIdentities.root().name() = "identities";

		for (identity_list::const_iterator i = iIdentities.begin(); i != iIdentities.end(); ++i)
		{
			const identity& id = *i;
			neolib::xml::element& xmlId = xmlIdentities.append(xmlIdentities.root(), "identity");
			xmlIdentities.append(xmlId, "nick").set_attribute("value", id.nick_name());
			xmlIdentities.append(xmlId, "name").set_attribute("value", id.full_name());
			xmlIdentities.append(xmlId, "email").set_attribute("value", id.email_address());
			xmlIdentities.append(xmlId, "invisible").set_attribute("value", id.invisible() ? std::string("1") : std::string("0"));
			neolib::xml::element& lastServerUsed = xmlIdentities.append(xmlId, "last_server_used");
			lastServerUsed.set_attribute("network", id.last_server_used().first);
			lastServerUsed.set_attribute("name", id.last_server_used().second);
			for (identity::alternate_nick_names_t::const_iterator j = id.alternate_nick_names().begin(); j != id.alternate_nick_names().end(); ++j)
				xmlIdentities.append(xmlId, "alternate_nick").set_attribute("value", *j);
			for (identity::passwords_t::const_iterator j = id.passwords().begin(); j != id.passwords().end(); ++j)
			{
				neolib::xml::element& password = xmlIdentities.append(xmlId, "password");
				password.set_attribute("server_network", j->first.first);
				password.set_attribute("server_name", j->first.second);
				password.set_attribute("password", scramble_password(j->second, id.nick_name()));
			}
		}

		xmlIdentities.append(xmlIdentities.root(), "default").set_attribute("value", aModel.default_identity().nick_name());
		
		std::ofstream output((aModel.root_path() + "identities.xml").c_str());
		xmlIdentities.write(output);
	}

	void read_identity_list(model& aModel, std::string& aDefaultIdentity, std::function<bool()> aErrorFunction)
	{
		aModel.identities().read(aModel, aDefaultIdentity, aErrorFunction);
	}

	void write_identity_list(const model& aModel, const std::string& aDefaultIdentity)
	{
		aModel.identities().write(aModel, aDefaultIdentity);
	}
}