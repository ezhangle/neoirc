// server.h
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

#ifndef IRC_CLIENT_SERVER
#define IRC_CLIENT_SERVER

#include <neolib/neolib.hpp>
#include <utility>
#include <vector>
#include <string>
#include <neolib/random.hpp>
#include <neolib/string_utils.hpp>
#include <list>

namespace irc
{
	class model;

	class server
	{
		// types
	public:
		typedef uint16_t port_type;
		typedef std::pair<port_type, port_type> port_range;
		typedef std::vector<port_range> port_list;
		typedef std::pair<std::string, std::string> key_type;

		// construction
	public:
		server(const std::string& aNetwork, const std::string& aName, const std::string& aAddress, const port_list& aPorts = port_list(), bool aPassword = false, bool aSecure = false) : iNetwork(aNetwork), iName(aName), iAddress(aAddress), iPorts(aPorts), iPassword(aPassword), iSecure(aSecure) {}
		server() : iPassword(false), iSecure(false) {}

		// operations
	public:
		const std::string& network() const { return iNetwork; }
		const std::string& name() const { return iName; }
		const std::string& address() const { return iAddress; }
		std::string network_and_name() const { return network() != name() ? network() + " (" + name() + ")" : network(); }
		std::string name_and_address() const { return name() != address() && !address().empty() ? name() + " (" + address() + ")" : name(); }
		const port_list& ports() const { return iPorts; }
		bool password() const { return iPassword; }
		bool secure() const { return iSecure; }
		void set_network(const std::string& aNetwork) { iNetwork = aNetwork; }
		void set_name(const std::string& aName) { iName = aName; }
		void set_address(const std::string& aAddress) { iAddress = aAddress; }
		void set_ports(const port_list& aPorts) { iPorts = aPorts; }
		void set_password(bool aHasConnectionPassword) { iPassword = aHasConnectionPassword; }
		void set_secure(bool aSecure) { iSecure = aSecure; }
		port_type port(neolib::random& aRandom) const;
		key_type key() const { return key_type(iNetwork, iName); }
		operator key_type() const { return key(); }
		bool operator==(const key_type& aOther) const { return iNetwork == aOther.first && (iName == aOther.second || iName == "*" || aOther.second == "*"); }
		bool operator!=(const key_type& aOther) const { return !operator==(aOther); }
		bool operator<(const key_type& aOther) { return neolib::make_ci_string(iNetwork) < neolib::make_ci_string(aOther.first) || (iNetwork == aOther.first && (iName != "*" && aOther.second != "*" && neolib::make_ci_string(iName) < neolib::make_ci_string(aOther.second))); }
		bool operator==(const server& aOther) const { return iNetwork == aOther.iNetwork && (iName == aOther.iName || iName == "*" || aOther.iName == "*"); }
		bool operator!=(const server& aOther) const { return !operator==(aOther); }
		bool operator<(const server& aOther) { return neolib::make_ci_string(iNetwork) < neolib::make_ci_string(aOther.iNetwork) || (iNetwork == aOther.iNetwork && (iName != "*" && aOther.iName != "*" && neolib::make_ci_string(iName) < neolib::make_ci_string(aOther.iName))); }

		// attributes
	protected:
		std::string iNetwork;
		std::string iName;
		std::string iAddress;
		bool iPassword;
		bool iSecure;
		port_list iPorts;
	};

	typedef server::key_type server_key;

	inline server_key any_server(const server_key& aServer) 
	{ 
		return server_key(aServer.first, "*"); 
	}

	inline bool server_match(const server_key& aFirst, const server_key& aSecond)
	{
		return (aFirst.first == aSecond.first || aFirst.first == "*" || aSecond.first == "*") &&
			(aFirst.second == aSecond.second || aFirst.second == "*" || aSecond.second == "*");
	}

	struct server_match_predicate
	{
		server_key iToFind;
		server_match_predicate(const server_key& aToFind) : iToFind(aToFind) {}
		bool operator()(const server_key& aItem) const { return server_match(iToFind, aItem); }
	};

	struct server_list : std::list<server>
	{
		std::string iVersion;
	};

	bool read_server_list(server_list& aServerList, std::istream& aInput);
	void read_server_list(model& aModel, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_server_list(const model& aModel);
	void merge_server_list(model& aModel, const server_list& aServerList);
}

#endif //IRC_CLIENT_SERVER