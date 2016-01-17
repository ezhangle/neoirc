// server.cpp
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
#include <cstdlib>
#include <neolib/xml.hpp>
#include <fstream>
#include "server.hpp"
#include "model.hpp"

namespace irc
{
	server::port_type server::port(neolib::random& aRandom) const
	{
		uint32_t numPorts = 0;
		for (port_list::const_iterator i = iPorts.begin(); i != iPorts.end(); ++i)
		{
			numPorts += (i->second - i->first) + 1;
		}
		uint32_t randomPort = aRandom.get(0, numPorts - 1);
		uint32_t nextPort = 0;
		for (port_list::const_iterator i = iPorts.begin(); i != iPorts.end(); ++i)
		{
			if (randomPort >= nextPort && randomPort <= nextPort + (i->second - i->first))
			{
				return i->first + (randomPort - nextPort);			
			}
			nextPort += (i->second - i->first) + 1;
		}
		return 0;
	}

	namespace
	{
		std::string make_ports_string(const server::port_list& aPorts)
		{
			std::stringstream ret;
			server::port_list::const_iterator last = aPorts.end(); --last;
			for (server::port_list::const_iterator i = aPorts.begin(); i != aPorts.end(); ++i)
			{
				if (i->first == i->second)
					ret << i->first;
				else
					ret << i->first << "-" << i->second;
				if (i != last)
					ret << ",";
			}
			return ret.str();
		}
		void parse_ports(const std::string& aPortsString, server::port_list& aPorts)
		{
			aPorts.clear();
			std::vector<std::string> ports;
			neolib::tokens(aPortsString, std::string(","), ports);
			if (ports.size() == 0)
				return;
			for (std::vector<std::string>::iterator i = ports.begin(); i != ports.end(); ++i)
			{
				std::vector<std::string> ranges;
				neolib::tokens(*i, std::string("-"), ranges);
				switch(ranges.size())
				{
				case 1:
					{
						server::port_type port = static_cast<server::port_type>(strtoul(ranges[0].c_str(), 0, 10));
						aPorts.push_back(std::make_pair(port, port));
					}
					break;
				case 2:
					{
						server::port_type port1 = static_cast<server::port_type>(strtoul(ranges[0].c_str(), 0, 10));
						server::port_type port2 = static_cast<server::port_type>(strtoul(ranges[1].c_str(), 0, 10));
						aPorts.push_back(std::make_pair(port1, port2));
					}
					break;
				}
			}
		}
		struct details
		{
			std::string iAddress;
			server::port_list iPorts;
			bool iPassword;
		};
		bool old_server_details(const std::string& aAddress, details& aDetails)
		{
			std::vector<std::string> parts;
			neolib::tokens(aAddress, std::string(":"), parts);
			if (parts.size() < 2)
				return false;
			aDetails.iAddress = neolib::remove_leading_and_trailing(parts[0], std::string(" "));
			parse_ports(parts[1], aDetails.iPorts);
			aDetails.iPassword = parts.size() >= 3 && parts[2][0] == '*';
			return true;
		}
	}

	bool read_server_list(server_list& aServerList, std::istream& aInput)
	{
		aServerList.clear();

		neolib::xml xmlServers(true);
		
		xmlServers.read(aInput);

		if (xmlServers.error())
			return false;

		for (neolib::xml::element::const_iterator i = xmlServers.root().begin(); i != xmlServers.root().end(); ++i)
		{
			const neolib::xml::element& networkElement = *i;
			if (networkElement.name() == "version")
			{
				aServerList.iVersion = networkElement.attribute_value("value");
			}
			else	if (networkElement.name() == "network")
			{
				for (neolib::xml::element::const_iterator j = networkElement.begin(); j != networkElement.end(); ++j)
				{
					const neolib::xml::element& serverElement = *j;
					if (serverElement.name() == "server")
					{
						server theServer;
						theServer.set_network(networkElement.attribute_value("name"));
						theServer.set_name(serverElement.attribute_value("name"));
						theServer.set_address(serverElement.attribute_value("address"));
						irc::server::port_list portList;
						parse_ports(serverElement.attribute_value("ports"), portList);
						theServer.set_ports(portList);
						theServer.set_password(serverElement.attribute_value("password") == "1" ? true : false);
						theServer.set_secure(serverElement.attribute_value("secure") == "1" ? true : false);
						aServerList.push_back(theServer);
					}
				}
			}
		}
		return true;
	}

	void read_server_list(model& aModel, std::function<bool()> aErrorFunction)
	{
		server_list& theServers = aModel.server_list();

		std::ifstream input((aModel.root_path() + "servers.xml").c_str());

		if (!read_server_list(theServers, input) && aErrorFunction && aErrorFunction())
		{
			theServers.clear();
			write_server_list(aModel);
			return;
		}

		std::ifstream oldServerFile((aModel.root_path() + "servers.dat").c_str());
		if (oldServerFile)
		{
			server_list oldServerList;
			std::string strLine;
			while(std::getline(oldServerFile, strLine))
			{
				std::vector<std::string> parts;
				neolib::tokens(strLine, std::string("{}"), parts);
				if (parts.size() != 3)
					continue;
				details theDetails;
				if (!old_server_details(parts[2], theDetails))
					continue;
				oldServerList.push_back(server(parts[0], parts[1], theDetails.iAddress, theDetails.iPorts, theDetails.iPassword, false));
			}
			oldServerFile.close();
			server_list newServerList = theServers;
			theServers = oldServerList;
			merge_server_list(aModel, newServerList);
			remove((aModel.root_path() + "servers.dat").c_str());
		}
	}

	void write_server_list(const model& aModel)
	{
		const server_list& theServers = aModel.server_list();

		neolib::xml xmlServers(true);

		xmlServers.root().name() = "servers";
		xmlServers.append(xmlServers.root(), "version").set_attribute("value", theServers.iVersion);	
		
		typedef std::vector<server_list::const_iterator> servers;
		typedef std::map<neolib::ci_string,  servers> servers_by_network;
		servers_by_network serversByNetwork;
		for (server_list::const_iterator i = theServers.begin(); i != theServers.end(); ++i)
			serversByNetwork[neolib::make_ci_string(i->network())].push_back(i);

		for (servers_by_network::const_iterator i = serversByNetwork.begin(); i != serversByNetwork.end(); ++i)
		{
			neolib::xml::element& xmlNetwork = xmlServers.append(xmlServers.root(), "network");
			xmlNetwork.set_attribute("name", neolib::make_string(i->first));
			for (servers::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				neolib::xml::element& xmlServer = xmlServers.append(xmlNetwork, "server");
				xmlServer.set_attribute("name", (*j)->name());
				xmlServer.set_attribute("address", (*j)->address());
				xmlServer.set_attribute("ports", make_ports_string((*j)->ports()));
				xmlServer.set_attribute("password", std::string((*j)->password() ? "1" : "0"));
				xmlServer.set_attribute("secure", std::string((*j)->secure() ? "1" : "0"));
			}
		}

		std::ofstream output((aModel.root_path() + "servers.xml").c_str());
		xmlServers.write(output);
	}

	void merge_server_list(model& aModel, const server_list& aServerList)
	{
		typedef std::map<std::pair<std::string, bool>, std::pair<server, bool> > merged_servers;
		merged_servers mergedServers;
		for (server_list::const_iterator i = aModel.server_list().begin(); i != aModel.server_list().end(); ++i)
			mergedServers[std::make_pair(i->address(), i->secure())] = std::make_pair(*i, false);
		for (server_list::const_iterator i = aServerList.begin(); i != aServerList.end(); ++i)
		{
			merged_servers::iterator j = mergedServers.find(std::make_pair(i->address(), i->secure()));
			if (j == mergedServers.end())
				mergedServers[std::make_pair(i->address(), i->secure())] = std::make_pair(*i, true);
			else
				j->second.second = true;
		}
		typedef std::map<std::string, std::vector<merged_servers::iterator> > merged_servers_by_network;
		merged_servers_by_network mergedServersByNetwork;
		for (merged_servers::iterator i = mergedServers.begin(); i != mergedServers.end(); ++i)
			mergedServersByNetwork[i->second.first.network()].push_back(i);
		for (merged_servers_by_network::iterator i = mergedServersByNetwork.begin(); i != mergedServersByNetwork.end(); ++i)
		{
			bool found = false;
			for (std::vector<merged_servers::iterator>::iterator j = i->second.begin(); !found && j != i->second.end(); ++j)
				if ((**j).second.second)
					found = true;
	#if 0 // disable the dropping of servers not in the online server list for now...
			if (found) 
				for (std::vector<merged_servers::iterator>::iterator j = i->second.begin(); j != i->second.end(); ++j)
					if (!(**j).second.second)
						mergedServers.erase(*j);
	#endif
		}
		aModel.server_list().iVersion = aServerList.iVersion;
		aModel.server_list().clear();
		for (merged_servers::iterator i = mergedServers.begin(); i != mergedServers.end(); ++i)
			aModel.server_list().push_back(i->second.first);
		aModel.server_list().sort();
		write_server_list(aModel);
	}
}
