#include "JC/Mem.h"

#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct VMem {
	u8* beg;
	u8* end;
	u8* endReserve;
};

//--------------------------------------------------------------------------------------------------

static VMem perm;
static VMem scratch;

//--------------------------------------------------------------------------------------------------

VMem* FindVMem(u8* p) {
	if (p >= perm.beg && p <= perm.endReserve) {
		return &perm;
	} else if (p >= scratch.beg && p <= scratch.endReserve) {
		return &scratch;
	}
	Panic("Pointer {} is not in either the perm ({}-{}) or scratch arenas ({}-{})", p, perm.beg, perm.end, scratch.beg, scratch.end);
}

//--------------------------------------------------------------------------------------------------

static u8* Grow(u8* newEnd) {
	VMem* m = FindVMem(newEnd);
	if (m->end >= newEnd) {
		u8* nextEnd = m->beg + Max((u64)4096, (u64)(m->end - m->beg) * 2);
		while (nextEnd < newEnd) {
			nextEnd = m->beg + (nextEnd - m->beg) * 2;
		}
		Assert(nextEnd <= m->endReserve);
		VirtualMemoryApi::Get()->Commit(m->end, nextEnd - m->end);
		m->end = nextEnd;
	}
	return m->end;
}

//--------------------------------------------------------------------------------------------------

void* Mem::Alloc(u64 size) {
	u8* newEnd = beg + size;
	if (newEnd > end) {
		end = Grow(newEnd);
	}
	void* p = beg;
	beg += size;
	return p;
}

//--------------------------------------------------------------------------------------------------

void* Mem::Realloc(void* p, u64 oldSize, u64 newSize) {
	bool inPlace = (u8*)p + oldSize == end;
	if (inPlace) {
		end -= oldSize;
	}
	if (beg + newSize > end) {
		end = Grow(beg + newSize);
	}
	if (!inPlace) {
		memcpy(beg, p, Min(oldSize, newSize));
		p = beg;
		beg += newSize;
	}
	return p;
}

//--------------------------------------------------------------------------------------------------

void Mem::Init() {
	VirtualMemoryApi* v = VirtualMemoryApi::Get();

}

//--------------------------------------------------------------------------------------------------

Mem Mem::Perm()    { return Mem { .beg = perm.beg,    .end = perm.end    }; }
Mem Mem::Scratch() { return Mem { .beg = scratch.beg, .end = scratch.end }; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC