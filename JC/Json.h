#pragma once

#include "JC/Core.h"

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

namespace Type {
	constexpr u32 Bool = 1 << 0;
	constexpr u32 I64  = 1 << 1;
	constexpr u32 F64  = 1 << 3;
	constexpr u32 Str  = 1 << 4;
	constexpr u32 Arr  = 1 << 5;
	constexpr u32 Obj  = 1 << 6;
};

struct Doc;

struct Elem {
	u32 type   : 3;
	u32 offset : 29;
};

struct Obj {
	Span<Elem> keys;
	Span<Elem> vals;
};

Res<Doc*>  Parse(Mem::Allocator* allocator, Str json);
void       Free(Doc* doc);

Elem       GetRoot(Doc* doc);
bool       GetBool(const Doc* doc, Elem elem);
i64        GetI64 (const Doc* doc, Elem elem);
u64        GetU64 (const Doc* doc, Elem elem);
f64        GetF64 (const Doc* doc, Elem elem);
Str        GetStr (const Doc* doc, Elem elem);
Span<Elem> GetArr (const Doc* doc, Elem elem);
Obj        GetObj (const Doc* doc, Elem elem);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json