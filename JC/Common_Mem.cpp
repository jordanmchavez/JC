#include "JC/Common_Mem.h"
#include "JC/Common_Assert.h"
#include "JC/Bit.h"
#include "JC/Handle.h"
#include "JC/Sys.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

static constexpr U64 Align   = 8;
static constexpr U32 MaxMems = 64;

struct MemObj {
	U8* begin;
	U8* end;
	U8* endCommit;
	U8* endReserve;
	U8* lastAlloc;
};

static HandleArray<MemObj, Mem> memObjs;

//--------------------------------------------------------------------------------------------------

void Init() {
	constexpr U64 size = Bit::AlignUp(MaxMems * sizeof(MemObj), Sys::VirtualPageSize);
	HandleArray<MemObj, Mem>::Entry* entries = (HandleArray<MemObj, Mem>::Entry*)Sys::VirtualAlloc(size);
	memObjs.Init(entries, MaxMems);
}

//--------------------------------------------------------------------------------------------------

Mem Create(U64 reserveSize) {
	Assert(Bit::IsPow2(reserveSize));
	HandleArray<MemObj, Mem>::Entry* const entry = memObjs.Alloc();

	MemObj* const memObj = &entry->obj;
	memObj->begin      = (U8*)Sys::VirtualReserve(reserveSize);
	memObj->end        = memObj->begin;
	memObj->endCommit  = memObj->begin;
	memObj->endReserve = memObj->begin + reserveSize;
	memObj->lastAlloc  = 0;

	return entry->Handle();
}

//--------------------------------------------------------------------------------------------------

void Destroy(Mem mem) {
	if (mem) {
		memObjs.Free(mem);
	}
}

//--------------------------------------------------------------------------------------------------

void* Alloc(Mem mem, U64 size, SrcLoc) {
	Assert(mem);
	MemObj* const memObj = memObjs.Get(mem);

	size = Bit::AlignUp(size, Align);

	Assert(memObj->endCommit >= memObj->end);

	const U64 avail = (U64)(memObj->endCommit - memObj->end);
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

	memset(oldEnd, 0, size);
	return oldEnd;
}

//--------------------------------------------------------------------------------------------------

bool Extend(Mem mem, void* ptr, U64 size, SrcLoc) {
	Assert(mem);
	MemObj* const memObj = memObjs.Get(mem);

	if (!ptr || memObj->lastAlloc != ptr) {
		return false;
	}

	size = Bit::AlignUp(size, Align);
	memObj->end = (U8*)ptr;
	Assert(memObj->endCommit >= memObj->end);
	const U64 avail = (U64)(memObj->endCommit - memObj->end);
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

	return true;
}

//--------------------------------------------------------------------------------------------------

U64 Mark(Mem mem) {
	Assert(mem);
	MemObj* const memObj = memObjs.Get(mem);
	Assert(memObj->end >= memObj->begin);
	return (U64)(memObj->end - memObj->begin);
}

//--------------------------------------------------------------------------------------------------

void Reset(Mem mem, U64 mark) {
	Assert(mem);
	MemObj* const memObj = memObjs.Get(mem);
	Assert(memObj->begin + mark <= memObj->endCommit);
	memObj->end = memObj->begin + mark;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem