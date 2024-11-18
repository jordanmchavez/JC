#pragma once

#include "JC/Common.h"

namespace JC {

template <class T> struct Array;
struct ErrArg;
struct Mem;

//--------------------------------------------------------------------------------------------------

Err* _VMakeErr(Mem* mem, Err* prev, s8 file, i32 line, ErrCode ec, const ErrArg* errArgs, u32 errArgsLen);

template <class T, class... A> static void _FillErrArgs(ErrArg* errArgs, s8 name, T arg, A... args) {
	errArgs[0] = { .name = name, .arg = Arg::Make(arg) };
	if constexpr (sizeof...(A) > 0) {
		_FillErrArgs(&errArgs[1], args...);
	}
}
template <class... A> Err* _MakeErr(Mem* mem, Err* prev, s8 file, i32 line, ErrCode ec, A... args) {
	static_assert(sizeof...(A) % 2 == 0);
	constexpr u32 ErrArgsLen = sizeof...(A) / 2;
	ErrArg errArgs[ErrArgsLen > 0 ? ErrArgsLen : 1];
	if constexpr (ErrArgsLen > 0) {
		_FillErrArgs(errArgs, args...);
	}
	return _VMakeErr(mem, prev, file, line, ec, errArgs, ErrArgsLen);
}

#define MakeErr(mem, ec, ...) \
	_MakeErr(mem, nullptr, __FILE__, __LINE__, ec, ##__VA_ARGS__)

void ErrStr(Err* err, Array<char>* out);

//--------------------------------------------------------------------------------------------------

}	// namespace JC