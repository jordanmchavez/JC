#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

struct Err_Arg {
	Str name;
	Arg arg;
};

constexpr U32 Err_MaxArgs = 32;

struct Err_Data {
	U64             frame;
	Err_Data const* prev;
	SrcLoc          sl;
	Str             ns;
	Str             sCode;
	U64             uCode;
	Err_Arg         args[Err_MaxArgs];
	U32             argsLen;
};

Err_Data const* Err_GetData(Err err);

void Err_Frame(U64 frame);

bool operator==(Err_Code ec, Err err);
bool operator==(Err err, Err_Code ec);