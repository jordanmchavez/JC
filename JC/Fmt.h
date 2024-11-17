#pragma once

#include "JC/Common.h"

namespace JC {

template <class T> struct Array;

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, s8 fmt, Args args);
void  VFmt(Array<char>* out, s8 fmt, Args args);
s8    VFmt(Mem* mem, s8 fmt, Args args);

template <class... A> char* Fmt(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) { return VFmt(outBegin, outEnd, fmt.fmt, Args::Make(args...)); }
template <class... A> void  Fmt(Array<char>* out,             FmtStr<A...> fmt, A... args) {        VFmt(out,              fmt.fmt, Args::Make(args...)); }
template <class... A> s8    Fmt(Mem* mem,                     FmtStr<A...> fmt, A... args) { return VFmt(mem,              fmt.fmt, Args::Make(args...)); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC