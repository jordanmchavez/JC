#include "JC/Common.h"
#include "JC/MinimalWindows.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct VMem {
	u8* beg;
	u8* end;
	u8* endReserve;
};

VMem* FindVMem(u8* p) {
}

struct Mem {
	u8*  beg;
	u8*  end;

	void* Alloc(u64 size) {
		u8* newEnd = beg + size;
		if (newEnd > end) {
			end = Grow(newEnd);
		}
		void* p = beg;
		beg += size;
		return p;
	}

	void* Realloc(void* p, u64 oldSize, u64 newSize) {
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

	static u8* Grow(u8* newEnd) {
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
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC