#pragma once

#include "JC/Core.h"

namespace JC::Fmt {

//--------------------------------------------------------------------------------------------------

inline void BadFmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace() {}
inline void BadFmtStr_NotEnoughArgs() {}
inline void BadFmtStr_CloseBraceNotEscaped() {}
inline void BadFmtStr_TooManyArgs() {}

consteval const char* CheckFmtSpec(const char* i) {
	for (Bool flagsDone = false; !flagsDone; ) {
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
	U32 nextArg = 0;

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

}	// namespace JC::Fmt