#include "JC/Common_Mem.h"
#include "JC/Common_Assert.h"
#include "JC/Bit.h"
#include "JC/Sys.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

static constexpr U64 Align   = 8;
static constexpr U32 MaxMems = 64;

struct Mem {
	U8* begin;
	U8* end;
	U8* endCommit;
	U8* endReserve;
	U8* lastAlloc;
};

static Mem mems[MaxMems];

//--------------------------------------------------------------------------------------------------

Mem* Create(U64 reserveSize) {
	Assert(Bit::IsPow2(reserveSize));

	Mem* mem = 0;
	for (U32 i = 0; i < MaxMems; i++) {
		if (!mems[i].begin) {
			mem = &mems[i];
			break;
		}
	}
	Assert(mem, "Exceeded Mem_MaxMems=%u", MaxMems);

	mem->begin      = (U8*)Sys::VirtualReserve(reserveSize);
	mem->end        = mem->begin;
	mem->endCommit  = mem->begin;
	mem->endReserve = mem->begin + reserveSize;
	mem->lastAlloc  = 0;

	return mem;
}

//--------------------------------------------------------------------------------------------------

void Destroy(Mem* mem) {
	Assert(mem);
	if (mem->begin) {
		Sys::VirtualFree(mem->begin);
	}
	memset(mem, 0, sizeof(mem));
}

//--------------------------------------------------------------------------------------------------

void* Alloc(Mem* mem, U64 size, SrcLoc) {
	Assert(mem);

	size = Bit::AlignUp(size, Align);

	Assert(mem->endCommit >= mem->end);

	const U64 avail = (U64)(mem->endCommit - mem->end);
	if (size > avail) {
		U64 const curCommit = (U64)(mem->endCommit - mem->begin);
		U64 nextCommit = Max((U64)4096, curCommit);
		while (avail + (nextCommit - curCommit) < size) {
			nextCommit *= 2;
		}
		Assert(mem->begin + nextCommit <= mem->endReserve);
		Sys::VirtualCommit(mem->endCommit, nextCommit - curCommit);
		mem->endCommit  = mem->begin + nextCommit;
	}
	U8* const oldEnd = mem->end;
	mem->end += size;

	Assert(mem->end <= mem->endCommit);
	Assert(Bit::IsPow2(mem->endCommit - mem->begin));
	mem->lastAlloc = oldEnd;

	memset(oldEnd, 0, size);
	return oldEnd;
}

//--------------------------------------------------------------------------------------------------

bool Extend(Mem* mem, void* ptr, U64 size, SrcLoc) {
	Assert(mem);

	if (!ptr || mem->lastAlloc != ptr) {
		return false;
	}

	size = Bit::AlignUp(size, Align);
	mem->end = (U8*)ptr;
	Assert(mem->endCommit >= mem->end);
	const U64 avail = (U64)(mem->endCommit - mem->end);
	if (size > avail) {
		U64 const curCommit = (U64)(mem->endCommit - mem->begin);
		U64 nextCommit = Max((U64)4096, curCommit);
		while (avail + (nextCommit - curCommit) < size) {
			nextCommit *= 2;
		}
		Assert(mem->begin + nextCommit <= mem->endReserve);
		Sys::VirtualCommit(mem->endCommit, nextCommit - curCommit);
		mem->endCommit  = mem->begin + nextCommit;
	}
	mem->end += size;
	Assert(mem->end <= mem->endCommit);
	Assert(Bit::IsPow2(mem->endCommit - mem->begin));

	return true;
}

//--------------------------------------------------------------------------------------------------

void Reset(Mem* mem) {
	Assert(mem);
	mem->end = mem->begin;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem