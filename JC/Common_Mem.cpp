#include "JC/Common.h"
#include "JC/Bit.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

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
	for (U32 i = 1; i < MaxMemObjs; i++) {	// reserver zero
		if (!memObjs[i].begin)
		{
			memObj = &memObjs[i];
			break;
		}
	}
	Assert(memObj);

	memObj->begin      = (U8*)Sys::VirtualReserve(reserveSize);
	memObj->end        = memObj->begin;
	memObj->endCommit  = memObj->begin;
	memObj->endReserve = memObj->begin + reserveSize;
	memObj->lastAlloc   = 0;

	return Mem { .handle = (U64)(memObj - memObjs) };
}

//--------------------------------------------------------------------------------------------------

void Mem::Destroy(Mem mem) {
	Assert(mem.handle < MaxMemObjs);
	if (mem.handle)
	{
		memset(&memObjs[mem.handle], 0, sizeof(MemObj));
	}
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

void* Mem::Extend(Mem mem, void* ptr, U64 size, SrcLoc sl) {
	Assert(mem);
	Assert(mem.handle < MaxMemObjs);
	MemObj* const memObj = &memObjs[mem.handle];

	if (!ptr) {
		return Alloc(mem, size, sl);
	}
	
	Assert(
		memObj->lastAlloc == (U8*)ptr,
		"Last stack alloc %p at %s(%u)",
		memObj->lastAlloc,
		memObj->lastAllocSl.file,
		memObj->lastAllocSl.line
	);

	size = Bit::AlignUp(size, Align);
	memObj->end = (U8*)ptr;
	Assert(memObj->endCommit >= memObj->end);
	U64 const avail = (U64)(memObj->endCommit - memObj->end);
	if (size > avail) {
		U64 const curCommit = (U64)(memObj->endCommit - memObj->begin);
		U64 nextCommit = Max((U64)4096, curCommit);
		while (avail + (nextCommit - curCommit) < size) {
			nextCommit *= 2;
		}
		Assert(memObj->begin + nextCommit <= memObj->endReserve);
		Sys::VirtualCommit(memObj->endCommit, nextCommit - curCommit);
		memObj->endCommit  = memObj->begin + nextCommit;
	}
	memObj->end += size;
	Assert(memObj->end <= memObj->endCommit);
	Assert(Bit::IsPow2(memObj->endCommit - memObj->begin));

	return ptr;
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