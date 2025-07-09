#include "JC/Pool.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Foo { Str name = {}; };
struct FooHandle { U64 handle = 0; };

UnitTest("HandlePool") {
	using FooPool = HandlePool<Foo, FooHandle, 16>;
	FooPool fp;

	const FooPool::Entry* e1 = fp.Alloc(); CheckEq(e1->Handle().handle, ((U64)1 << 32) | 1); CheckEq(&e1->obj, &fp.entries[1].obj);
	const FooPool::Entry* e2 = fp.Alloc(); CheckEq(e2->Handle().handle, ((U64)2 << 32) | 2); CheckEq(&e2->obj, &fp.entries[2].obj);
	const FooPool::Entry* e3 = fp.Alloc(); CheckEq(e3->Handle().handle, ((U64)3 << 32) | 3); CheckEq(&e3->obj, &fp.entries[3].obj);
	const FooPool::Entry* e4 = fp.Alloc(); CheckEq(e4->Handle().handle, ((U64)4 << 32) | 4); CheckEq(&e4->obj, &fp.entries[4].obj);
	const FooPool::Entry* e5 = fp.Alloc(); CheckEq(e5->Handle().handle, ((U64)5 << 32) | 5); CheckEq(&e5->obj, &fp.entries[5].obj);

	fp.Free(e1->Handle());
	fp.Free(e2->Handle());
	fp.Free(e4->Handle());
	fp.Free(e5->Handle());

	const FooPool::Entry* e6 = fp.Alloc(); CheckEq(e6->Handle().handle, ((U64)6 << 32) | 5); CheckEq(&e6->obj, &fp.entries[5].obj); 
	const FooPool::Entry* e7 = fp.Alloc(); CheckEq(e7->Handle().handle, ((U64)7 << 32) | 4); CheckEq(&e7->obj, &fp.entries[4].obj); 
	const FooPool::Entry* e8 = fp.Alloc(); CheckEq(e8->Handle().handle, ((U64)8 << 32) | 2); CheckEq(&e8->obj, &fp.entries[2].obj); 

	CheckEq(fp.entries[1].gen, 0u); CheckEq(fp.entries[1].idx, 0u);
	CheckEq(fp.entries[2].gen, 8u); CheckEq(fp.entries[2].idx, 2u);
	CheckEq(fp.entries[3].gen, 3u); CheckEq(fp.entries[3].idx, 3u);
	CheckEq(fp.entries[4].gen, 7u); CheckEq(fp.entries[4].idx, 4u);
	CheckEq(fp.entries[5].gen, 6u); CheckEq(fp.entries[5].idx, 5u);

	CheckEq(fp.free, 1u);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC