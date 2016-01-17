// server_updater.h
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

#ifndef IRC_CLIENT_SERVER_UPDATER
#define IRC_CLIENT_SERVER_UPDATER

#include <neolib/timer.hpp>
#include <neolib/http.hpp>
#include "server.hpp"

namespace irc
{
	class server_list_updater_observer
	{
		friend class server_list_updater;
	private:
		virtual void server_list_downloading() = 0;
		virtual void server_list_downloaded() = 0;
		virtual void server_list_download_failure() = 0;
	public:
		enum notify_type { NotifyDownloading, NotifyDownloaded, NotifyDownloadFailure };
	};

	class server_list_updater : public neolib::observable<server_list_updater_observer>, private neolib::timer, private neolib::i_http_observer
	{
		// construction
	public:
		server_list_updater(model& aModel);
		virtual ~server_list_updater();

		// operations
	public:
		void check_for_updates(bool aEnable);
		void download();
		bool is_downloading() const { return iDownloading; }

		// implementation
	private:
		// from neolib::observable<server_list_updater_observer>
		virtual void notify_observer(server_list_updater_observer& aObserver, server_list_updater_observer::notify_type aType, const void* aParameter, const void* aParameter2);
		// from neolib::timer
		virtual void ready() { check_for_updates(true); }
		// from neolib::i_http_observer
		virtual void http_request_started(neolib::http& aRequest);
		virtual void http_request_completed(neolib::http& aRequest);
		virtual void http_request_failure(neolib::http& aRequest);	

		// attributes
	private:
		model& iModel;
		neolib::optional<neolib::http> iChecker;
		neolib::optional<neolib::http> iDownloader;
		bool iDownloading;
	};
}

#endif //IRC_CLIENT_SERVER_UPDATER
