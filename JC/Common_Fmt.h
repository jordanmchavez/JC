#pragma once

#include "JC/Common_Arg.h"
#include "JC/Common_Mem.h"
#include "JC/Common_SrcLoc.h"

namespace JC::Fmt {

//--------------------------------------------------------------------------------------------------

inline void CheckStr_TooManyArgs() {}
inline void CheckStr_NotEnoughArgs() {}
inline void CheckStr_t_Arg_NotBool() {}
inline void CheckStr_c_Arg_NotChar() {}
inline void CheckStr_s_Arg_NotStr() {}
inline void CheckStr_i_Arg_NotI64() {}
inline void CheckStr_u_Arg_NotU64() {}
inline void CheckStr_x_Arg_NotU64() {}
inline void CheckStr_X_Arg_NotU64() {}
inline void CheckStr_b_Arg_NotU64() {}
inline void CheckStr_f_Arg_NotF64() {}
inline void CheckStr_e_Arg_NotF64() {}
inline void CheckStr_E_Arg_NotF64() {}
inline void CheckStr_g_Arg_NotF64() {}
inline void CheckStr_p_Arg_NotPtr() {}
inline void CheckStr_UnknownArgType() {}

template <class T> struct TypeIdentity { using Type = T; };

template <class... A> struct _CheckStr {
	char const* fmt;

	consteval _CheckStr(char const* fmt_) {
		fmt = fmt_;
		Check<A...>();
	}

	operator char const*() const { return fmt; }

	template <class... A> consteval void Check() {
		constexpr Arg::Type argTypes[sizeof...(A) + 1] = { Arg::Make(A()).type... };

		U32 argIdx = 0;

		char const* f = fmt;
		for (;;) {
			while (*f != '%') {
				if (*f == 0) {
					if (argIdx < sizeof...(A)) { CheckStr_TooManyArgs(); }
					return;
				}
				f++;
			}
			f++;

			if (*f == '%') {
				f++;
				continue;
			}

			for (;;) {
				switch (*f) {
					case '-': f++; continue;
					case '+': f++; continue;
					case ' ': f++; continue;
					case '0': f++; continue;
				}
				break;
			}

			while (*f >= '0' && *f <= '9') {
				f++;
			}

			if (*f == '.') {
				f++;
				while (*f >= '0' && *f <= '9') {
					f++;
				}
			}

			if (argIdx >= sizeof...(A)) { CheckStr_NotEnoughArgs(); }
			switch (*f) {
				case 't': if (argTypes[argIdx] != Arg::Type::Bool) { CheckStr_t_Arg_NotBool(); } break;
				case 'c': if (argTypes[argIdx] != Arg::Type::Char) { CheckStr_c_Arg_NotChar(); } break;
				case 's': if (argTypes[argIdx] != Arg::Type::Str ) { CheckStr_s_Arg_NotStr(); } break;
				case 'i': if (argTypes[argIdx] != Arg::Type::I64 ) { CheckStr_i_Arg_NotI64(); } break;
				case 'u': if (argTypes[argIdx] != Arg::Type::U64 ) { CheckStr_u_Arg_NotU64(); } break;
				case 'x': if (argTypes[argIdx] != Arg::Type::U64 ) { CheckStr_x_Arg_NotU64(); } break;
				case 'X': if (argTypes[argIdx] != Arg::Type::U64 ) { CheckStr_X_Arg_NotU64(); } break;
				case 'b': if (argTypes[argIdx] != Arg::Type::U64 ) { CheckStr_b_Arg_NotU64(); } break;
				case 'f': if (argTypes[argIdx] != Arg::Type::F64 ) { CheckStr_f_Arg_NotF64(); } break;
				case 'e': if (argTypes[argIdx] != Arg::Type::F64 ) { CheckStr_e_Arg_NotF64(); } break;
				case 'E': if (argTypes[argIdx] != Arg::Type::F64 ) { CheckStr_E_Arg_NotF64(); } break;
				case 'g': if (argTypes[argIdx] != Arg::Type::F64 ) { CheckStr_g_Arg_NotF64(); } break;
				case 'p': if (argTypes[argIdx] != Arg::Type::Ptr ) { CheckStr_p_Arg_NotPtr(); } break;
				case 'a': break;
				default: CheckStr_UnknownArgType(); break;
			}
			argIdx++;
			f++;
		}
	}
};
template <class... A> using CheckStr = _CheckStr<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

Str Printv(Mem::Mem mem, char const* fmt, Span<Arg::Arg const> args);
char* Printv(char* outBegin,  char* outEnd, char const* fmt, Span<Arg::Arg const> args);

template <class... A> Str Printf(Mem::Mem mem, CheckStr<A...> fmt, A... args) {
	return Printv(mem, fmt.fmt, { Arg::Make(args)... });
}

template <class... A> char* Printf(char* outBegin, char* outEnd, CheckStr<A...> fmt, A... args) {
	return Printv(outBegin, outEnd, fmt.fmt, { Arg::Make(args)... });
}

//--------------------------------------------------------------------------------------------------

struct PrintBuf {
	Mem::Mem mem;
	char*    data;
	U64      len;
	U64      cap;

	PrintBuf(Mem::Mem mem);

	void Add(char c, SrcLoc sl = SrcLoc::Here());
	void Add(char c, U64 n, SrcLoc sl = SrcLoc::Here());
	void Add(char const* str, U64 strLen, SrcLoc sl = SrcLoc::Here());
	void Add(Str s, SrcLoc sl = SrcLoc::Here());
	void Remove();
	void Remove(U64 n);

	void Printv(char const* fmt, Span<Arg::Arg const> args);
	template <class... A> void Printf(Fmt::CheckStr<A...> fmt, A... args) { Printv(fmt, { Arg::Make(args)... }); }

	inline Str ToStr() const { return Str(data, len); }
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Fmt