#pragma once

#include "JC/Core.h"

namespace JC { template <class T> struct Array; }

namespace JC::Fmt {

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, const char* fmt, Span<Arg> args);
void  VFmt(Array<char>* out,             const char* fmt, Span<Arg> args);
Str   VFmt(Mem::Allocator* allocator,    const char* fmt, Span<Arg> args);

template <class... A> char* Fmt(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) { return VFmt(outBegin, outEnd, fmt.fmt, Span<Arg>({ MakeArg(args)..., })); }
template <class... A> void  Fmt(Array<char>* out,             FmtStr<A...> fmt, A... args) {        VFmt(out,              fmt.fmt, Span<Arg>({ MakeArg(args)..., })); }
template <class... A> Str   Fmt(Mem::Allocator* allocator,    FmtStr<A...> fmt, A... args) { return VFmt(allocator,        fmt.fmt, Span<Arg>({ MakeArg(args)..., })); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Fmt