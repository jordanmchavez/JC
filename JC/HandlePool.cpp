#include "JC/Handle.h"
#include "JC/Unit.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Foo { Str name = {}; };
DefHandle(FooHandle);

Unit_Test("HandleArray") {
	using FooPool = HandleArray<Foo, FooHandle>;
	FooPool fp;
	fp.Init(testMem, 16);

	FooPool::Entry const* e1 = fp.Alloc(); Unit_CheckEq(e1->Handle().idx, (U64)1); Unit_CheckEq(e1->Handle().gen, (U64)1); Unit_CheckEq(&e1->obj, &fp.entries[1].obj);
	FooPool::Entry const* e2 = fp.Alloc(); Unit_CheckEq(e2->Handle().idx, (U64)2); Unit_CheckEq(e2->Handle().gen, (U64)2); Unit_CheckEq(&e2->obj, &fp.entries[2].obj);
	FooPool::Entry const* e3 = fp.Alloc(); Unit_CheckEq(e3->Handle().idx, (U64)3); Unit_CheckEq(e3->Handle().gen, (U64)3); Unit_CheckEq(&e3->obj, &fp.entries[3].obj);
	FooPool::Entry const* e4 = fp.Alloc(); Unit_CheckEq(e4->Handle().idx, (U64)4); Unit_CheckEq(e4->Handle().gen, (U64)4); Unit_CheckEq(&e4->obj, &fp.entries[4].obj);
	FooPool::Entry const* e5 = fp.Alloc(); Unit_CheckEq(e5->Handle().idx, (U64)5); Unit_CheckEq(e5->Handle().gen, (U64)5); Unit_CheckEq(&e5->obj, &fp.entries[5].obj);

	fp.Free(e1->Handle());
	fp.Free(e2->Handle());
	fp.Free(e4->Handle());
	fp.Free(e5->Handle());

	FooPool::Entry const* e6 = fp.Alloc(); Unit_CheckEq(e6->Handle().idx, (U64)5); Unit_CheckEq(e6->Handle().gen, (U64)6); Unit_CheckEq(&e6->obj, &fp.entries[5].obj); 
	FooPool::Entry const* e7 = fp.Alloc(); Unit_CheckEq(e7->Handle().idx, (U64)4); Unit_CheckEq(e7->Handle().gen, (U64)7); Unit_CheckEq(&e7->obj, &fp.entries[4].obj); 
	FooPool::Entry const* e8 = fp.Alloc(); Unit_CheckEq(e8->Handle().idx, (U64)2); Unit_CheckEq(e8->Handle().gen, (U64)8); Unit_CheckEq(&e8->obj, &fp.entries[2].obj); 

	Unit_CheckEq(fp.entries[1].gen, 0u); Unit_CheckEq(fp.entries[1].idx, 0u);
	Unit_CheckEq(fp.entries[2].gen, 8u); Unit_CheckEq(fp.entries[2].idx, 2u);
	Unit_CheckEq(fp.entries[3].gen, 3u); Unit_CheckEq(fp.entries[3].idx, 3u);
	Unit_CheckEq(fp.entries[4].gen, 7u); Unit_CheckEq(fp.entries[4].idx, 4u);
	Unit_CheckEq(fp.entries[5].gen, 6u); Unit_CheckEq(fp.entries[5].idx, 5u);

	Unit_CheckEq(fp.free, 1u);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC