// macros.h
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

#ifndef IRC_CLIENT_MACROS
#define IRC_CLIENT_MACROS

#include <neolib/neolib.hpp>
#include <neolib/string_utils.hpp>
#include <neolib/mutable_set.hpp>

namespace irc
{
	class macro
	{
	public:
		// types
		typedef neolib::ci_string key_type;
		enum error
		{
			ok,
			syntax_error,
			insufficient_parameters,
			not_found,
			too_deep
		};
	public:
		// construction
		macro() : iEnabled(true), iUserMenu(false) {}
		macro(const macro& aOther) : iName(aOther.iName), iDescription(aOther.iDescription), iScript(aOther.iScript), iEnabled(aOther.iEnabled), iUserMenu(aOther.iUserMenu) {}
		macro(const std::string& aName, const std::string& aDescription, const std::string& aScript, bool aEnabled, bool aUserMenu) : iName(aName), iDescription(aDescription), iScript(aScript), iEnabled(aEnabled), iUserMenu(aUserMenu) {}
	public:
		// operations
		operator key_type() const { return neolib::make_ci_string(iName); }
		const std::string& name() const { return iName; }
		void set_name(const std::string& aValue) { iName = aValue; if (!iName.empty() && iName[0] != '/') iName = "/" + iName; }
		const std::string& description() const { return iDescription; }
		void set_description(const std::string& aValue) { iDescription = aValue; }
		const std::string& script() const { return iScript; }
		void set_script(const std::string& aValue) { iScript = aValue; }
		bool enabled() const { return iEnabled; }
		void set_enabled(bool aValue) { iEnabled = aValue; }
		bool user_menu() const { return iUserMenu; }
		void set_user_menu(bool aValue) { iUserMenu = aValue; }
		bool operator==(const macro& aOther) const 
		{ 
			return iName == aOther.iName;
		}
	private:
		// attributes
		std::string iName;
		std::string iDescription;
		std::string iScript;
		bool iEnabled;
		bool iUserMenu;
	};

	class macros_observer
	{
	public:
		friend class macros;
	private:
		virtual void macro_added(const macro& aEntry) = 0;
		virtual void macro_updated(const macro& aEntry) = 0;
		virtual void macro_removed(const macro& aEntry) = 0;
		virtual void macro_syntax_error(const macro& aEntry, std::size_t aLineNumber, macro::error aError) = 0;
	public:
		enum notify_type { NotifyAdded, NotifyUpdated, NotifyRemoved, NotifySyntaxError };
	};

	class macros : public neolib::observable<macros_observer>
	{
	public:
		// types
		typedef neolib::mutable_set<macro> container_type;
		typedef container_type::const_iterator const_iterator;
		typedef container_type::iterator iterator;

	public:
		// construction
		macros();
		~macros();

	public:
		// operations
		const container_type& entries() const { return iEntries; }
		container_type& entries() { return iEntries; }
		bool has_macro(const std::string& aMacro) const;
		bool add(const std::string& aMacro, const std::string& aDescription, const std::string& aScript, bool aEnabled, bool aUserMenu);
		bool add(const macro& aEntry);
		void update(macro& aExistingEntry, const macro& aNewEntry);
		void remove(const std::string& aMacro);
		void remove(const macro& aEntry);
		void clear() { iEntries.clear(); }
		void loading(bool aLoading) { iLoading = aLoading; }
		bool parse(buffer& aBuffer, const std::string& aMessage, bool aAll = false);

	private:
		// implementation
		macro::error parse_line(buffer& aBuffer, const std::string& aMessage, const std::string& aLine, bool aAll = false);
		// from neolib::observable<macros_observer>
		virtual void notify_observer(macros_observer& aObserver, macros_observer::notify_type aType, const void* aParameter = 0, const void* aParameter2 = 0);

	private:
		// attributes
		container_type iEntries;
		bool iLoading;
		std::size_t iParseDepth;
	};

	void read_macros(model& aModel, std::function<bool()> aErrorFunction = std::function<bool()>());
	void write_macros(const model& aModel);
}

#endif //IRC_CLIENT_MACROS
