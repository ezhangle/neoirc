// favourite_requester.h
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

#ifndef CAW_FAVORITE_REQUESTER
#define CAW_FAVORITE_REQUESTER

#include <neolib/variant.hpp>
#include <irc/client/connection_manager.hpp>
#include <irc/client/connection.hpp>
#include <irc/client/identity.hpp>
#include <caw/i_favourites.hpp>
#include <caw/favourite.hpp>

namespace caw_irc_plugin
{
	class favourite_requester : private irc::identities_observer, private irc::connection_manager_observer, private irc::connection_observer, private caw::i_favourites::i_subscriber
	{
		// types
	public:
		enum favourite_type_e { Channel, User };
	private:
		typedef neolib::variant<caw::favourite_item, caw::i_favourite_folder::iterator> request_type;
		struct request_comparator
		{
			bool operator()(const request_type& aLhs, const request_type& aRhs) const
			{
				const caw::i_favourite_item& lhs = to_item(aLhs);
				const caw::i_favourite_item& rhs = to_item(aRhs);
				return std::make_tuple(lhs.property("nick_name").to_std_string(), lhs.property("server_network").to_std_string(), lhs.property("server_name").to_std_string(), lhs.property("name").to_std_string(), lhs.property("type").to_std_string()) <
					std::make_tuple(rhs.property("nick_name").to_std_string(), rhs.property("server_network").to_std_string(), rhs.property("server_name").to_std_string(), rhs.property("name").to_std_string(), rhs.property("type").to_std_string());
			}
		};
		typedef std::map<request_type, irc::connection*, request_comparator> request_list;

		// construction
	public:
		favourite_requester(neolib::random& aRandom, irc::connection_manager& aConnectionManager, irc::identities& aIdentities, irc::server_list& aServerList, caw::i_favourites& aFavorites);
		~favourite_requester();

		// operations
	public:
		void add_request(const caw::i_favourite_item& aRequest);
		caw::favourite_folder& request_folder() { return iRequestFolder; }

		// implementation
	private:
		void add_request(irc::connection& aConnection, const caw::i_favourite_item& aRequest);
		void remove_requests(irc::connection& aConnection);
		static const caw::i_favourite_item& to_item(const request_type& aRequest)
		{
			return aRequest.is<caw::favourite_item>() ?
				static_variant_cast<const caw::i_favourite_item&>(aRequest) :
				static_cast<const caw::i_favourite_item&>(**static_variant_cast<const caw::i_favourite_folder::iterator&>(aRequest));
		}

		// from irc::identities_observer
		virtual void identity_added(const irc::identity& aEntry);
		virtual void identity_updated(const irc::identity& aEntry, const irc::identity& aOldEntry);
		virtual void identity_removed(const irc::identity& aEntry);
		// from irc::connection_manager_observer
		virtual void connection_added(irc::connection& aConnection);
		virtual void connection_removed(irc::connection& aConnection);
		virtual void filter_message(irc::connection& aConnection, const irc::message& aMessage, bool& aFiltered);
		virtual bool query_disconnect(const irc::connection& aConnection) { return false; }
		virtual void query_nickname(irc::connection& aConnection) {}
		virtual void disconnect_timeout_changed() {}
		virtual void retry_network_delay_changed() {}
		virtual void buffer_activated(irc::buffer& aActiveBuffer) {}
		virtual void buffer_deactivated(irc::buffer& aDeactivatedBuffer) {}
		// from irc::connection_observer
		virtual void connection_connecting(irc::connection& aConnection) {}
		virtual void connection_registered(irc::connection& aConnection);
		virtual void buffer_added(irc::buffer& aBuffer) {}
		virtual void buffer_removed(irc::buffer& aBuffer) {}
		virtual void incoming_message(irc::connection& aConnection, const irc::message& aMessage) {}
		virtual void outgoing_message(irc::connection& aConnection, const irc::message& aMessage) {}
		virtual void connection_quitting(irc::connection& aConnection);
		virtual void connection_disconnected(irc::connection& aConnection);
		virtual void connection_giveup(irc::connection& aConnection);
		// from caw::i_favourites::i_subscriber
		virtual void favourite_added(const caw::i_favourite& aFavourite);
		virtual void favourite_removing(const caw::i_favourite& aFavourite);
		virtual void favourite_removed(const caw::i_favourite& aFavourite);
		virtual void favourite_updated(const caw::i_favourite& aFavourite);
		virtual void open_favourite(const caw::i_favourite_item& aFavourite);

		// attributes
	private:
		neolib::random& iRandom;
		irc::connection_manager& iConnectionManager;
		irc::identities& iIdentities;
		irc::server_list& iServerList;
		caw::i_favourites& iFavourites;
		typedef std::list<irc::connection*> watched_connections;
		watched_connections iWatchedConnections;
		caw::favourite_folder iRequestFolder;
		request_list iRequests;
	};
}

#endif //CAW_FAVORITE_REQUESTER
