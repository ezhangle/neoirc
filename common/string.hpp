// string.h
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

#ifndef IRC_STRING
#define IRC_STRING

#include <neolib/neolib.hpp>
#include <string>
#include <vector>

namespace irc
{
	namespace casemapping
	{
		enum type
		{
			ascii,
			rfc1459,
			strict_rfc1459
		};

		template <typename CharT>
		inline CharT tolower(type cmt, int c, CharT=0) 
		{	
			// WARNING: ASCII encoding assumed here
			if (c >= 'A' && c <= 'Z')
				return c + ('a' - 'A');
			switch(cmt)
			{
			default:
			case ascii:
				return c; 
			case rfc1459:
				switch(c)
				{
				case '{':
					return '[';
				case '}':
					return ']';
				case '|':
					return '\\';
				case '^':
					return '~';
				default:
					return c; 
				}
			case strict_rfc1459:
				switch(c)
				{
				case '{':
					return '[';
				case '}':
					return ']';
				case '|':
					return '\\';
				default:
					return c; 
				}
			}
		}
		template <>
		inline wchar_t tolower<wchar_t>(type cmt, int c, wchar_t) 
		{ 
			// WARNING: ASCII encoding assumed here
			if (c >= L'A' && c <= L'Z')
				return c + (L'a' - L'A');
			switch(cmt)
			{
			default:
			case ascii:
				return c; 
			case rfc1459:
				switch(c)
				{
				case L'{':
					return L'[';
				case L'}':
					return L']';
				case L'|':
					return L'\\';
				case L'^':
					return L'~';
				default:
					return c; 
				}
			case strict_rfc1459:
				switch(c)
				{
				case L'{':
					return L'[';
				case L'}':
					return L']';
				case L'|':
					return L'\\';
				default:
					return c; 
				}
			}
		}
		struct stack : std::vector<type>
		{
			stack()
			{
				push_back(rfc1459);
			}
		};
	}

	namespace
	{
		irc::casemapping::stack sCasemappingStack;
	}

	template <typename Traits>
	struct irc_char_traits : Traits
	{
	public:
		typedef typename Traits::char_type char_type;
		typedef typename Traits::int_type int_type;
	public:
		static irc::casemapping::type casemapping() { return sCasemappingStack.back(); }
		static void push(irc::casemapping::type cmt) { sCasemappingStack.push_back(cmt); }
		static void pop() { sCasemappingStack.pop_back(); }
		struct pusher
		{
			pusher(irc::casemapping::type cmt) { irc_char_traits<Traits>::push(cmt); }
			~pusher() { irc_char_traits<Traits>::pop(); }
		};
		static int compare(const char_type* s1, const char_type* s2, std::size_t n)
		{
			for(;n-- > 0;++s1, ++s2)
			{
				if (eq(*s1, *s2))
					continue;
				else if (lt(*s1, *s2))
					return -1;
				else
					return 1;
			}
			return 0;
		}
		static const char_type* find(const char_type* str, std::size_t n, const char_type& c)
		{
			while(n > 0)
			{
				if (eq(*str, c))
					return str;
				++str;
				--n;	
			}
			return 0;
		}
		static bool eq(const char_type& c1, const char_type& c2)
		{
			return lower(c1) == lower(c2);
		}
		static bool lt(const char_type& c1, const char_type& c2)
		{
			return lower(c1) < lower(c2);
		}
		static int_type lower(char_type c) 
		{
			return irc::casemapping::tolower<char_type>(casemapping(), to_int_type(c), char_type());
		}
	};

