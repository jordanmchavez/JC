#include "JC/Core.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Map.h"
#include "JC/Sys.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

static constexpr u64 Align = 8;

struct TempAllocatorObj : TempAllocator {
	u8* begin      = 0;
	u8* end        = 0;
	u8* endCommit  = 0;
	u8* endReserve = 0;
	u8* last       = 0;

	//----------------------------------------------------------------------------------------------

	void Init(u64 reserveSize) {
	}

	//----------------------------------------------------------------------------------------------

	void* Alloc(u64 size, SrcLoc) override {
		size = Bit::AlignUp(size, AlignSize);
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
		u8* const p = end;
		end += size;
		memset(p, 0x00, size);
		Assert(end <= endCommit);
		Assert(endCommit <= endReserve);
		last = p;

		return p;
	}

	//----------------------------------------------------------------------------------------------

	bool Extend(void* p, u64 newSize, SrcLoc) override {
		if (!p || p != last) {
			return false;
		}

		Assert(end >= last);
		const u64 oldSize = end - last;
		newSize = Bit::AlignUp(newSize, AlignSize);
		end = (u8*)p;
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

		memset((u8*)p + oldSize, 0, newSize - oldSize);

		Assert(end <= endCommit);
		Assert(endCommit <= endReserve);

		return true;
	}
};

//--------------------------------------------------------------------------------------------------

Arena CreateArena(u64 reserveSize) {
	u8* const begin = (u8*)Sys::VirtualReserve(reserveSize);
	return Arena {
		.begin      = begin,
		.end        = begin,
		.endCommit  = begin,
		.endReserve = begin + reserveSize,
	};
}

void DestroyArena(Arena arena) {
	Sys::VirtualFree(arena.begin);
}

//--------------------------------------------------------------------------------------------------

u64 Arena::Mark() {
	return (u64)(end - begin);
}

void Arena::Reset(u64 mark) {
	Assert(mark < (u64)(endCommit - begin));
	end = begin + mark;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem