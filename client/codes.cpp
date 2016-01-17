// codes.cpp
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
#include "codes.hpp"

namespace irc
{
	void parse_codes(std::string& aText, const codes& aCodes, neolib::string_spans* aSpans)
	{
		std::string::size_type optionalPos = 0;
		while (optionalPos != std::string::npos && (optionalPos = aText.find("%?", optionalPos)) != std::string::npos)
		{
			std::string::size_type startPos = optionalPos;
			optionalPos += 2;
			std::string::size_type nextPos = aText.find('%', optionalPos);
			if (nextPos != std::string::npos)
			{
				std::string optionalCodeName = "%" + aText.substr(startPos + 2, nextPos - (startPos + 2) + 1);
				std::string::size_type endPos = aText.find("%?%", nextPos);
				if (endPos != std::string::npos)
				{
					codes::const_iterator i = aCodes.find(optionalCodeName);
					bool hasValue = false;
					if (i != aCodes.end())
					{
						const std::string& replacementString = i->second.first.is<std::string>() ? static_cast<const std::string&>(i->second.first) : static_cast<const std::tr1::function<std::string()>&>(i->second.first)();
						hasValue = !replacementString.empty();
					}
					std::string optionalReplace;
					if (hasValue)
						optionalReplace = aText.substr(nextPos + 1, endPos - (nextPos + 1));
					std::string::size_type oldSize = endPos + 3 - startPos;
					aText.replace(startPos, oldSize, optionalReplace);
					optionalPos = startPos;
					if (aSpans)
					{
						for (neolib::string_spans::iterator i = aSpans->begin(); i != aSpans->end(); ++i)
						{
							if (i->first >= startPos)
							{
								if (oldSize > optionalReplace.size())
									i->first -= oldSize - optionalReplace.size();
								else
									i->first += optionalReplace.size() - oldSize;
							}
							if (i->second >= startPos)
							{
								if (oldSize > optionalReplace.size())
									i->second -= oldSize - optionalReplace.size();
								else
									i->second += optionalReplace.size() - oldSize;
							}
						}
					}
				}
			}
		}
		std::string::size_type startPos = std::string::npos;
		for (std::string::size_type pos = 0; pos != aText.size();)
		{
			char ch = aText[pos];
			if (ch == '%')
			{
				if (startPos == std::string::npos)
					startPos = pos++;
				else if (startPos == pos-1)
				{
					aText.erase(pos, 1);
					startPos = std::string::npos;
					if (aSpans)
					{
						for (neolib::string_spans::iterator i = aSpans->begin(); i != aSpans->end(); ++i)
						{
							if (i->first > pos)
								--i->first;
							if (i->second > pos)
								--i->second;
						}
					}
				}
				else
				{
					codes::const_iterator i = aCodes.find(aText.substr(startPos, pos - startPos + 1));
					if (i != aCodes.end())
					{
						std::string::size_type oldSize = pos - startPos + 1;
						const std::string& replacementString = i->second.first.is<std::string>() ? static_cast<const std::string&>(i->second.first) : static_cast<const std::tr1::function<std::string()>&>(i->second.first)();
						std::string::size_type newSize = replacementString.size();
						aText.replace(startPos, oldSize, replacementString);
						if (aSpans)
						{
							if (i->second.second && aSpans->empty())
								aSpans->push_back(neolib::string_span(startPos, startPos + newSize, i->second.second));
							else
							{
								for (neolib::string_spans::iterator si = aSpans->begin(); si != aSpans->end(); ++si)
								{
									if (si->first >= pos)
									{
										if (oldSize > newSize)
											si->first -= oldSize - newSize;
										else
											si->first += newSize - oldSize;
									}
									if (si->second >= pos)
									{
										if (oldSize > newSize)
											si->second -= oldSize - newSize;
										else
											si->second += newSize - oldSize;
									}
								}
							}
						}
						pos = startPos + newSize;
					}
					else
						++pos;
					startPos = std::string::npos;
				}
			}
			else
				++pos;
		}
	}
}