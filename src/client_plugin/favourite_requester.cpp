// favourite_requester.cpp
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
#include <boost/lexical_cast.hpp>
#include "favourite_requester.hpp"

namespace caw_irc_plugin
{
	favourite_requester::favourite_requester(neolib::random& aRandom, irc::connection_manager& aConnectionManager, irc::identities& aIdentities, irc::server_list& aServerList, caw::i_favourites& aFavorites) :
		iRandom(aRandom), iConnectionManager(aConnectionManager), iIdentities(aIdentities), iServerList(aServerList), iFavourites(aFavorites)
	{
		iConnectionManager.add_observer(*this);
		iIdentities.add_observer(*this);
	}

	favourite_requester::~favourite_requester()
	{
		for (watched_connections::iterator i = iWatchedConnections.begin(); i != iWatchedConnections.end(); ++i)
			(*i)->remove_observer(*this);
		iConnectionManager.remove_observer(*this);
		iIdentities.remove_observer(*this);
	}

	void favourite_requester::add_request(const caw::i_favourite_item& aRequest)
	{
		if (iRequests.find(aRequest) != iRequests.end())
			return;
		for (irc::connection_manager::connection_list::iterator i = iConnectionManager.connections().begin(); i != iConnectionManager.connections().end(); ++i)
		{
			irc::connection& theConnection = **i;
			if (!theConnection.quitting() && theConnection.identity().nick_name() == aRequest.property("nick_name") &&
				theConnection.server().key().first == aRequest.property("server_network") &&
				(theConnection.server().key().second == aRequest.property("server_name") ||
				aRequest.property("server_name") == "*" || iConnectionManager.reconnect_any_server()))
			{
				if (theConnection.quitting())
					continue;
				else if (aRequest.name().empty())
					return;
				else if (theConnection.registered())
				{
					std::string name = aRequest.property("name").to_std_string();
					irc::buffer* existingBuffer = theConnection.find_buffer(name);
					if (existingBuffer == 0)
					{
						if (static_cast<favourite_type_e>(boost::lexical_cast<int>(aRequest.property("type"))) == Channel)
						{
							irc::message joinMessage(theConnection, irc::message::OUTGOING);
							joinMessage.set_command(irc::message::JOIN);
							joinMessage.parse_parameters("JOIN " + name);
							theConnection.send_message(joinMessage);
						}
						else
						{
							theConnection.create_buffer(name).activate();
						}
					}
					else
						existingBuffer->activate();
				}
				else if (!theConnection.connected())
				{
					if (theConnection.connect())
						add_request(theConnection, aRequest);
				}
				else
					add_request(theConnection, aRequest);
				return;
			}
		}
		irc::connection* theConnection = 0;
		for (irc::identity_list::const_iterator i = iIdentities.identity_list().begin(); theConnection == 0 && i != iIdentities.identity_list().end(); ++i)
			if (i->nick_name() == aRequest.property("nick_name"))
				if (aRequest.property("server_name") != "*")
				{
					for (irc::server_list::const_iterator s = iServerList.begin(); theConnection == 0 && s != iServerList.end(); ++s)
						if (s->key() == irc::server::key_type(aRequest.property("server_network").to_std_string(), aRequest.property("server_name").to_std_string()))
						{
							irc::server theServer = *s;
							if (aRequest.property_exists("server_port"))
							{
								irc::server::port_list ports;
								ports.push_back(irc::server::port_range(
									boost::lexical_cast<uint16_t>(aRequest.property("server_port")),
									boost::lexical_cast<uint16_t>(aRequest.property("server_port"))));
								theServer.set_ports(ports);
							}
							if (aRequest.property_exists("server_secure"))
								theServer.set_secure(boost::lexical_cast<bool>(aRequest.property("server_secure")));
							theConnection = iConnectionManager.add_connection(theServer, *i, std::string(), true);
						}
				}
				else
				{
					int number = 0;
					for (irc::server_list::const_iterator s = iServerList.begin(); theConnection == 0 && s != iServerList.end(); ++s)
						if (s->key().first == aRequest.property("server_network"))
							++number;
					number = iRandom.get(0, number - 1);
					for (irc::server_list::const_iterator s = iServerList.begin(); theConnection == 0 && s != iServerList.end(); ++s)
						if (s->key().first == aRequest.property("server_network") && --number < 0)
						{
							irc::server theServer = *s;
							if (aRequest.property_exists("server_port"))
							{
								irc::server::port_list ports;
								ports.push_back(irc::server::port_range(
									boost::lexical_cast<uint16_t>(aRequest.property("server_port")), 
									boost::lexical_cast<uint16_t>(aRequest.property("server_port"))));
								theServer.set_ports(ports);
							}
							if (aRequest.property_exists("server_secure"))
								theServer.set_secure(boost::lexical_cast<bool>(aRequest.property("server_secure")));
							theConnection = iConnectionManager.add_connection(theServer, *i, std::string(), true);
						}
				}

		if (theConnection != 0)
			add_request(*theConnection, aRequest);
	}

