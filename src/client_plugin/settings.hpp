#pragma once

// settings.hpp
/*
 *  Copyright (c) 2010, 2014 Leigh Johnston.
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
#include <map>
#include <neolib/settings.hpp>
#include <neolib/i_application.hpp>
#include <caw/i_gui_setting_presentation_info_provider.hpp>
#include <caw/gui_setting_presentation_info.hpp>
#include <caw/gui_colour.hpp>
#include <caw/gui_custom_type_factory.hpp>
#include "logger.hpp"
#include "message_strings.hpp"
#include "identd.hpp"

namespace caw_irc_plugin
{
	class settings : public neolib::settings, public neolib::reference_counted < caw::i_gui_setting_presentation_info_provider >
	{
		// types
	public:
		enum encoding_e { ANSI, Mixed, UTF8 };
		enum alignment_e { Left, Centred, Right, Justified };
		enum formatting_e { Ignore, Strip, Display };
		enum close_tab_on_e { MiddleClick, DoubleClick };
		enum reconnect_server_e { ReconnectServerSame, ReconnectServerNetwork };
		enum item_sort_e { SortChronologically, SortAlphabetically };
		enum events_e { EventNone = 0x0, EventMessage = 0x1, EventNotice = 0x2, EventJoinPartQuit = 0x4, EventKick = 0x8, EventMode = 0x10, EventOther = 0x20 };
		enum crazy_colours_key_e { NickName, UserName, HostName };
		enum column_max_width_type_e { Percentage, Pixels };
		typedef caw::gui_colour colour;
	private:
		typedef std::map<neolib::i_setting::id_type, caw::gui_setting_presentation_info> setting_presentation_info_map_type;

		// construction
	public:
		settings(const neolib::i_application& aApplication) :
			neolib::settings(aApplication.info().settings_folder() + neolib::string("/irc_settings.xml"), neolib::auto_ref<neolib::i_custom_type_factory>(new caw::gui_custom_type_factory())), iApplication(aApplication) { register_settings(); }

		// operations
	public:
		virtual void add_ref() const { return neolib::settings::add_ref(); }
		virtual void release() const { return neolib::settings::release(); }

		// implementation
	private:
		virtual bool has_presentation_info(const neolib::i_setting& aSetting) const;
		virtual const caw::i_gui_setting_presentation_info& presentation_info(const neolib::i_setting& aSetting) const;
	private:
		void register_settings();

		// attributes
	private:
		const neolib::i_application& iApplication;
		setting_presentation_info_map_type iSettingPresentationInfo;
	};
}