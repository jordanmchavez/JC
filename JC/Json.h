#pragma once

#include "JC/Core.h"

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

enum struct Type {
	Bool,
	I64,
	F64,
	Str,
	Arr,
	Obj,
};

struct Elem { u64 handle = 0; };

struct KeyVal {
	Str  key = {};
	Elem val = {};
};

Res<Elem>         Parse(Mem::Allocator allocator, Mem::TempAllocator* tempAllocator, Str str);
Type              GetType(Elem elem);
Res<bool>         GetBool(Elem elem);
Res<i64>          GetI64(Elem elem);
Res<f64>          GetF64(Elem elem);
Res<Str>          GetStr(Elem elem);
Res<Span<Elem>>   GetArr(Elem elem);
Res<Span<KeyVal>> GetObj(Elem elem);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json