	void favourite_requester::add_request(irc::connection& aConnection, const caw::i_favourite_item& aRequest)
	{
		if (aRequest.name().empty())
			return;
		iRequests.insert(request_list::value_type(aRequest, &aConnection));
	}

	void favourite_requester::remove_requests(irc::connection& aConnection)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			if (i->second == &aConnection)
				iRequests.erase(i++);
			else
				++i;
		}
	}

	void favourite_requester::identity_added(const irc::identity& aEntry)
	{
	}

	void favourite_requester::identity_updated(const irc::identity& aEntry, const irc::identity& aOldEntry)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			const caw::i_favourite_item& existingItem = to_item(i->first);
			if (existingItem.property("nick_name") == aOldEntry.nick_name() &&
				existingItem.property("nick_name") != aEntry.nick_name())
			{
				caw::favourite_item theRequest = existingItem;
				theRequest.property("nick_name") = aEntry.nick_name();
				if (i->first.is<caw::i_favourite_folder::iterator>())
				{
					caw::i_favourite_folder::iterator iterExisting = static_variant_cast<const caw::i_favourite_folder::iterator&>(i->first);
					iFavourites.update_favourite(iterExisting, theRequest);
				}
				irc::connection* theConnection = i->second;
				iRequests.erase(i);
				add_request(*theConnection, theRequest);
				i = iRequests.begin();
			}
			else
				++i;
		}
	}

	void favourite_requester::identity_removed(const irc::identity& aEntry)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			const caw::i_favourite_item& existingItem = to_item(i->first);
			if (existingItem.property("nick_name") == aEntry.nick_name())
			{
				if (i->first.is<caw::i_favourite_folder::iterator>())
				{
					caw::i_favourite_folder::iterator iterExisting = static_variant_cast<const caw::i_favourite_folder::iterator&>(i->first);
					iFavourites.remove_favourite(iterExisting);
				}
				i = iRequests.erase(i);
			}
			else
				++i;
		}
	}

	void favourite_requester::connection_added(irc::connection& aConnection)
	{
		iWatchedConnections.push_back(&aConnection);
		aConnection.add_observer(*this);
	}

	void favourite_requester::connection_removed(irc::connection& aConnection)
	{
		iWatchedConnections.remove(&aConnection);
		remove_requests(aConnection);
	}

	void favourite_requester::filter_message(irc::connection& aConnection, const irc::message& aMessage, bool& aFiltered)
	{
		irc::buffer* activeBuffer = aConnection.connection_manager().active_buffer();
		irc::buffer& msgBuffer = activeBuffer != 0 ? *activeBuffer : aConnection.server_buffer();
		if (neolib::make_ci_string(aMessage.command_string()) == "bind")
		{
			if (aMessage.parameters().size() == 0)
			{
				if (iFavourites.root().find_subfolder("keybindings") == iFavourites.root().contents().end() ||
					iFavourites.root().get_subfolder("keybindings").contents().empty())
					msgBuffer.new_message("/echo No key bindings set.");
				else
				{
					msgBuffer.new_message("/echo Key bindings:");
					caw::i_favourite_folder& keybindingsFolder = iFavourites.root().get_subfolder("keybindings");
					for (caw::i_favourite_folder::iterator i = keybindingsFolder.contents().begin(); i != keybindingsFolder.contents().end(); ++i)
					{
						if ((**i).type() != caw::i_favourite::Item)
							continue;
						const caw::i_favourite_item& item = static_cast<const caw::i_favourite_item&>(**i);
						std::string line;
						line += "/echo " + item.keybinding().to_std_string() + ": " + item.name() + " (" + item.property("server_network").to_std_string() + ")";
						msgBuffer.new_message(line);
					}
				}
			}
			else if (aMessage.parameters().size() == 1)
			{
				unsigned int key = neolib::string_to_unsigned_integer(aMessage.parameters()[0]);
				if (key >= 1 && key <= 9)
				{
					if (activeBuffer != 0 && (activeBuffer->type() == irc::buffer::CHANNEL || activeBuffer->type() == irc::buffer::USER))
					{
						caw::i_favourite_folder& keybindingsFolder = iFavourites.root().get_subfolder("keybindings");
//						iFavourites.add_favourite(keybindingsFolder, )
//						bind(key, *activeBuffer);
						msgBuffer.new_message("/echo Key bound to window.");
					}
					else
						msgBuffer.new_message("/echo Invalid window.");
				}
				else
					msgBuffer.new_message("/echo Invalid key.");
			}
			else
				msgBuffer.new_message("/echo Usage: /bind [<number>]");
			aFiltered = true;
		}
		else if (neolib::make_ci_string(aMessage.command_string()) == "unbind")
		{
			if (aMessage.parameters().size() == 0)
			{
				if (activeBuffer != 0 && (activeBuffer->type() == irc::buffer::CHANNEL || activeBuffer->type() == irc::buffer::USER))
				{
//					if (unbind(*activeBuffer))
	//					msgBuffer.new_message("/echo Key binding removed.");
		//			else
						msgBuffer.new_message("/echo Window has no binding.");
				}
				else
					msgBuffer.new_message("/echo Invalid window.");
			}
			else if (aMessage.parameters().size() == 1)
			{
				unsigned int key = neolib::string_to_unsigned_integer(aMessage.parameters()[0]);
				if (key >= 1 && key <= 9)
				{
//					if (unbind(key))
	//					msgBuffer.new_message("/echo Key binding removed.");
		//			else
						msgBuffer.new_message("/echo Key has no binding.");
				}
				else
					msgBuffer.new_message("/echo Invalid key.");
			}
			else
				msgBuffer.new_message("/echo Usage: /unbind <number>");

			aFiltered = true;
		}
	}

	void favourite_requester::connection_registered(irc::connection& aConnection)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			const caw::i_favourite_item& entry = to_item(i->first);
			if (entry.property("nick_name") == aConnection.identity().nick_name() &&
				entry.property("server_network") == aConnection.server().key().first &&
				(entry.property("server_name") == aConnection.server().key().second ||
				entry.property("server_name") == "*" || iConnectionManager.reconnect_any_server()))
			{
				if (static_cast<favourite_type_e>(boost::lexical_cast<int>(entry.property("type"))) == Channel)
				{
					irc::message joinMessage(aConnection, irc::message::OUTGOING);
					joinMessage.set_command(irc::message::JOIN);
					joinMessage.parse_parameters("JOIN " + entry.property("name"));
					aConnection.send_message(joinMessage);
				}
				else
				{
					aConnection.create_buffer(entry.property("name").to_std_string()).activate();
				}
				iRequests.erase(i++);
			}
			else
				++i;
		}
	}

	void favourite_requester::connection_quitting(irc::connection& aConnection)
	{
		remove_requests(aConnection);
	}

	void favourite_requester::connection_disconnected(irc::connection& aConnection)
	{
		if (!iConnectionManager.auto_reconnect())
			remove_requests(aConnection);
	}

	void favourite_requester::connection_giveup(irc::connection& aConnection)
	{
		remove_requests(aConnection);
	}

	void favourite_requester::favourite_added(const caw::i_favourite& aFavourite)
	{
	}

	void favourite_requester::favourite_removing(const caw::i_favourite& aFavourite)
	{
		for (request_list::iterator i = iRequests.begin(); i != iRequests.end();)
		{
			const caw::i_favourite_item& entry = to_item(i->first);
			if (&entry == &aFavourite)
				i = iRequests.erase(i);
			else
				++i;
		}
	}

	void favourite_requester::favourite_removed(const caw::i_favourite& aFavourite)
	{
	}

	void favourite_requester::favourite_updated(const caw::i_favourite& aFavourite)
	{
	}

	void favourite_requester::open_favourite(const caw::i_favourite_item& aFavourite)
	{
		add_request(aFavourite);
	}

}
