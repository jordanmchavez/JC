#pragma once

#include "JC/Common.h"

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

enum struct Type {
	Null = 0,
	Bool,
	U64,
	F64,
	S8,
	Arr,
	Obj,
};

struct Elem { u64 handle = 0; };

struct NameVal {
	s8   name = {};
	Elem val  = {};
};

Res<Elem> Parse(Arena* arena, s8 str);
Type GetType(Elem elem);
Res<bool> GetBool(Elem elem);
Res<u64> GetU64(Elem elem);
Res<f64> GetF4(Elem elem);
Res<s8> GetS8(Elem elem);
Res<Span<Elem>> GetArr(Elem elem);
Res<Span<NameVal>> GetObj(Elem elem);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json