	template <class CharT, class Traits, class Alloc = std::allocator<CharT> >
	class basic_irc_string : public std::basic_string<CharT, Traits, Alloc>
	{
	// types
	private:
		typedef std::basic_string<CharT, Traits, Alloc> base_type;
		typedef basic_irc_string<CharT, Traits, Alloc> my_type;
	public:
		typedef typename base_type::size_type size_type;
	// construction
	public:
		explicit basic_irc_string(casemapping::type cmt, const Alloc& a = Alloc()) : base_type(a), iCasemapping(cmt) {}
		template <class InputIterator>
		basic_irc_string(casemapping::type cmt, InputIterator begin, InputIterator end, const Alloc& a = Alloc()) : base_type(begin, end, a), iCasemapping(cmt) {}
	// operations
	public:
		irc::casemapping::type casemapping() const { return iCasemapping; }
		size_type find(const my_type& _X, size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find(_X, _P);
		}
		size_type find(const CharT *_S, size_type _P, size_type _N) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find(_S, _P, _N);
		}
		size_type find(const CharT *_S, size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find(_S, _P);
		}
		size_type find(CharT _C, size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find(_C, _P);
		}
		size_type rfind(const my_type& _X, size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::rfind(_X, _P);
		}
		size_type rfind(const CharT *_S, size_type _P,	size_type _N) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::rfind(_S, _P,	_N);
		}
		size_type rfind(const CharT *_S, size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::rfind(_S, _P);
		}
		size_type rfind(CharT _C, size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::rfind(_C, _P);
		}
		size_type find_first_of(const my_type& _X,	size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_first_of(_X,	_P);
		}
		size_type find_first_of(const CharT *_S, size_type _P,	size_type _N) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_first_of(_S, _P,	_N);
		}
		size_type find_first_of(const CharT *_S, size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_first_of(_S, _P);
		}
		size_type find_first_of(CharT _C, size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_first_of(_C, _P);
		}
		size_type find_last_of(const my_type& _X, size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_last_of(_X, _P);
		}
		size_type find_last_of(const CharT *_S, size_type _P, size_type _N) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_last_of(_S, _P, _N);
		}
		size_type find_last_of(const CharT *_S,	size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_last_of(_S,	_P);
		}
		size_type find_last_of(CharT _C, size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_last_of(_C, _P);
		}
		size_type find_first_not_of(const my_type& _X, size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_first_not_of(_X, _P);
		}
		size_type find_first_not_of(const CharT*_S, size_type _P, size_type _N) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_first_not_of(_S, _P, _N);
		}
		size_type find_first_not_of(const CharT *_S, size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_first_not_of(_S, _P);
		}
		size_type find_first_not_of(CharT _C, size_type _P = 0) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_first_not_of(_C, _P);
		}
		size_type find_last_not_of(const my_type& _X, size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_last_not_of(_X, _P);
		}
		size_type find_last_not_of(const CharT *_S, size_type _P, size_type _N) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_last_not_of(_S, _P, _N);
		}
		size_type find_last_not_of(const CharT *_S, size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_last_not_of(_S, _P);
		}
		size_type find_last_not_of(CharT _C, size_type _P = npos) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::find_last_not_of(_C, _P);
		}
		int compare(const my_type& _X) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::compare(_X);
		}
		int compare(size_type _P0, size_type _N0, const my_type& _X) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::compare(_P0, _N0, _X);
		}
		int compare(size_type _P0, size_type _N0, const my_type& _X, size_type _P, size_type _M) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::compare(_P0, _N0, _X, _P, _M);
		}
		int compare(const CharT *_S) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::compare(_S);
		}
		int compare(size_type _P0, size_type _N0, const CharT *_S) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::compare(_P0, _N0, _S);
		}
		int compare(size_type _P0, size_type _N0, const CharT *_S,	size_type _M) const
		{
			typename Traits::pusher context(casemapping());
			return base_type::compare(_P0, _N0, _S,	_M);
		}
	// attributes
	private:
		casemapping::type iCasemapping;
	};

	typedef basic_irc_string<char, irc_char_traits<std::char_traits<char> > > string;
	typedef basic_irc_string<wchar_t, irc_char_traits<std::char_traits<wchar_t> > > wstring;

	template<class CharT, class Traits, class Alloc> inline
	string make_string(casemapping::type cmt, const std::basic_string<CharT, Traits, Alloc>& s)
	{
		return string(cmt, s.begin(), s.end());
	}
	template<class Traits, class Alloc> inline
	wstring make_string(casemapping::type cmt, const std::basic_string<wchar_t, Traits, Alloc>& s)
	{
		return wstring(cmt, s.begin(), s.end());
	}

	template<class CharT, class Traits, class Alloc> inline
		bool operator==(const basic_irc_string<CharT, Traits, Alloc>& left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (left.compare(right) == 0); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator==(const CharT * left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (right.compare(left) == 0); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator==(const basic_irc_string<CharT, Traits, Alloc>& left,
			const CharT *right)
		{return (left.compare(right) == 0); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator!=(const basic_irc_string<CharT, Traits, Alloc>& left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (!(left == right)); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator!=(const CharT *left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (!(left == right)); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator!=(const basic_irc_string<CharT, Traits, Alloc>& left,
			const CharT *right)
		{return (!(left == right)); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator<(const basic_irc_string<CharT, Traits, Alloc>& left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (left.compare(right) < 0); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator<(const CharT * left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (right.compare(left) > 0); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator<(const basic_irc_string<CharT, Traits, Alloc>& left,
			const CharT *right)
		{return (left.compare(right) < 0); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator>(const basic_irc_string<CharT, Traits, Alloc>& left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (right < left); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator>(const CharT * left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (right < left); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator>(const basic_irc_string<CharT, Traits, Alloc>& left,
			const CharT *right)
		{return (right < left); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator<=(const basic_irc_string<CharT, Traits, Alloc>& left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (!(right < left)); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator<=(const CharT * left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (!(right < left)); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator<=(const basic_irc_string<CharT, Traits, Alloc>& left,
			const CharT *right)
		{return (!(right < left)); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator>=(const basic_irc_string<CharT, Traits, Alloc>& left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (!(left < right)); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator>=(const CharT * left,
			const basic_irc_string<CharT, Traits, Alloc>& right)
		{return (!(left < right)); }
	template<class CharT, class Traits, class Alloc> inline
		bool operator>=(const basic_irc_string<CharT, Traits, Alloc>& left,
			const CharT *right)
		{return (!(left < right)); }

	inline std::string make_string(const string& s)
	{
		return std::string(s.begin(), s.end());
	}
	inline std::wstring make_string(const wstring& s)
	{
		return std::wstring(s.begin(), s.end());
	}

	inline bool operator==(const string& s1, const std::string& s2)
	{
		return s1 == string(s1.casemapping(), s2.begin(), s2.end());
	}
	inline bool operator==(const std::string& s1, const string& s2)
	{
		return string(s2.casemapping(), s1.begin(), s1.end()) == s2;
	}
	inline bool operator==(const wstring& s1, const std::wstring& s2)
	{
		return s1 == wstring(s1.casemapping(), s2.begin(), s2.end());
	}
	inline bool operator==(const std::wstring& s1, const wstring& s2)
	{
		return wstring(s2.casemapping(), s1.begin(), s1.end()) == s2;
	}
	inline bool operator!=(const string& s1, const std::string& s2)
	{
		return s1 != string(s1.casemapping(), s2.begin(), s2.end());
	}
	inline bool operator!=(const std::string& s1, const string& s2)
	{
		return string(s2.casemapping(), s1.begin(), s1.end()) != s2;
	}
	inline bool operator!=(const wstring& s1, const std::wstring& s2)
	{
		return s1 != wstring(s1.casemapping(), s2.begin(), s2.end());
	}
	inline bool operator!=(const std::wstring& s1, const wstring& s2)
	{
		return wstring(s2.casemapping(), s1.begin(), s1.end()) != s2;
	}
	inline bool operator<(const string& s1, const std::string& s2)
	{
		return s1 < string(s1.casemapping(), s2.begin(), s2.end());
	}
	inline bool operator<(const std::string& s1, const string& s2)
	{
		return string(s2.casemapping(), s1.begin(), s1.end()) < s2;
	}
	inline bool operator<(const wstring& s1, const std::wstring& s2)
	{
		return s1 < wstring(s1.casemapping(), s2.begin(), s2.end());
	}
	inline bool operator<(const std::wstring& s1, const wstring& s2)
	{
		return wstring(s2.casemapping(), s1.begin(), s1.end()) < s2;
	}
	inline bool operator>(const string& s1, const std::string& s2)
	{
		return s1 > string(s1.casemapping(), s2.begin(), s2.end());
	}
	inline bool operator>(const std::string& s1, const string& s2)
	{
		return string(s2.casemapping(), s1.begin(), s1.end()) > s2;
	}
	inline bool operator>(const wstring& s1, const std::wstring& s2)
	{
		return s1 > wstring(s1.casemapping(), s2.begin(), s2.end());
	}
	inline bool operator>(const std::wstring& s1, const wstring& s2)
	{
		return wstring(s2.casemapping(), s1.begin(), s1.end()) > s2;
	}
}

#endif // IRC_STRING