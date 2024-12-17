#include "JC/HandleArray.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Foo { s8 name = {}; };
struct FooHandle { u64 handle = 0; };

UnitTest("HandleArray") {
	HandleArray<Foo, FooHandle> ha;
	ha.Init(temp);

	Foo* const f1 = ha.Alloc(); CheckEq(f1, &ha.entries[1].val); CheckEq(ha.GetHandle(f1).handle, ((u64)1 << 32) | 1);
	Foo* const f2 = ha.Alloc(); CheckEq(f2, &ha.entries[2].val); CheckEq(ha.GetHandle(f2).handle, ((u64)2 << 32) | 2);
	Foo* const f3 = ha.Alloc(); CheckEq(f3, &ha.entries[3].val); CheckEq(ha.GetHandle(f3).handle, ((u64)3 << 32) | 3);
	Foo* const f4 = ha.Alloc(); CheckEq(f4, &ha.entries[4].val); CheckEq(ha.GetHandle(f4).handle, ((u64)4 << 32) | 4);
	Foo* const f5 = ha.Alloc(); CheckEq(f5, &ha.entries[5].val); CheckEq(ha.GetHandle(f5).handle, ((u64)5 << 32) | 5);

	ha.Free(f1);
	ha.Free(f2);
	ha.Free(f4);
	ha.Free(f5);

	Foo* const f6 = ha.Alloc(); CheckEq(f6, &ha.entries[5].val); CheckEq(ha.GetHandle(f6).handle, ((u64)6 << 32) | 5);
	Foo* const f7 = ha.Alloc(); CheckEq(f7, &ha.entries[4].val); CheckEq(ha.GetHandle(f7).handle, ((u64)7 << 32) | 4);
	Foo* const f8 = ha.Alloc(); CheckEq(f8, &ha.entries[2].val); CheckEq(ha.GetHandle(f8).handle, ((u64)8 << 32) | 2);

	CheckEq(ha.entries[1].gen, 0u); CheckEq(ha.entries[1].idx, 0u);
	CheckEq(ha.entries[2].gen, 8u); CheckEq(ha.entries[2].idx, 2u);
	CheckEq(ha.entries[3].gen, 3u); CheckEq(ha.entries[3].idx, 3u);
	CheckEq(ha.entries[4].gen, 7u); CheckEq(ha.entries[4].idx, 4u);
	CheckEq(ha.entries[5].gen, 6u); CheckEq(ha.entries[5].idx, 5u);

	CheckEq(ha.free, 1u);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC