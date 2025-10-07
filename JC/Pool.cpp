#include "JC/Pool.h"
#include "JC/Unit.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Foo { Str name = {}; };
struct FooHandle { U64 handle = 0; };

Unit_Test("HandlePool") {
	using FooPool = HandlePool<Foo, FooHandle, 16>;
	FooPool fp;

	FooPool::Entry const* e1 = fp.Alloc(); Unit_CheckEq(e1->Handle().handle, ((U64)1 << 32) | 1); Unit_CheckEq(&e1->obj, &fp.entries[1].obj);
	FooPool::Entry const* e2 = fp.Alloc(); Unit_CheckEq(e2->Handle().handle, ((U64)2 << 32) | 2); Unit_CheckEq(&e2->obj, &fp.entries[2].obj);
	FooPool::Entry const* e3 = fp.Alloc(); Unit_CheckEq(e3->Handle().handle, ((U64)3 << 32) | 3); Unit_CheckEq(&e3->obj, &fp.entries[3].obj);
	FooPool::Entry const* e4 = fp.Alloc(); Unit_CheckEq(e4->Handle().handle, ((U64)4 << 32) | 4); Unit_CheckEq(&e4->obj, &fp.entries[4].obj);
	FooPool::Entry const* e5 = fp.Alloc(); Unit_CheckEq(e5->Handle().handle, ((U64)5 << 32) | 5); Unit_CheckEq(&e5->obj, &fp.entries[5].obj);

	fp.Free(e1->Handle());
	fp.Free(e2->Handle());
	fp.Free(e4->Handle());
	fp.Free(e5->Handle());

	FooPool::Entry const* e6 = fp.Alloc(); Unit_CheckEq(e6->Handle().handle, ((U64)6 << 32) | 5); Unit_CheckEq(&e6->obj, &fp.entries[5].obj); 
	FooPool::Entry const* e7 = fp.Alloc(); Unit_CheckEq(e7->Handle().handle, ((U64)7 << 32) | 4); Unit_CheckEq(&e7->obj, &fp.entries[4].obj); 
	FooPool::Entry const* e8 = fp.Alloc(); Unit_CheckEq(e8->Handle().handle, ((U64)8 << 32) | 2); Unit_CheckEq(&e8->obj, &fp.entries[2].obj); 

	Unit_CheckEq(fp.entries[1].gen, 0u); Unit_CheckEq(fp.entries[1].idx, 0u);
	Unit_CheckEq(fp.entries[2].gen, 8u); Unit_CheckEq(fp.entries[2].idx, 2u);
	Unit_CheckEq(fp.entries[3].gen, 3u); Unit_CheckEq(fp.entries[3].idx, 3u);
	Unit_CheckEq(fp.entries[4].gen, 7u); Unit_CheckEq(fp.entries[4].idx, 4u);
	Unit_CheckEq(fp.entries[5].gen, 6u); Unit_CheckEq(fp.entries[5].idx, 5u);

	Unit_CheckEq(fp.free, 1u);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC