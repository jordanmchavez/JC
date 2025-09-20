#pragma once

#include "JC/Core.h"

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

namespace Type {
	constexpr U32 Bool = 0;
	constexpr U32 I64  = 1;
	constexpr U32 F64  = 2;
	constexpr U32 Str  = 3;
	constexpr U32 Arr  = 4;
	constexpr U32 Obj  = 5;
};

struct Doc;

struct Elem {
	U32 type   : 3;
	U32 offset : 29;
};

struct Obj {
	Span<Elem> keys;
	Span<Elem> vals;
};

Res<Doc*>  Parse(Allocator* allocator, TempAllocator* tempAllocator, Str json);
void       Free(Doc* doc);

Elem       GetRoot(Doc* doc);
Bool       GetBool(const Doc* doc, Elem elem);
I64        GetI64 (const Doc* doc, Elem elem);
U64        GetU64 (const Doc* doc, Elem elem);
F64        GetF64 (const Doc* doc, Elem elem);
Str        GetStr (const Doc* doc, Elem elem);
Span<Elem> GetArr (const Doc* doc, Elem elem);
Obj        GetObj (const Doc* doc, Elem elem);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json