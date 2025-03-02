#pragma once

#include "JC/Core.h"

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

enum struct Type { Bool, I64, U64, F64, Str, Arr, Obj };

struct Doc;

struct Elem;

struct Obj {
	Span<Str>  keys;
	Span<Elem> vals;
};

Res<Doc*>        Parse(Mem::Allocator allocator, Str json);
void             Free(Doc* doc);

Elem             GetRoot(Doc* doc);
Type             GetType(Elem* elem);
Res<bool>        GetBool(Elem* elem);
Res<i64>         GetI64 (Elem* elem);
Res<u64>         GetU64 (Elem* elem);
Res<f64>         GetF64 (Elem* elem);
Res<Str>         GetStr (Elem* elem);
Res<Span<Elem*>> GetArr (Elem* elem);
Res<Obj>         GetObj (Elem* elem);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json