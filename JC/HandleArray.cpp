#include "JC/HandleArray.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Foo { s8 name = {}; };
struct FooHandle { u64 handle = 0; };

UnitTest("HandleArray") {
	HandleArray<Foo, FooHandle> ha;
	ha.Init(temp);

	const FooHandle fh1 = ha.Alloc(); CheckEq(fh1.handle, ((u64)1 << 32) | 1); CheckEq(ha.Get(fh1), &ha.entries[1].obj);
	const FooHandle fh2 = ha.Alloc(); CheckEq(fh2.handle, ((u64)2 << 32) | 2); CheckEq(ha.Get(fh2), &ha.entries[2].obj);
	const FooHandle fh3 = ha.Alloc(); CheckEq(fh3.handle, ((u64)3 << 32) | 3); CheckEq(ha.Get(fh3), &ha.entries[3].obj);
	const FooHandle fh4 = ha.Alloc(); CheckEq(fh4.handle, ((u64)4 << 32) | 4); CheckEq(ha.Get(fh4), &ha.entries[4].obj);
	const FooHandle fh5 = ha.Alloc(); CheckEq(fh5.handle, ((u64)5 << 32) | 5); CheckEq(ha.Get(fh5), &ha.entries[5].obj);

	ha.Free(fh1);
	ha.Free(fh2);
	ha.Free(fh4);
	ha.Free(fh5);

	const FooHandle fh6 = ha.Alloc(); CheckEq(fh6.handle, ((u64)6 << 32) | 5); CheckEq(ha.Get(fh6), &ha.entries[5].obj); 
	const FooHandle fh7 = ha.Alloc(); CheckEq(fh7.handle, ((u64)7 << 32) | 4); CheckEq(ha.Get(fh7), &ha.entries[4].obj); 
	const FooHandle fh8 = ha.Alloc(); CheckEq(fh8.handle, ((u64)8 << 32) | 2); CheckEq(ha.Get(fh8), &ha.entries[2].obj); 

	CheckEq(ha.entries[1].gen, 0u); CheckEq(ha.entries[1].idx, 0u);
	CheckEq(ha.entries[2].gen, 8u); CheckEq(ha.entries[2].idx, 2u);
	CheckEq(ha.entries[3].gen, 3u); CheckEq(ha.entries[3].idx, 3u);
	CheckEq(ha.entries[4].gen, 7u); CheckEq(ha.entries[4].idx, 4u);
	CheckEq(ha.entries[5].gen, 6u); CheckEq(ha.entries[5].idx, 5u);

	CheckEq(ha.free, 1u);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC