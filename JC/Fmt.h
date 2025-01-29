#pragma once

#include "JC/Core.h"

namespace JC { template <class T> struct Array; }

namespace JC::Fmt {

//--------------------------------------------------------------------------------------------------

char* VPrintf(char* outBegin, char* outEnd, const char* fmt, Span<Arg> args);
void  VPrintf(Array<char>* out,             const char* fmt, Span<Arg> args);
Str   VPrintf(Mem::Allocator* allocator,    const char* fmt, Span<Arg> args);

template <class... A> char* Printf(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) { return VPrintf(outBegin, outEnd, fmt.fmt, Span<Arg>({ MakeArg(args)..., })); }
template <class... A> void  Printf(Array<char>* out,             FmtStr<A...> fmt, A... args) {        VPrintf(out,              fmt.fmt, Span<Arg>({ MakeArg(args)..., })); }
template <class... A> Str   Printf(Mem::Allocator* allocator,    FmtStr<A...> fmt, A... args) { return VPrintf(allocator,        fmt.fmt, Span<Arg>({ MakeArg(args)..., })); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Fmt