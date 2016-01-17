// server_updater.cpp
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
#include <neolib/timer.hpp>
#include <neolib/gunzip.hpp>
#include <neolib/version.hpp>
#include <strstream>
#include "server_updater.hpp"
#include "model.hpp"

namespace irc
{
	server_list_updater::server_list_updater(model& aModel) : neolib::timer(aModel.owner_thread(), 60 * 60 * 1000), iModel(aModel), iDownloading(false) 
	{
	}

	server_list_updater::~server_list_updater()
	{
	}

	void server_list_updater::check_for_updates(bool aEnable)
	{
		if (aEnable && waiting())
			return;
		if (aEnable)
		{
			iChecker.reset();
			iChecker = neolib::http(iModel.owner_thread());
			iChecker->add_observer(*this);
			iChecker->request("clicksandwhistles.com", "/servers.version");
			again();
		}
		else
			cancel();
	}

	void server_list_updater::download()
	{
		iDownloader.reset();
		iDownloader = neolib::http(iModel.owner_thread());
		iDownloader->add_observer(*this);
		neolib::http::headers_t requestHeaders;
		requestHeaders["Accept-Encoding"] = "identity";
		iDownloader->request("http://clicksandwhistles.com/servers.xml.gz", neolib::http::Get, requestHeaders);
	}

	void server_list_updater::notify_observer(server_list_updater_observer& aObserver, server_list_updater_observer::notify_type aType, const void* aParameter, const void* aParameter2)
	{
		switch (aType)
		{
		case server_list_updater_observer::NotifyDownloading:
			aObserver.server_list_downloading();
			break;
		case server_list_updater_observer::NotifyDownloaded:
			aObserver.server_list_downloaded();
			break;
		case server_list_updater_observer::NotifyDownloadFailure:
			aObserver.server_list_download_failure();
			break;
		}
	}

	void server_list_updater::http_request_started(neolib::http& aRequest)
	{
		if (iDownloader && &*iDownloader == &aRequest)
		{
			iDownloading = true;
			notify_observers(server_list_updater_observer::NotifyDownloading);
		}
	}

	void server_list_updater::http_request_completed(neolib::http& aRequest)
	{
		if (iChecker && &*iChecker == &aRequest)
		{
			bool newerAvailable = neolib::version(aRequest.body_as_string()) > neolib::version(iModel.server_list().iVersion);
			if (newerAvailable)
				download();
		}
		else if (iDownloader && &*iDownloader == &aRequest)
		{
			iDownloading = false;
			neolib::gunzip decompressed(aRequest.body());
			bool ok = false;
			if (decompressed.ok() && !decompressed.uncompressed_data().empty())
			{
				std::strstream ss(const_cast<char*>(&decompressed.uncompressed_data()[0]), decompressed.uncompressed_data().size());
				server_list updatedList;
				if (read_server_list(updatedList, ss))
				{
					merge_server_list(iModel, updatedList);
					ok = true;
				}
			}
			if (ok)
				notify_observers(server_list_updater_observer::NotifyDownloaded);
			else
				notify_observers(server_list_updater_observer::NotifyDownloadFailure);
		}
	}

	void server_list_updater::http_request_failure(neolib::http& aRequest)
	{
		if (iChecker && &*iChecker == &aRequest)
		{
		}
		else if (iDownloader && &*iDownloader == &aRequest)
		{
			iDownloading = false;
			notify_observers(server_list_updater_observer::NotifyDownloadFailure);
		}
	}
}
