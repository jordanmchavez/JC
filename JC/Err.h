#pragma once

#include "JC/Common.h"

namespace JC {

template <class T> struct Array;
struct Mem;

//--------------------------------------------------------------------------------------------------

Err* _VMakeErr(Err* prev, SrcLoc sl, ErrCode ec, const s8* argNames, const Arg* args, u32 argsLen);

template <class T, class... A> static void _FillErrArgs(s8* argNames, Arg* args, s8 name, T val, A... a) {
	argNames[0] = name;
	args[0]  = Arg::Make(val);
	if constexpr (sizeof...(A) > 0) {
		_FillErrArgs(argNames + 1, args + 1, a...);
	}
}

template <class... A> Err* _MakeErr(Err* prev, SrcLoc sl, ErrCode ec, A... a) {
	static_assert(sizeof...(A) % 2 == 0);
	constexpr u32 ArgsLen = sizeof...(A) / 2;
	s8  argNames[ArgsLen > 0 ? ArgsLen : 1];
	Arg args[ArgsLen > 0 ? ArgsLen : 1];
	if constexpr (ArgsLen > 0) {
		_FillErrArgs(argNames, args, a...);
	}
	return _VMakeErr(prev, sl, ec, argNames, args, ArgsLen);
}

#define MakeErr(ec, ...) \
	_MakeErr(0, SrcLoc::Here(), ec, ##__VA_ARGS__)

s8 MakeErrStr(Err* err);
void AddErrStr(Err* err, Array<char>* arr);

//--------------------------------------------------------------------------------------------------

}	// namespace JC