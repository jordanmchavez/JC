#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

struct ErrArg {
	Str name;
	Arg arg;
};

constexpr U32 Err_MaxArgs = 32;

struct ErrData {
	U64            frame;
	ErrData const* prev;
	SrcLoc         sl;
	Str            ns;
	Str            code;
	ErrArg         args[Err_MaxArgs];
	U32            argsLen;
};

const ErrData* Err_GetData(Err err);

void Err_Frame(U64 frame);

bool operator==(ErrCode ec, Err err);
bool operator==(Err err, ErrCode ec);