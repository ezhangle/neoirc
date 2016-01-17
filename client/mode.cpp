// mode.cpp
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
#include "mode.hpp"
#include "message.hpp"

namespace irc
{
	void parse_mode(const message& aMessage, mode::list& aModeList)
	{
		aModeList.clear();
		std::string modes;
		mode::parameters theModeParameters;
		message::parameters_t::const_iterator next = aMessage.parameters().begin() + 1;
		while (next != aMessage.parameters().end())
		{
			if (modes.empty())
			{
				modes = *next++;
				if (next != aMessage.parameters().end())
					continue;
			}

			std::string parameter = modes;
			if (next != aMessage.parameters().end())
				parameter = *next++;
			bool parameterIsMode = (parameter[0] == '+' || parameter[0] == '-');

			if (!parameterIsMode)
				theModeParameters.push_back(parameter);

			if (parameterIsMode || next == aMessage.parameters().end())
			{
				mode::direction currentDirection = mode::ADD;
				mode::list::iterator mie = aModeList.end();
				if (!aModeList.empty())
					--mie;
				for (std::string::const_iterator mi = modes.begin(); mi != modes.end(); ++mi)
				{
					if (*mi == '+')
					{
						currentDirection = mode::ADD;
						continue;
					}
					else if (*mi == '-')
					{
						currentDirection = mode::SUBTRACT;
						continue;
					}
					else
					{
						aModeList.push_back(mode());
						aModeList.back().iDirection = currentDirection;
						aModeList.back().iType = static_cast<mode::type>(*mi);
					}
				}
				mode::list::reverse_iterator mi = aModeList.rbegin();
				for (mode::parameters::reverse_iterator pi = theModeParameters.rbegin(); pi != theModeParameters.rend() && mi != aModeList.rend(); ++pi)
					if (mie == aModeList.end() || mi.operator->() != mie.operator->())
						(*mi++).iParameter = *pi;
			}
			if (parameterIsMode)
				modes = parameter;
		}
	}
}