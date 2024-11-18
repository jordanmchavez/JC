#pragma once

#include "JC/Common.h"

namespace JC {

template <class T> struct Array;
struct Mem;

//--------------------------------------------------------------------------------------------------

Err* _VMakeErr(Mem* mem, Err* prev, SrcLoc sl, ErrCode ec, const s8* argNames, const Arg* argVals, u32 argsLen);

template <class T, class... A> static void _FillErrArgs(s8* argNames, Arg* argVals, s8 name, T val, A... args) {
	argNames[0] = name;
	argVals[0]  = Arg::Make(val);
	if constexpr (sizeof...(A) > 0) {
		_FillErrArgs(argNames + 1, argVals + 1, args...);
	}
}
template <class... A> Err* _MakeErr(Mem* mem, Err* prev, s8 file, i32 line, ErrCode ec, A... args) {
	static_assert(sizeof...(A) % 2 == 0);
	constexpr u32 ArgsLen = sizeof...(A) / 2;
	s8  argNames[ArgsLen > 0 ? ArgsLen : 1];
	Arg argVals[ArgsLen > 0 ? ArgsLen : 1];
	if constexpr (ArgsLen > 0) {
		_FillErrArgs(argNames, argVals, args...);
	}
	return _VMakeErr(mem, prev, file, line, ec, argNames, argVals, ArgsLen);
}

#define MakeErr(mem, ec, ...) \
	_MakeErr(mem, 0, __FILE__, __LINE__, ec, ##__VA_ARGS__)

s8 MakeErrStr(Err* err);
void AddErrStr(Err* err, Array<char>* arr);

//--------------------------------------------------------------------------------------------------

}	// namespace JC