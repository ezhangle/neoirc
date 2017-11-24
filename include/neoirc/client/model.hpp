// model.h
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

#ifndef IRC_CLIENT_MODEL
#define IRC_CLIENT_MODEL

#include <neolib/observable.hpp>
#include <neolib/random.hpp>
#include <neolib/thread.hpp>
#include <neolib/io_task.hpp>
#include <neoirc/client/fwd.hpp>
#include <neoirc/client/gui.hpp>
#include <neoirc/client/message_strings.hpp>

namespace irc
{
	class model_observer
	{
		friend class model;
	private:
		virtual void buffer_size_changed(std::size_t aNewBufferSize) = 0;

	public:
		enum notify_type { NotifyBufferSizeChanged };
	};

	class model_impl;

	class model : public neolib::observable<model_observer>, public neolib::observable<gui>
	{
		friend class model_impl;
		friend class buffer;
		friend class dcc_connection;

	public:
		// types
		typedef uint64_t id;
		static const id invalid_id = -1;
		static const id no_id = invalid_id;
		typedef bool (*yield_proc_t)();
		struct no_new_buffer : std::logic_error { no_new_buffer() : std::logic_error("irc::model::no_new_buffer") {} };
		struct no_new_dcc_connection : std::logic_error { no_new_dcc_connection() : std::logic_error("irc::model::no_new_dcc_connection") {} };

	public:
		// construction
		model(neolib::thread& aOwnerThread);
		~model();

	public:
		// operations
		void set_yield_proc(yield_proc_t aYieldProc) { iYieldProc = aYieldProc; }
		bool yield() const { return iYieldProc && iYieldProc(); }
		yield_proc_t yield_proc() const { return iYieldProc; }
		neolib::thread& owner_thread() const { return iOwnerThread; }
		neolib::io_task& io_task() const { return iIoTask; }
		neolib::random& random();
		irc::identities& identities();
		const irc::identities& identities() const;
		irc::identity_list& identity_list();
		const irc::identity_list& identity_list() const;
		const irc::identity& default_identity() const;
		void set_default_identity(const identity& aIdentity);
		irc::server_list& server_list();
		const irc::server_list& server_list() const;
		irc::server_list_updater& server_list_updater();
		irc::identd& identd();
		irc::auto_joins& auto_joins();
		const irc::auto_joins& auto_joins() const;
		irc::contacts& contacts();
		const irc::contacts& contacts() const;
		irc::connection_scripts& connection_scripts();
		const irc::connection_scripts& connection_scripts() const;
		irc::ignore_list& ignore_list();
		const irc::ignore_list& ignore_list() const;
		irc::notify& notify_list();
		const irc::notify& notify_list() const;
		irc::auto_join_watcher& auto_join_watcher();
		irc::connection_script_watcher& connection_script_watcher();
		irc::notify_watcher& notify_watcher();
		irc::auto_mode& auto_mode_list();
		const irc::auto_mode& auto_mode_list() const;
		irc::connection_manager& connection_manager();
		const irc::connection_manager& connection_manager() const;
		irc::dcc_connection_manager& dcc_connection_manager();
		const irc::dcc_connection_manager& dcc_connection_manager() const;
		irc::logger& logger();
		irc::macros& macros();
		const irc::macros& macros() const;
		irc::plugins& plugins();
		const irc::plugins& plugins() const;
		void set_buffer_size(std::size_t aBufferSize);
		std::size_t buffer_size() const { return iBufferSize; }
		void set_new_buffer(buffer* aBuffer) { iNewBuffer = aBuffer; }
		buffer& new_buffer() const { if (iNewBuffer != 0) return *iNewBuffer; throw no_new_buffer();  }
		void set_new_dcc_connection(dcc_connection* aConnection) { iNewDccConnection = aConnection; }
		dcc_connection& new_dcc_connection() const { if (iNewDccConnection != 0) return *iNewDccConnection; throw no_new_dcc_connection(); }
		std::string error_message(int aError, const std::string& aErrorString) const;
		void set_root_path(const std::string& aRootPath) { iRootPath = aRootPath; }
		const std::string& root_path() const { return iRootPath; }
		const irc::message_strings& message_strings() const { return iMessageStrings; }
		irc::message_strings& message_strings() { return iMessageStrings; }
		bool is_unicode(const buffer& aBuffer);
		bool is_unicode(const dcc_connection& aBuffer);
		bool can_activate_buffer(const buffer& aBuffer);
		bool do_custom_command(const std::string& aCommand);
		bool enter_password(irc::connection& aConnection);

	private:
		// implementation
		void reopen_buffer(buffer& aBuffer);
		void reopen_connection(dcc_connection& aConnection);
		// from neolib::observable<model_observer>
		virtual void notify_observer(model_observer& aObserver, model_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);
		// from neolib::observable<gui>
		virtual void notify_observer(gui& aObserver, gui::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);

	private:
		// attributes
		neolib::thread& iOwnerThread;
		mutable neolib::io_task iIoTask;
		yield_proc_t iYieldProc;
		std::unique_ptr<model_impl> iModelImpl;
		std::size_t iBufferSize;
		buffer* iNewBuffer;
		dcc_connection* iNewDccConnection;
		std::string iRootPath;
		irc::message_strings iMessageStrings;
	public:
		static std::string sErrorMessageString;
		static char sPathSeparator;
	};
}

#endif // IRC_CLIENT_MODEL