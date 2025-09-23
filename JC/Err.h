#pragma once

#include "JC/Common.h"

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

struct NamedArg {
	Str name;
	Arg arg;
};

constexpr U32 MaxNamedArgs = 32;

struct Data {
	U64         frame;
	const Data* prev;
	SrcLoc      sl;
	Str         ns;
	Str         code;
	NamedArg    namedArgs[MaxNamedArgs];
	U32         namedArgsLen;
};

const Data* GetData(Err err);

void Frame(U64 frame);

bool operator==(Code ec, Err err);
bool operator==(Err err, Code ec);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err