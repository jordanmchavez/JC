#include "JC/Mem.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Map.h"
#include "JC/Sys.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

struct Trace {
	SrcLoc sl     = {};
	u64    bytes  = 0;
	u32    allocs = 0;
};

static constexpr u64 AlignSize = 8;

//--------------------------------------------------------------------------------------------------

void* Arena::Alloc(u64 size, SrcLoc) {
	size = AlignUp(size, AlignSize);
	Assert(endCommit >= end);
	const u64 avail = (u64)(endCommit - end);
	if (avail < size) {
		const u64 commitSize = (u64)(endCommit - begin);
		u64 extendSize = Max((u64)4096, commitSize);
		while (avail + extendSize < size) {
			extendSize *= 2;
			Assert(endCommit + extendSize < endReserve);
		}
		Sys::VirtualCommit(endCommit, extendSize);
		endCommit += extendSize;
	}
	void* p = end;
	end += size;
	Assert(end <= endCommit);
	return p;
}

//--------------------------------------------------------------------------------------------------

bool Arena::Extend(void* p, u64 oldSize, u64 newSize, SrcLoc) {
	if (!p) {
		return false;
	}

	oldSize = AlignUp(oldSize, AlignSize);
	if (p != end - oldSize) {
		return false;
	}

	newSize = AlignUp(newSize, AlignSize);
	end -= oldSize;
	Assert(endCommit >= end);
	const u64 avail = (u64)(endCommit - end);
	if (avail < newSize) {
		const u64 commitSize = (u64)(endCommit - begin);
		u64 extendSize = Max((u64)4096, commitSize);
		while (avail + extendSize < newSize) {
			extendSize *= 2;
			Assert(endCommit + extendSize < endReserve);
		}
		Sys::VirtualCommit(endCommit, extendSize);
		endCommit += extendSize;
	}
	end += newSize;
	return true;
}

//--------------------------------------------------------------------------------------------------

struct ApiObj : Api {

	Array<Trace>     traces            = {};
	Map<u64, u64>    slToTrace         = {};
	Map<void*, u64>  ptrToTrace        = {};

	Arena CreateArena(u64 reserveSize) override {
		u8* const begin = (u8*)Sys::VirtualReserve(reserveSize);
		return Arena {
			.begin      = begin,
			.end        = begin,
			.endCommit  = begin,
			.endReserve = begin + reserveSize,
		};
	}

	void DestroyArena(Arena arena) override {
		Sys::VirtualFree(arena.begin);
	}
};

static ApiObj apiObj;

Api* GetApi() { return &apiObj; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem