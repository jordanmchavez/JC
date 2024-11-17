#include "JC/Common.h"

#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct VMem {
	u8* beg;
	u8* endCommit;
	u8* endReserve;
};

static constexpr u32 MaxVMems = 128;

static VMem vmems[MaxVMems];
static u32  vmemsLen;

//--------------------------------------------------------------------------------------------------

VMem* FindVMem(u8* p) {
	for (u32 i = 0; i < vmemsLen; i++) {
		if (p >= vmems[i].beg && p <= vmems[i].endReserve) {
			return &vmems[i];
		}
	}
	Panic("No VMem for pointer {}", p);
}

//--------------------------------------------------------------------------------------------------

static u8* Grow(u8* newEnd) {
	VMem* m = FindVMem(newEnd);
	if (newEnd > m->endCommit) {
		u8* nextEnd = m->beg + Max((u64)4096, (u64)(m->endCommit - m->beg) * 2);
		while (nextEnd < newEnd) {
			nextEnd = m->beg + (nextEnd - m->beg) * 2;
		}
		Assert(nextEnd <= m->endReserve);
		Sys::VirtualCommit(m->endCommit, nextEnd - m->endCommit);
		m->endCommit = nextEnd;
	}
	return m->endCommit;
}

//--------------------------------------------------------------------------------------------------

void* Mem::Alloc(u64 size, SrcLoc) {
	u8* newEnd = beg + size;
	if (newEnd > end) {
		end = Grow(newEnd);
	}
	void* p = beg;
	beg += size;
	MemSet(p, 0, size);
	return p;
}

//--------------------------------------------------------------------------------------------------

void* Mem::Realloc(void* p, u64 oldSize, u64 newSize, SrcLoc) {
	bool inPlace = (u8*)p + oldSize == end;
	if (inPlace) {
		if (beg - oldSize + newSize > end) {
			end = Grow(beg - oldSize + newSize);
		}
		beg  = beg - oldSize + newSize;
	} else {
		if (beg + newSize > end) {
			end = Grow(beg + newSize);
		}
		if (p) {
			MemCpy(beg, p, oldSize);
		}
		p = beg;
		beg += newSize;
	}
	return p;
}

//--------------------------------------------------------------------------------------------------

Mem Mem::Create(u64 commit, u64 reserve) {
	Assert(vmemsLen < MaxVMems);
	Assert(reserve % (64 * 1024) == 0);
	Assert(commit % 4096 == 0);
	u8* p = (u8*)Sys::VirtualReserve(reserve);
	if (commit > 0) {
		Sys::VirtualCommit(p, commit);
	}
	vmems[vmemsLen++] = {
		.beg        = p,
		.endCommit  = p + commit,
		.endReserve = p + reserve,
	};
	return Mem { .beg = p, .end = p + commit };
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC