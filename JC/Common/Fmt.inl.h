#pragma once

#include "JC/Common.h"

namespace JC {

template <class T> struct Array;

//--------------------------------------------------------------------------------------------------

inline void BadFmtStr_UnmatchedOpenBrace() {}
inline void BadFmtStr_NotEnoughArgs() {}
inline void BadFmtStr_CloseBraceNotEscaped() {}
inline void BadFmtStr_TooManyArgs() {}
inline void BadFmtStr_BadPlaceholderSpec() {}

consteval const char* CheckFmtSpec(const char* i, const char* end) {
	bool flagsDone = false;
	while (!flagsDone && i < end) {
		switch (*i) {
			case '}': i++; return i;
			case '<': i++; break;
			case '+': i++; break;
			case ' ': i++; break;
			case '0': i++; flagsDone = true; break;
			default:       flagsDone = true; break;
		}
	}
	while (i < end && *i >= '0' && *i <= '9') {
		i++;
	}
	if (i < end && *i == '.') {
		i++;
		while (i < end && *i >= '0' && *i <= '9') {
			i++;
		}
	}
	if (i >= end) { BadFmtStr_BadPlaceholderSpec(); }

	switch (*i) {
		case 'x': i++; break;
		case 'X': i++; break;
		case 'b': i++; break;
		case 'f': i++; break;
		case 'e': i++; break;
		default:       break;
	}
	if (i >= end || *i != '}') { BadFmtStr_BadPlaceholderSpec(); }
	i++;
	return i;
}

consteval void CheckFmtStr(Str fmt, size_t argsLen) {
	const char* i = fmt.data;
	const char* const end = i + fmt.len;
	u32 nextArg = 0;

	for (;;) {
		if (i >= end) {
			if (nextArg != argsLen) { BadFmtStr_TooManyArgs(); }
			return;
		} else if (*i == '{') {
			i++;
			if (i >= end) { BadFmtStr_UnmatchedOpenBrace(); }
			if (*i == '{') {
				i++;
			} else {
				i = CheckFmtSpec(i, end);
				if (nextArg >= argsLen) { BadFmtStr_NotEnoughArgs(); }
				nextArg++;
			}
		} else if (*i != '}') {
			i++;
		} else {
			i++;
			if (i >= end || *i != '}') { BadFmtStr_CloseBraceNotEscaped(); }
			i++;
		}
	}
}

//--------------------------------------------------------------------------------------------------

template <class... A> struct _FmtStr {
	Str fmt;
	consteval _FmtStr(Str         fmtIn) { CheckFmtStr(fmtIn, sizeof...(A)); fmt = fmtIn; }
	consteval _FmtStr(const char* fmtIn) { CheckFmtStr(fmtIn, sizeof...(A)); fmt = fmtIn; }
	operator Str() const { return fmt; }
};

template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

template <class... A> struct _FmtStrSrcLoc {
	Str    fmt;
	SrcLoc sl;
	consteval _FmtStrSrcLoc(Str         fmtIn, SrcLoc slIn = SrcLoc::Here()) { CheckFmtStr(fmtIn, sizeof...(A)); fmt = fmtIn; sl = slIn; }
	consteval _FmtStrSrcLoc(const char* fmtIn, SrcLoc slIn = SrcLoc::Here()) { CheckFmtStr(fmtIn, sizeof...(A)); fmt = fmtIn; sl = slIn; }
	operator Str() const { return fmt; }
};

template <class... A> using FmtStrSrcLoc = _FmtStrSrcLoc<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, Str fmt, VArgs args);
void  VFmt(Array<char>* out, Str fmt, VArgs args);
Str   VFmt(Mem::Allocator* allocator, Str fmt, VArgs args);

Str   VTFmt(Str fmt, VArgs args, Mem::Allocator* allocator = context->tempAllocator);

template <class... A> char* Fmt(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) { return VFmt(outBegin, outEnd, fmt.fmt, MakeVArgs(args...)); }
template <class... A> void  Fmt(Array<char>* out,             FmtStr<A...> fmt, A... args) {        VFmt(out,              fmt.fmt, MakeVArgs(args...)); }
template <class... A> Str   Fmt(Mem::Allocator* allocator,    FmtStr<A...> fmt, A... args) { return VFmt(allocator,        fmt.fmt, MakeVArgs(args...)); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC