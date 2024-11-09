#include "JC/Math.h"

#if defined JC_COMPILER_MSVC
	#include <intrin.h>
#endif

namespace JC {

//--------------------------------------------------------------------------------------------------

u32 Log2(u64 u) {
	#if defined JC_COMPILER_MSVC
		unsigned long res;
		_BitScanReverse64(&res, u);
		return (u32)res;
	#else	// JC_COMPILER
		#error("Unsupported compiler")
	#endif	// JC_COMPILER
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC