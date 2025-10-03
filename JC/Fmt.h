#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

Str   Fmt_Printv(Mem* mem,                      char const* fmt, Span<Arg const> args);
void  Fmt_Printv(Array<char>* arr,              char const* fmt, Span<Arg const> args);
char* Fmt_Printv(char* outBegin,  char* outEnd, char const* fmt, Span<Arg const> args);

template <class... A> Str Fmt_Printf(Mem* mem, FmtStr<A...> fmt, A... args) {
	return Fmt_Printv(mem, fmt.fmt, { Arg_Make(args)..., });
}

template <class... A> void Fmt_Printf(Array<char>* arr, FmtStr<A...> fmt, A... args) {
	Fmt_Printv(arr, fmt.fmt, { Arg_Make(args)..., });
}

template <class... A> char* Fmt_Printf(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args) {
	return Fmt_Printv(outBegin, outEnd, fmt.fmt, { Arg_Make(args)..., });
}