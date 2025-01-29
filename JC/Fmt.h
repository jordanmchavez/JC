#pragma once

#include "JC/Core.h"

namespace JC { template <class T> struct Array; }

namespace JC::Fmt {

//--------------------------------------------------------------------------------------------------

inline void BadFmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace() {}
inline void BadFmtStr_NotEnoughArgs() {}
inline void BadFmtStr_CloseBraceNotEscaped() {}
inline void BadFmtStr_TooManyArgs() {}

consteval const char* CheckFmtSpec(const char* i) {
	bool flagsDone = false;
	while (!flagsDone) {
		switch (*i) {
			case '}': i++; return i;
			case '<': i++; break;
			case '+': i++; break;
			case ' ': i++; break;
			case '0': i++; flagsDone = true; break;
			default:       flagsDone = true; break;
		}
	}
	while (*i >= '0' && *i <= '9') {
		i++;
	}
	if (*i == '.') {
		i++;
		while (*i >= '0' && *i <= '9') {
			i++;
		}
	}
	if (!*i) { BadFmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace(); }

	switch (*i) {
		case 'x': i++; break;
		case 'X': i++; break;
		case 'b': i++; break;
		case 'f': i++; break;
		case 'e': i++; break;
		default:       break;
	}
	if (*i != '}') { BadFmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace(); }
	i++;
	return i;
}

consteval void CheckFmtStr(const char* fmt, size_t argsLen) {
	const char* i = fmt;
	u32 nextArg = 0;

	for (;;) {
		if (!*i) {
			if (nextArg != argsLen) { BadFmtStr_TooManyArgs(); }
			return;
		}

		if (*i == '{') {
			i++;
			if (*i == '{') {
				i++;
			} else {
				i = CheckFmtSpec(i);
				if (nextArg >= argsLen) { BadFmtStr_NotEnoughArgs(); }
				nextArg++;
			}
		} else if (*i != '}') {
			i++;
		} else {
			i++;
			if (*i != '}') { BadFmtStr_CloseBraceNotEscaped(); }
			i++;
		}
	}
}

template <class... A> struct _FmtStr {
	const char* fmt;
	consteval _FmtStr(const char* fmtIn) { CheckFmtStr(fmtIn, sizeof...(A)); fmt = fmtIn; }
	operator const char*() const { return fmt; }
};
template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, const char* fmt, Span<Arg> args);
void  VFmt(Array<char>* out,             const char* fmt, Span<Arg> args);
Str   VFmt(Allocator* allocator,         const char* fmt, Span<Arg> args);

template <class... A> char* Fmt(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) { return VFmt(outBegin, outEnd, fmt.fmt, MakeVArgs(args...)); }
template <class... A> void  Fmt(Array<char>* out,             FmtStr<A...> fmt, A... args) {        VFmt(out,              fmt.fmt, MakeVArgs(args...)); }
template <class... A> Str   Fmt(Allocator* allocator,         FmtStr<A...> fmt, A... args) { return VFmt(allocator,        fmt.fmt, MakeVArgs(args...)); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Fmt