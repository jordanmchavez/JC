#include "JC/Common.h"

#include "JC/Bit.h"
#include "JC/Sys.h"
#include "JC/Unit.h"

//--------------------------------------------------------------------------------------------------

static constexpr U64 Mem_Align   = 8;
static constexpr U32 Mem_MaxMems = 64;

struct Mem {
	U8* begin;
	U8* end;
	U8* endCommit;
	U8* endReserve;
	U8* lastAlloc;
};

static Mem mem_mems[Mem_MaxMems];

//--------------------------------------------------------------------------------------------------

Mem* Mem_Create(U64 reserveSize) {
	Assert(IsPow2(reserveSize));

	Mem* mem = 0;
	for (U32 i = 0; i < Mem_MaxMems; i++) {
		if (!mem_mems[i].begin) {
			mem = &mem_mems[i];
			break;
		}
	}
	Assert(mem, "Exceeded Mem_MaxMems={}", Mem_MaxMems);

	mem->begin      = (U8*)Sys_VirtualReserve(reserveSize);
	mem->end        = mem->begin;
	mem->endCommit  = mem->begin;
	mem->endReserve = mem->begin + reserveSize;
	mem->lastAlloc  = 0;

	return mem;
}

//--------------------------------------------------------------------------------------------------

void Mem_Destroy(Mem* mem) {
	Assert(mem);
	if (mem->begin) {
		Sys_VirtualFree(mem->begin);
	}
	memset(mem, 0, sizeof(mem));
}

//--------------------------------------------------------------------------------------------------

void* Mem_Alloc(Mem* mem, U64 size, SrcLoc) {
	Assert(mem);

	size = AlignUp(size, Mem_Align);

	Assert(mem->endCommit >= mem->end);

	const U64 avail = (U64)(mem->endCommit - mem->end);
	if (size > avail) {
		const U64 committed = (U64)(mem->endCommit - mem->begin);
		U64 extend = Max((U64)4096, committed);
		while (avail + extend < size) {
			extend *= 2;
		}
		Assert(mem->endCommit + extend <= mem->endReserve);
		Sys_VirtualCommit(mem->endCommit, extend);
		mem->endCommit += extend;
	}
	U8* const oldEnd = mem->end;
	mem->end += size;

	Assert(mem->end <= mem->endCommit);
	Assert(IsPow2(mem->endCommit - mem->begin));
	mem->lastAlloc = oldEnd;

	memset(oldEnd, 0, size);
	return oldEnd;
}

//--------------------------------------------------------------------------------------------------

bool Mem_Extend(Mem* mem, void* ptr, U64 size, SrcLoc) {
	Assert(mem);

	if (mem->lastAlloc != ptr) {
		return false;
	}

	size = AlignUp(size, Mem_Align);
	mem->end = (U8*)ptr;
	Assert(mem->endCommit >= mem->end);
	const U64 avail = (U64)(mem->endCommit - mem->end);
	if (size > avail) {
		const U64 commit = (U64)(mem->endCommit - mem->begin);
		U64 extend = Max((U64)4096, commit);
		while (avail + extend < size) {
			extend *= 2;
		}
		Assert(mem->endCommit + extend < mem->endReserve);
		Sys_VirtualCommit(mem->endCommit, extend);
		mem->endCommit += extend;
	}
	mem->end += size;
	Assert(mem->end <= mem->endCommit);
	Assert(IsPow2(mem->endCommit - mem->begin));

	return true;
}

//--------------------------------------------------------------------------------------------------

void Mem_Reset(Mem* mem) {
	Assert(mem);
	mem->end = mem->begin;
}