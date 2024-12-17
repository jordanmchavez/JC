#pragma once

#include "JC/Common.h"

namespace JC {

template <class T> struct Array;

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, s8 fmt, VArgs args);
void  VFmt(Array<char>* out, s8 fmt, VArgs args);
s8    VFmt(Arena* arena, s8 fmt, VArgs args);

template <class... A> char* Fmt(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) { return VFmt(outBegin, outEnd, fmt.fmt, MakeVArgs(args...)); }
template <class... A> void  Fmt(Array<char>* out,             FmtStr<A...> fmt, A... args) {        VFmt(out,              fmt.fmt, MakeVArgs(args...)); }
template <class... A> s8    Fmt(Arena* arena,                 FmtStr<A...> fmt, A... args) { return VFmt(arena,            fmt.fmt, MakeVArgs(args...)); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC