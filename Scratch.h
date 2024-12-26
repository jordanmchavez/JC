#include "JC/Common.h"

#include "JC/Array.h"
#include "JC/Math.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace Json {

struct Elem { u32 handle = 0; };

Res<Elem> Parse(Arena* arena, s8 str);

Res<u64> GetU64(Elem elem);
Res<f64> GetF4(Elem elem);
Res<s8> GetS8(Elem elem);
Res<Span<Elem>> GetArray(Elem elem);

struct Obj {
	Span<s8> names;
	Span<Elem> vals;
};
Res<Obj> GetObject(Elem elem);

enum Type {
	U64,
	F64,
	S8,
	Array,
	Object,
};

struct Elem {
	Type type;
	union {
		u64 u64Val;
		f64 f64Val;
		s8  s8Val;
	};
};

Elem* elems;

}

/*
[ 1, 2, 3 ]

[
	{ id=1, name="tree", u1=0.0, u2=id, name, uv1, uv2
]

512x512 32x32 -> 16 across, 16 bottom
*/

}

//--------------------------------------------------------------------------------------------------

}	// namespace JC