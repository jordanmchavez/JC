#pragma once

#include "JC/Common.h"

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

enum struct Type {
	Bool,
	I64,
	F64,
	S8,
	Arr,
	Obj,
};

struct Elem { u64 handle = 0; };

struct KeyVal {
	s8   key = {};
	Elem val = {};
};

Res<Elem> Parse(Arena* perm, Arena* temp, s8 str);
Type GetType(Elem elem);
Res<bool> GetBool(Elem elem);
Res<i64> GetI64(Elem elem);
Res<f64> GetF64(Elem elem);
Res<s8> GetS8(Elem elem);
Res<Span<Elem>> GetArr(Elem elem);
Res<Span<KeyVal>> GetObj(Elem elem);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json