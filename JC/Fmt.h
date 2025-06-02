#pragma once

#include "JC/Core.h"

namespace JC { template <class T> struct Array; }

namespace JC::Fmt {

//--------------------------------------------------------------------------------------------------

char* VPrintf(char* outBegin, char* outEnd, const char* fmt, Span<const Arg> args);
void  VPrintf(Array<char>* out,             const char* fmt, Span<const Arg> args);
Str   VPrintf(Mem::Allocator* allocator,    const char* fmt, Span<const Arg> args);

template <class... A> char* Printf(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) { return VPrintf(outBegin, outEnd, fmt.fmt, { MakeArg(args)..., }); }
template <class... A> void  Printf(Array<char>* out,             FmtStr<A...> fmt, A... args) {        VPrintf(out,              fmt.fmt, { MakeArg(args)..., }); }
template <class... A> Str   Printf(Mem::Allocator* allocator,    FmtStr<A...> fmt, A... args) { return VPrintf(allocator,        fmt.fmt, { MakeArg(args)..., }); }
template <class... A> Str   Printfz(Mem::Allocator* allocator,   FmtStr<A...> fmt, A... args) {
	Array<char> out(allocator);
	VPrintf(&out, fmt.fmt, { MakeArg(args)..., });
	out.Add('\0');
	return Str(out.data, out.len);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Fmt