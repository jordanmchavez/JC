#pragma once

#include "JC/Common.h"

namespace JC {

template <class T> struct Array;
struct Allocator;

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, s8 fmt, Args args);
void  VFmt(Array<char>* out, s8 fmt, Args args);
s8    VFmt(Allocator* allocator, s8 fmt, Args args);

template <class... A> char* Fmt(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) { return VFmt(outBegin, outEnd, fmt.fmt, Args::Make(args...)); }
template <class... A> void  Fmt(Array<char>* out,             FmtStr<A...> fmt, A... args) {        VFmt(out,              fmt.fmt, Args::Make(args...)); }
template <class... A> s8    Fmt(Allocator* allocator,         FmtStr<A...> fmt, A... args) { return VFmt(allocator,        fmt.fmt, Args::Make(args...)); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC