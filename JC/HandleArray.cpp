#include "JC/HandleArray.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Foo { s8 name = {}; };
struct FooHandle { u64 handle = 0; };

UnitTest("HandleArray") {
	HandleArray<Foo, FooHandle> ha;
	ha.Init(tempMem);

	const FooHandle fh = ha.Alloc();
	const Foo* f = ha.Get(fh);
	ha.Free(fh);

	ha.Shutdown();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC