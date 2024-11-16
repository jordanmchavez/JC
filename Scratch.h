#include "JC/Common.h"
#include "JC/MinimalWindows.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Mem {
	u8*  beg;
	u8*  end;

	void* Alloc(Mem* a, u64 size) {
		u8* newEnd = a->beg + size;
		if (newEnd > a->end) {
			a->end = Grow(newEnd);
		}
		void* p = a->beg;
		a->beg += size;
		return p;
	}
};

struct VMem {
	u8* beg;
	u8* end;
	u8* endReserve;
};

VMem* FindVMem(u8* p) {
}

u8* Grow(u8* newEnd) {
	VMem* m = FindVMem(newEnd);
	if (m->end >= newEnd) {
		u8* nextEnd = m->beg + Max((u64)4096, (u64)(m->end - m->beg) * 2);
		while (nextEnd < newEnd) {
			nextEnd = m->beg + (nextEnd - m->beg) * 2;
		}
		JC_ASSERT(nextEnd <= m->endReserve);
		JC_ASSERT(VirtualAlloc(m->end, nextEnd - m->end, MEM_COMMIT, PAGE_READWRITE), "oom");
		m->end = nextEnd;
	}
	return m->end;

}


//--------------------------------------------------------------------------------------------------

}
