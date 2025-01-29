#include "JC/Core.h"	// not Core/Mem.h to preserve core inclusion order

#include "JC/Bit.h"
#include "JC/Sys.h"
#include "JC/TLSF.h"

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
		begin      = (u8*)Sys::VirtualReserve(reserveSize);
		end        = begin;
		endCommit  = begin;
		endReserve = begin + reserveSize;
	}

	//----------------------------------------------------------------------------------------------

	void* Alloc(void* ptr, u64 ptrSize, u64 size, u32 flags, SrcLoc) override {
		if (!ptr) {
			// alloc
			size = Bit::AlignUp(size, Align);
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

			if (!(flags & AllocFlag_NoInit)) {
				memset(p, 0x0, size);
			}

			Assert(end <= endCommit);
			Assert(endCommit <= endReserve);
			last = p;

			return ptr;

		} else if (ptr == last && size) {
			// realloc
			Assert(end >= last);
			size = Bit::AlignUp(size, Align);
			end = (u8*)ptr;
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
			end += size;

			if (size > ptrSize && !(flags & AllocFlag_NoInit)) {
				memset((u8*)ptr + ptrSize, 0, size - ptrSize);
			}

			Assert(end <= endCommit);
			Assert(endCommit <= endReserve);

			return ptr;

		} else {
			// free: nop
			return 0;
		}
	}

	void Reset() override {
		end = begin;
	}

};

//--------------------------------------------------------------------------------------------------

Allocator* CreateAllocator(u64 reserveSize) {
	u8* const mem = (u8*)Sys::VirtualReserve(reserveSize);
	return TLSF::CreateAllocator(mem, reserveSize);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem