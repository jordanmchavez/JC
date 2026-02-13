#include "JC/HandlePool.h"
#include "JC/Unit.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Foo { Str name; };
DefHandle(FooHandle);

Unit_Test("HandlePool") {
	using FooPool = HandlePool<Foo, FooHandle>;
	FooPool fp;
	fp.Init(testMem, 16);

	FooPool::Entry const* e0 = fp.Alloc(); Unit_CheckEq(e0->idx, 0u); Unit_CheckEq(e0->gen, 1u); Unit_CheckEq(&e0->obj, &fp.entries[0].obj);
	FooPool::Entry const* e1 = fp.Alloc(); Unit_CheckEq(e1->idx, 1u); Unit_CheckEq(e1->gen, 2u); Unit_CheckEq(&e1->obj, &fp.entries[1].obj);
	FooPool::Entry const* e2 = fp.Alloc(); Unit_CheckEq(e2->idx, 2u); Unit_CheckEq(e2->gen, 3u); Unit_CheckEq(&e2->obj, &fp.entries[2].obj);
	FooPool::Entry const* e3 = fp.Alloc(); Unit_CheckEq(e3->idx, 3u); Unit_CheckEq(e3->gen, 4u); Unit_CheckEq(&e3->obj, &fp.entries[3].obj);
	FooPool::Entry const* e4 = fp.Alloc(); Unit_CheckEq(e4->idx, 4u); Unit_CheckEq(e4->gen, 5u); Unit_CheckEq(&e4->obj, &fp.entries[4].obj);

	fp.Free(e1->Handle());
	fp.Free(e2->Handle());
	fp.Free(e3->Handle());

	Unit_CheckEq(fp.free, 3u);

	FooPool::Entry const* e5 = fp.Alloc(); Unit_CheckEq(e5->idx, 3u); Unit_CheckEq(e5->gen, 6u); Unit_CheckEq(&e5->obj, &fp.entries[3].obj); 
	FooPool::Entry const* e6 = fp.Alloc(); Unit_CheckEq(e6->idx, 2u); Unit_CheckEq(e6->gen, 7u); Unit_CheckEq(&e6->obj, &fp.entries[2].obj); 
	FooPool::Entry const* e7 = fp.Alloc(); Unit_CheckEq(e7->idx, 1u); Unit_CheckEq(e7->gen, 8u); Unit_CheckEq(&e7->obj, &fp.entries[1].obj); 
	FooPool::Entry const* e8 = fp.Alloc(); Unit_CheckEq(e8->idx, 5u); Unit_CheckEq(e8->gen, 9u); Unit_CheckEq(&e8->obj, &fp.entries[5].obj); 

	Unit_CheckEq(fp.free, 0u);

	Unit_CheckEq(fp.entries[0].idx, 0u); Unit_CheckEq(fp.entries[0].gen, 1u);
	Unit_CheckEq(fp.entries[1].idx, 1u); Unit_CheckEq(fp.entries[1].gen, 8u);
	Unit_CheckEq(fp.entries[2].idx, 2u); Unit_CheckEq(fp.entries[2].gen, 7u);
	Unit_CheckEq(fp.entries[3].idx, 3u); Unit_CheckEq(fp.entries[3].gen, 6u);
	Unit_CheckEq(fp.entries[4].idx, 4u); Unit_CheckEq(fp.entries[4].gen, 5u);
	Unit_CheckEq(fp.entries[5].idx, 5u); Unit_CheckEq(fp.entries[5].gen, 9u);
	Unit_CheckEq(fp.entries[6].idx, 0u); Unit_CheckEq(fp.entries[6].gen, 0u);
	Unit_CheckEq(fp.entries[7].idx, 0u); Unit_CheckEq(fp.entries[7].gen, 0u);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC