// connection_manager_observer.h
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

#ifndef IRC_CLIENT_CONNECTION_MANAGER_OBSERVER
#define IRC_CLIENT_CONNECTION_MANAGER_OBSERVER

namespace irc
{
	class connection;
	class buffer;
	class message;

	class connection_manager_observer
	{
		friend class connection_manager;
	private:
		virtual void connection_added(connection& aConnection) = 0;
		virtual void connection_removed(connection& aConnection) = 0;
		virtual void filter_message(connection& aConnection, const message& aMessage, bool& aFiltered) = 0;
		virtual bool query_disconnect(const connection& aConnection) = 0;
		virtual void query_nickname(connection& aConnection) = 0;
		virtual void disconnect_timeout_changed() = 0;
		virtual void retry_network_delay_changed() = 0;
		virtual void buffer_activated(buffer& aActiveBuffer) = 0;
		virtual void buffer_deactivated(buffer& aDeactivatedBuffer) = 0;

	public:
		enum notify_type { NotifyConnectionAdded, NotifyConnectionRemoved, NotifyFilterMessage, NotifyDisconnectTimeoutChanged, NotifyRetryNetworkDelayChanged, NotifyBufferActivated, NotifyBufferDeactivated };
	};
}

#endif //IRC_CLIENT_CONNECTION_MANAGER_OBSERVER
