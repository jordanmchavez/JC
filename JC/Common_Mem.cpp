#include "JC/Common.h"

#include "JC/Bit.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static_assert(Bit::IsPow2(Sys::VirtualPageSize));

static constexpr U64 Align              = 8;
static constexpr U32 MaxMemObjs         = 64;

struct MemObj {
	U8*    begin = 0;
	U8*    end = 0;
	U8*    endCommit = 0;
	U8*    endReserve = 0;
	U8*    lastAlloc = 0;
	SrcLoc lastAllocSl;
};

static MemObj memObjs[MaxMemObjs];

//--------------------------------------------------------------------------------------------------

Mem Mem::Create(U64 reserveSize) {
	Assert(Bit::IsPow2(reserveSize));

	MemObj* memObj = 0;
	for (U32 i = 1; i < MaxMemObjs; i++) {	// reserve zero
		if (!memObjs[i].begin)
		{
			memObj = &memObjs[i];
			break;
		}
	}
	Assert(memObj);

	memObj->begin       = (U8*)Sys::VirtualReserve(reserveSize);
	memObj->end         = memObj->begin;
	memObj->endCommit   = memObj->begin;
	memObj->endReserve  = memObj->begin + reserveSize;
	memObj->lastAlloc   = 0;
	memObj->lastAllocSl = { .file = "", .line = 0 };

	return Mem { .handle = (U64)(memObj - memObjs) };
}

//--------------------------------------------------------------------------------------------------

void* Mem::Alloc(Mem mem, U64 size, SrcLoc sl) {
	Assert(mem);
	Assert(mem.handle < MaxMemObjs);
	MemObj* const memObj = &memObjs[mem.handle];

	size = Bit::AlignUp(size, Align);

	Assert(memObj->endCommit >= memObj->end);

	U64 const avail = (U64)(memObj->endCommit - memObj->end);
	if (size > avail) {
		U64 const curCommit = (U64)(memObj->endCommit - memObj->begin);
		U64 nextCommit = Max(Sys::VirtualPageSize, curCommit);
		while (avail + (nextCommit - curCommit) < size) {
			nextCommit *= 2;
		}
		Assert(memObj->begin + nextCommit <= memObj->endReserve);
		Sys::VirtualCommit(memObj->endCommit, nextCommit - curCommit);
		memObj->endCommit  = memObj->begin + nextCommit;
	}
	U8* const oldEnd = memObj->end;
	memObj->end += size;

	Assert(memObj->end <= memObj->endCommit);
	Assert(Bit::IsPow2(memObj->endCommit - memObj->begin));

	memObj->lastAlloc = oldEnd;
	memObj->lastAllocSl = sl;

	memset(oldEnd, 0, size);
	return oldEnd;
}

//--------------------------------------------------------------------------------------------------

void* Mem::Realloc(Mem mem, void* oldPtr, U64 oldSize, U64 newSize, SrcLoc sl) {
	Assert(mem);
	Assert(mem.handle < MaxMemObjs);
	MemObj* const memObj = &memObjs[mem.handle];

	if (!oldPtr || memObj->lastAlloc != (U8*)oldPtr) {
		if (newSize <= oldSize) {
			return oldPtr;
		}
		void* const newPtr = Alloc(mem, newSize, sl);
		if (oldPtr) {
			memcpy(newPtr, oldPtr, oldSize);
		}
		return newPtr;
	}

	U64 const alignedNewSize = Bit::AlignUp(newSize, Align);
	memObj->end = (U8*)oldPtr;
	Assert(memObj->endCommit >= memObj->end);
	U64 const avail = (U64)(memObj->endCommit - memObj->end);
	if (alignedNewSize > avail) {
		U64 const curCommit = (U64)(memObj->endCommit - memObj->begin);
		U64 nextCommit = Max(Sys::VirtualPageSize, curCommit);
		while (avail + (nextCommit - curCommit) < alignedNewSize) {
			nextCommit *= 2;
		}
		Assert(memObj->begin + nextCommit <= memObj->endReserve);
		Sys::VirtualCommit(memObj->endCommit, nextCommit - curCommit);
		memObj->endCommit  = memObj->begin + nextCommit;
	}
	memObj->end += alignedNewSize;
	Assert(memObj->end <= memObj->endCommit);
	Assert(Bit::IsPow2(memObj->endCommit - memObj->begin));
	memObj->lastAllocSl = sl;

	if (newSize > oldSize) {
		memset((U8*)oldPtr + oldSize, 0, newSize - oldSize);
	}
	return oldPtr;
}

//--------------------------------------------------------------------------------------------------

MemMark Mem::Mark(Mem mem) {
	Assert(mem);
	Assert(mem.handle < MaxMemObjs);
	MemObj* const memObj = &memObjs[mem.handle];
	Assert(memObj->end >= memObj->begin);
	return {
		.mark        = (U64)(memObj->end - memObj->begin),
		.lastAlloc   = memObj->lastAlloc,
		.lastAllocSl = memObj->lastAllocSl,
	};
}

//--------------------------------------------------------------------------------------------------

void Mem::Reset(Mem mem, MemMark mark) {
	Assert(mem);
	Assert(mem.handle < MaxMemObjs);
	MemObj* const memObj = &memObjs[mem.handle];
	Assert(memObj->begin + mark.mark <= memObj->end);
	Assert(!mark.lastAlloc || (memObj->begin <= mark.lastAlloc && mark.lastAlloc <= memObj->begin + mark.mark));
	memObj->end         = memObj->begin + mark.mark;
	memObj->lastAlloc   = mark.lastAlloc;
	memObj->lastAllocSl = mark.lastAllocSl;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem