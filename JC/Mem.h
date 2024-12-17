#pragma once

#include "JC/Common.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

struct Arena {
	u8* begin      = 0;
	u8* end        = 0;
	u8* endCommit  = 0;
	u8* endReserve = 0;

	void* Alloc(u64 size, SrcLoc sl = SrcLoc::Here());
	bool  Extend(void* p, u64 oldSize, u64 newSize, SrcLoc sl = SrcLoc::Here());
	u64   Mark();
	void  Reset(u64 mark);
};

struct Api {
	virtual Arena CreateArena(u64 reserveSize) = 0;
	virtual void  DestroyArena(Arena arena) = 0;
};

Api* GetApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem