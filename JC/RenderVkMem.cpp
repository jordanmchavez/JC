#include "JC/RenderVk.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

/*
Static
	>= 256mb
		Dedicated DEVICE_LOCAL
		Fallback  0
	< 256mb
		Block    DEVICE_LOCAL
		Fallback 0

Staging
	HOST_VISIBLE | HOST_COHERENT
	no fallback
	< 64k allocation
	>= 64k allocations do the linear arena thing on the list of blocks managed by the allocator
	freeing a linear block decreases the usage and if zero offset goes to zero (full block reset)
	arena allocator
	256mb, rounded up to next multiple of 256mb for the linear allocator
	64k minimum block size, but double arena (arena within arena, doesn't make any sense)
*/



//--------------------------------------------------------------------------------------------------

}	// namespace JC