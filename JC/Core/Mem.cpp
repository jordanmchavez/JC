#include "JC/Core.h"	// not Core/Mem.h to preserve core inclusion order

#include "JC/Bit.h"
#include "JC/Sys.h"
#include "JC/UnitTest.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

static constexpr u32 AlignLog2        = 3;
static constexpr u32 Align            = 1 << AlignLog2;

//--------------------------------------------------------------------------------------------------

static constexpr u64 FreeBit     = 1 << 0;
static constexpr u64 PrevFreeBit = 1 << 1;

struct Block {
	Block* prev     = 0;	// prev is actually part of the previous block but can only be referenced if (size | PrevFreeBit)
	// block starts here
	u64    size     = 0;	// bottom two bits reserved for FreeBit and PrevFreeBit
	// user area
	Block* nextFree = 0;	// nextFree and prevFree are only used if the block is free
	Block* prevFree = 0;	// so our minimum "user payload" is 24 bytes (nextFree+prevFree+prev)
	// Block* prev (only if this block is free as defined by next block's (size | PrevFreeBit)

	u64          Size()       const { return size & ~(FreeBit | PrevFreeBit); }
	bool         IsFree()     const { return size & FreeBit; }
	bool         IsPrevFree() const { return size & PrevFreeBit; }
	Block*       Next()             { return (Block*)((u8*)this + Size() + 8); }
	const Block* Next()       const { return (const Block*)((u8*)this + Size() + 8); }
};

static constexpr u32 SecondCountLog2  = 5;
static constexpr u32 SecondCount      = 1 << SecondCountLog2;    // 32
static constexpr u32 FirstShift       = SecondCountLog2 + AlignLog2; // 8
static constexpr u32 FirstMax         = 40;
static constexpr u32 FirstCount       = FirstMax - FirstShift + 1;  // 40-8+1=33
static constexpr u32 SmallBlockSize   = 1 << FirstShift; // 256
static constexpr u64 BlockSizeMin     = 24;	// nextFree + prevFree + next->prev
static constexpr u64 BlockSizeMax     = ((u64)1 << FirstMax) - 8;	// 1TB - 8 bytes

static_assert(Align == SmallBlockSize / SecondCount);

struct Index {
	u32 f = 0;
	u32 s = 0;
};

static Index CalcIndex(u64 size) {
	if (size < SmallBlockSize) {
		return Index {
			.f = 0,
			.s = (u32)size / (SmallBlockSize / SecondCount),
		};
	} else {
		const u32 bit = Bit::Bsr64(size);
		return Index {
			.f = bit - FirstShift + 1,
			.s = (u32)(size >> (bit - SecondCountLog2)) & (SecondCount - 1),
		};
	}
}

//--------------------------------------------------------------------------------------------------

struct AllocatorObj : Allocator {
    Block  nullBlock                       = {};
    u64    first                           = 0;
    u64    second[FirstCount]              = {};
    Block* blocks[FirstCount][SecondCount] = {};
	u8*    begin                           = 0;
	u8*    endCommit                       = 0;
	u8*    endReserve                      = 0;

	//----------------------------------------------------------------------------------------------

	void InsertFreeBlock(Block* block) {
		const Index idx = CalcIndex(block->Size());
		Assert((u64)block % Align  == 0);
		block->nextFree = blocks[idx.f][idx.s];
		block->prevFree = &nullBlock;
		blocks[idx.f][idx.s]->prevFree = block;
		blocks[idx.f][idx.s]           = block;
		first         |= (u64)1 << idx.f;
		second[idx.f] |= (u64)1 << idx.s;
	}

	//----------------------------------------------------------------------------------------------

	void RemoveFreeBlock(Block* block, Index idx) {
		Block* const n = block->nextFree;
		Block* const p = block->prevFree;
		n->prevFree = p;
		p->nextFree = n;
		if (blocks[idx.f][idx.s] == block) {
			blocks[idx.f][idx.s] = n;
			if (n == &nullBlock) {
				second[idx.f] &= ~((u64)1 << idx.s);
				if (!second[idx.f]) {
					first &= ~((u64)1 << idx.f);
				}
			}
		}
	}

	void RemoveFreeBlock(Block* block) { RemoveFreeBlock(block, CalcIndex(block->Size()));  }

	//----------------------------------------------------------------------------------------------

	bool FindFreeIndex(u64 size, Index* out) {
		if (size >= SmallBlockSize) {
			// Round up to next block size
			const u64 round = ((u64)1 << (Bit::Bsr64(size) - SecondCountLog2)) - 1;
			size += round;
		}
		Index idx = CalcIndex(size);
		u64 sMap = second[idx.f] & ((u64)-1 << idx.s);
		if (!sMap) {
			const u32 fMap = first & ((u64)-1 << (idx.f + 1));
			if (!fMap) {
				return false;
			}
			idx.f = Bit::Bsf64(fMap);
			sMap = second[idx.f];
			Assert(sMap);
		}
		idx.s = Bit::Bsf64(sMap);

		*out = idx;
		return true;
	}

	//----------------------------------------------------------------------------------------------

	void Commit() {
		const u64 extendSize = Max((u64)4096, (u64)(endCommit - begin));
		Assert(endCommit + extendSize <= endReserve);
		u8* const oldEndCommit = endCommit;
		endCommit = (u8*)Sys::VirtualCommit(endCommit, extendSize);

		Block* block = ((Block*)(oldEndCommit - 16))->prev;
		Assert(block);	// block->prev should have been set during the last commit
		if (block->IsFree()) {
			RemoveFreeBlock(block);
			block->size += extendSize;
		} else {
			block->size = (extendSize - 8) | FreeBit;	// 8 for the dummy last block's size
		}
		Block* const last = block->Next();
		last->prev = block;
		last->size = 0 | PrevFreeBit;
		InsertFreeBlock(block);
	}

	//----------------------------------------------------------------------------------------------

	void* Alloc(u64 size) {
		Assert(size < BlockSizeMax);

		size = Max(Bit::AlignUp(size, Align), BlockSizeMin);

		Index idx;
		while (!FindFreeIndex(size, &idx)) {
			Commit();
		}

		Block* const block = blocks[idx.f][idx.s];
		Assert(block != &nullBlock);
		Assert(block->Size() >= size);
		Assert(block->IsFree());
		Assert(!block->IsPrevFree());
	
		RemoveFreeBlock(block, idx);

		Block* const next = block->Next();
		Assert(next->IsPrevFree());
		if (block->Size() >= sizeof(Block) + size) {
			Block* const rem = (Block*)((u8*)block + size + 8);
			rem->prev = block;
			rem->size = (block->Size() - size - 8) | FreeBit;
			Assert(rem->Next() == next);
			block->size = size;	// !free, !prevFree 
			next->prev = rem;

			InsertFreeBlock(rem);

		} else {
			block->size &= ~FreeBit;
			next->size  &= ~PrevFreeBit;
		}

		return (u8*)block + 16;
	}

	//----------------------------------------------------------------------------------------------

	bool Extend(void* ptr, u64 ptrSize) {
		ptrSize = Max(Bit::AlignUp(ptrSize, Align), BlockSizeMin);
		Assert(ptrSize <= BlockSizeMax);

		Block* const block = (Block*)((u8*)ptr - 16);
		Assert(!block->IsFree());

		const u64 blockSize = block->Size();
		if (ptrSize <= blockSize) {
			return true;
		}

		Block* next = block->Next();
		if (!next->IsFree()) {
			return false;
		}

		const u64 nextSize = next->Size();
		const u64 combinedSize = blockSize + nextSize + 8;
		if (combinedSize < ptrSize) {
			return false;
		}
		RemoveFreeBlock(next, CalcIndex(nextSize));

		if (combinedSize - ptrSize >= sizeof(Block)) {
			block->size = ptrSize;
			Block* const rem = block->Next();
			rem->prev = block;
			rem->size = (combinedSize - ptrSize - 8) | FreeBit;
			next = rem->Next();
			next->prev = rem;
			next->size |= PrevFreeBit;
			InsertFreeBlock(rem);

		} else {
			block->size += nextSize + 8;	// Doesn't change flags
			next = block->Next();
			next->prev = block;
			next->size &= ~PrevFreeBit;
		}

		return true;
	}

	//----------------------------------------------------------------------------------------------

	void Free(void* ptr) {
		Block* block = (Block*)((u8*)ptr - 16);
		Assert(!block->IsFree());
		block->size |= FreeBit;

		Block* const next = block->Next();
		next->size |= PrevFreeBit;

		if (block->IsPrevFree()) {
			Block* const prev = block->prev;
			Assert(prev->IsFree());
			RemoveFreeBlock(prev);
			prev->size += block->Size() + 8;
			next->prev = prev;
			block = prev;
		}
		if (next->IsFree()) {
			RemoveFreeBlock( next);
			block->size += next->Size() + 8;
			block->Next()->prev = block;
		}

		InsertFreeBlock(block);
	}

	//----------------------------------------------------------------------------------------------

	void* Alloc(void* ptr, u64 ptrSize, u64 size, u32 flags, SrcLoc) override {
		if (!ptr) {
			ptr = Alloc(size);
			if (!(flags & Mem::AllocFlag_NoInit)) {
				memset(ptr, 0, size);
			}

		} else if (size) {
			// TODO: is this even worth it? Our uses of Extend() are overwhelmingly to double our size,
			// so isn't it simpler to just always alloc-new/memcpy/free?
			if (!Extend(ptr, ptrSize)) {
				void* const oldPtr = ptr;
				ptr = Alloc(size);
				if (!(flags & Mem::AllocFlag_NoInit)) {
					memcpy(ptr, oldPtr, size);
				}
				Free(oldPtr);
			}
			if (size > ptrSize && !(flags & Mem::AllocFlag_NoInit)) {
				memset((u8*)ptr + ptrSize, 0, size - ptrSize);
			}

		} else {
			Free(ptr);
		}
		return ptr;
	}

	//----------------------------------------------------------------------------------------------

	void Init(u64 commitSize, u64 reserveSize) {
		Assert(Bit::IsPow2(commitSize));
		Assert(Bit::IsPow2(reserveSize));
		Assert(commitSize >= 48);	// 8 prev + 8 sz + 24 body + 8 sz
		Assert(reserveSize >= commitSize);
		nullBlock.prev     = 0;
		nullBlock.size     = 0;
		nullBlock.nextFree = &nullBlock;
		nullBlock.prevFree = &nullBlock;

		first = 0;
		memset(second, 0, sizeof(second));
		for (u32 i = 0; i < FirstCount; i++) {
			for (u32 j = 0; j < SecondCount; j++) {
				blocks[i][j] = &nullBlock;
			}
		}

		begin      = (u8*)Sys::VirtualReserve(reserveSize);
		endCommit  = (u8*)Sys::VirtualCommit(begin, commitSize);
		endReserve = begin + reserveSize;

		// Note thet the very first block will NEVER have PrevFreeBit, therefore it will never access Prev
		// We could save 8 bytes and make "block = begin - 8", but I think it's safer and safer not to do this
		Block* const block  = (Block*)begin;
		block->prev         = 0;
		block->size         = (commitSize - 16 - 8) | FreeBit;	// 16 for nextFree/prevFree, 8 for the dummy last block's Prev field
		Block* const last   = block->Next();
		last->prev          = block;
		last->size          = 0 | PrevFreeBit;
		InsertFreeBlock(block);
	}
};

//--------------------------------------------------------------------------------------------------

static AllocatorObj allocatorObj;

Allocator* InitDefaultAllocator(u64 commitSize, u64 reserveSize) {
	allocatorObj.Init(commitSize, reserveSize);
	return &allocatorObj;
}

//--------------------------------------------------------------------------------------------------

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

	void* Alloc(u64 size) {
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
		u8* const oldEnd = end;
		end += size;


		Assert(end <= endCommit);
		Assert(endCommit <= endReserve);
		last = oldEnd;

		return oldEnd;
	}

	void Extend(void* ptr, u64 size) {
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

		Assert(end <= endCommit);
		Assert(endCommit <= endReserve);
	}

	void* Alloc(void* ptr, u64 ptrSize, u64 size, u32 flags, SrcLoc) override {
		if (!ptr) {
			Assert(!ptrSize);
			ptr = Alloc(size);
			if (!(flags & AllocFlag_NoInit)) {
				memset(ptr, 0x0, size);
			}
			return ptr;
		} else if (size) {
			// realloc
			if (ptr == last) {
				Extend(ptr, size);
				if (size > ptrSize && !(flags & AllocFlag_NoInit)) {
					memset((u8*)ptr + ptrSize, 0, size - ptrSize);
				}
				return ptr;
			} else {
				void* const newPtr = Alloc(size);
				if (!(flags & AllocFlag_NoInit)) {
					memcpy(newPtr, ptr, ptrSize);
					if (size > ptrSize && !(flags & AllocFlag_NoInit)) {
						memset((u8*)newPtr + ptrSize, 0, size - ptrSize);
					}
				}
				return newPtr;
			}
		} else {
			// free: no-op
			return 0;
		}
	}

	void Reset() override {
		end = begin;
	}

};

//--------------------------------------------------------------------------------------------------

static TempAllocatorObj tempAllocatorObj;

TempAllocator* InitTempAllocator(u64 reserveSize) {
	tempAllocatorObj.Init(reserveSize);
	return &tempAllocatorObj;
}

//--------------------------------------------------------------------------------------------------

struct ExpectedBlock {
	u64  size;
	bool free;
};

void CheckAllocator(AllocatorObj* allocator, Span<ExpectedBlock> expectedBlocks) {
	u32 freeBlockCount[FirstCount][SecondCount] = {};

	for (u32 f = 0; f < FirstCount; f++) {
		const u64 fBit = allocator->first & ((u64)1 << f);
		CheckTrue(
			( fBit &&  allocator->second[f]) ||
			(!fBit && !allocator->second[f])
		);
		for (u32 s = 0; s < SecondCount; s++) {
			const u64 sBit = allocator->second[f] & ((u64)1 << s);
			CheckTrue(
				( sBit && allocator->blocks[f][s] != &allocator->nullBlock) ||
				(!sBit && allocator->blocks[f][s] == &allocator->nullBlock)
			);

			for (Block* block = allocator->blocks[f][s]; block != &allocator->nullBlock; block = block->nextFree) {
				CheckTrue(block->IsFree());
				CheckTrue(!block->IsPrevFree());
				CheckTrue(!block->Next()->IsFree());
				CheckTrue(block->Next()->IsPrevFree());
				CheckTrue(block->Size() >= BlockSizeMin);
				const Index idx = CalcIndex(block->Size());
				CheckEq(idx.f, f);
				CheckEq(idx.s, s);
				freeBlockCount[f][s]++;
			}
		}
	}

	const ExpectedBlock* expectedBlock     = expectedBlocks.Begin();
	const ExpectedBlock* expectedBlocksEnd = expectedBlocks.End();

	Block* prev     = 0;
	bool   prevFree = false;
	Block* block    = (Block*)allocator->begin;
	u32    blockNum = 0;
	while (block->Size() > 0) {
		CheckTrue(expectedBlock < expectedBlocksEnd);
		CheckEq(prev, block->prev);
		CheckEq(prevFree, block->IsPrevFree());
		CheckTrue(!prevFree || !block->IsFree());
			
		CheckEq(expectedBlock->size, block->Size());
		if (block->IsFree()) {
			CheckTrue(expectedBlock->free);
			const Index idx = CalcIndex(block->Size());
			CheckTrue(freeBlockCount[idx.f][idx.s] > 0);
			freeBlockCount[idx.f][idx.s]--;
		} else {
			CheckTrue(!expectedBlock->free);
		}

		expectedBlock++;
		prev     = block;
		prevFree = block->IsFree();
		block    = block->Next();
		blockNum++;
	}
	CheckEq(expectedBlock, expectedBlocksEnd);
	CheckEq(prev, block->prev);
	CheckEq(prevFree, block->IsPrevFree());
	CheckTrue(!block->IsFree());
	CheckEq(allocator->endCommit, (u8*)block + 16);

	for (u32 f = 0; f < FirstCount; f++) {
		for (u32 s = 0; s < SecondCount; s++) {
			CheckTrue(!freeBlockCount[f][s]);
		}
	}
}

//--------------------------------------------------------------------------------------------------

UnitTest("Mem") {
	//      |  Max |  Free Rng | Free Classes           | Alloc Rng | Alloc Classes
	// -----+------+-----------+------------------------+-----------+---------------
	// 2.27 |  944 |           |                        |           |
	// 2.28 |  960 |   960-975 |  960,  968             |   945-960 | 952, 960
	// 2.29 |  976 |   976-991 |  976,  984             |   961-976 | 968, 976
	// 2.30 |  992 |  992-1007 |  992, 1000             |   977-992 | 984, 992
	// 2.31 | 1008 | 1008-1023 | 1008, 1016             |  993-1008 | 1000, 1008
	// 3.0  | 1024 | 1024-1055 | 1024, 1032, 1040, 1048 | 1009-1024 | 1016, 1024
	// 3.1  | 1056 | 1056-1087 | 1056, 1064, 1072, 1080 | 1025-1056 | 1032, 1040, 1048, 1056
	// 3.2  | 1088 | 1088-1119 | 1088, 1096, 1104, 1112 | 1057-1088 | 1064, 1072, 1080, 1088
	// 3.3  | 1120 | 1120-1151 | 1120, 1128, 1136, 1144 | 1089-1120 | 1096, 1104, 1112, 1120
	// 3.4  | 1152 |           |                        |           |

	AllocatorObj allocator;

	SubTest("CalcIndex") {
		#define CheckCalcIndex(size, first, second) \
		{ \
			const Index i = CalcIndex(size); \
			CheckEq(i.f, (u64)first); \
			CheckEq(i.s, (u64)second); \
		}

		u64 size = 0;
		u64 inc = 8;
		for (u32 s = 0; s < SecondCount; s++) {
			CheckCalcIndex(size, 0, s);
			size += inc;
		}
		for (u32 f = 1; f < FirstCount; f++) {
			for (u32 s = 0; s < SecondCount; s++) {
				CheckCalcIndex(size, f, s);
				size += inc;
			}
			inc <<= 1;
		}
		CheckCalcIndex((u64)0x10000000000, 33, 0);
	}

	SubTest("Alloc/Extend/Free") {
		constexpr u64 commitSize  =       64 * 1024;
		constexpr u64 reserveSize = 64 * 1024 * 1024;
		Defer { Sys::VirtualFree(allocator.begin); };
		allocator.Init(commitSize, reserveSize);
		u64 base = commitSize - 24;
		const bool f = true;	// free
		const bool u = false;	// used
		Span<ExpectedBlock> expectedBlocks = {
			{ base, f },
		};
		CheckAllocator(&allocator, expectedBlocks);

		void* p[256] = {};
		u32 pn = 0;
		u64 used = 0;

		#define PAlloc(size) p[pn++] = allocator.Alloc(size); used += size + 8
		PAlloc( 960); PAlloc(24);
		PAlloc( 968); PAlloc(24);
		PAlloc( 976); PAlloc(24);
		PAlloc( 984); PAlloc(24);
		PAlloc( 992); PAlloc(24);
		PAlloc(1000); PAlloc(24);
		PAlloc(1008); PAlloc(24);
		PAlloc(1016); PAlloc(24);
		PAlloc(1024); PAlloc(24);
		PAlloc(1048); PAlloc(24);
		PAlloc(1056); PAlloc(24);
		PAlloc(1080); PAlloc(24);
		PAlloc(1088); PAlloc(24);
		PAlloc(1112); PAlloc(24);
		PAlloc(1120); PAlloc(24);
		PAlloc(1144); PAlloc(24);
		expectedBlocks = {
			{   960, u }, { 24, u }, //  0,  1, 2.28
			{   968, u }, { 24, u }, //  2,  3, 2.28
			{   976, u }, { 24, u }, //  4,  5, 2.29
			{   984, u }, { 24, u }, //  6,  7, 2.29
			{   992, u }, { 24, u }, //  8,  9, 2.30
			{  1000, u }, { 24, u }, // 10, 11, 2.30
			{  1008, u }, { 24, u }, // 12, 13, 2.31
			{  1016, u }, { 24, u }, // 14, 15, 2.31
			{  1024, u }, { 24, u }, // 16, 17, 3.0
			{  1048, u }, { 24, u }, // 18, 19, 3.0
			{  1056, u }, { 24, u }, // 20, 21, 3.1
			{  1080, u }, { 24, u }, // 22, 23, 3.1
			{  1088, u }, { 24, u }, // 24, 25, 3.2
			{  1112, u }, { 24, u }, // 26, 27, 3.2
			{  1120, u }, { 24, u }, // 28, 29, 3.3
			{  1144, u }, { 24, u }, // 30, 31, 3.3
			{ base - used, f },
		};
		CheckAllocator(&allocator, expectedBlocks);

		// free without merge
		for (u32 i = 0; i <= 30; i += 2) { allocator.Free(p[i]); }
		expectedBlocks = {
			{   960, f }, { 24, u }, //  0,  1, 2.28
			{   968, f }, { 24, u }, //  2,  3, 2.28
			{   976, f }, { 24, u }, //  4,  5, 2.29
			{   984, f }, { 24, u }, //  6,  7, 2.29
			{   992, f }, { 24, u }, //  8,  9, 2.30
			{  1000, f }, { 24, u }, // 10, 11, 2.30
			{  1008, f }, { 24, u }, // 12, 13, 2.31
			{  1016, f }, { 24, u }, // 14, 15, 2.31
			{  1024, f }, { 24, u }, // 16, 17, 3.0
			{  1048, f }, { 24, u }, // 18, 19, 3.0
			{  1056, f }, { 24, u }, // 20, 21, 3.1
			{  1080, f }, { 24, u }, // 22, 23, 3.1
			{  1088, f }, { 24, u }, // 24, 25, 3.2
			{  1112, f }, { 24, u }, // 26, 27, 3.2
			{  1120, f }, { 24, u }, // 28, 29, 3.3
			{  1144, f }, { 24, u }, // 30, 31, 3.3
			{ base - used, f },
		};
		CheckAllocator(&allocator, expectedBlocks);

		// REMEMBER: free list is LIFO
		// alloc exact first/second
		void* tmp = 0;
		tmp = allocator.Alloc(945); CheckEq(tmp, p[2]);	// no split
		tmp = allocator.Alloc(945); CheckEq(tmp, p[0]);	// no split
		// alloc exact first, larger second
		tmp = allocator.Alloc(945); CheckEq(tmp, p[6]);	// no split
		tmp = allocator.Alloc(945); CheckEq(tmp, p[4]);	// split 24
		// alloc larger first, larger second
		tmp = allocator.Alloc(945); CheckEq(tmp, p[10]);	// split 32
		tmp = allocator.Alloc(945); CheckEq(tmp, p[8]);	// split 48
		expectedBlocks = {
			{   960, u },            { 24, u }, //  0,     1, 2.28
			{   968, u },            { 24, u }, //  2,     3, 2.28
			{   976, u },            { 24, u }, //  4,     5, 2.29
			{   952, u }, { 24, f }, { 24, u }, //  6, x,  7, 2.28
			{   952, u }, { 32, f }, { 24, u }, //  8, x,  9, 2.28
			{   952, u }, { 40, f }, { 24, u }, // 10, x, 11, 2.28
			{  1008, f },            { 24, u }, // 12,    13, 2.31
			{  1016, f },            { 24, u }, // 14,    15, 2.31
			{  1024, f },            { 24, u }, // 16,    17, 3.0
			{  1048, f },            { 24, u }, // 18,    19, 3.0
			{  1056, f },            { 24, u }, // 20,    21, 3.1
			{  1080, f },            { 24, u }, // 22,    23, 3.1
			{  1088, f },            { 24, u }, // 24,    25, 3.2
			{  1112, f },            { 24, u }, // 26,    27, 3.2
			{  1120, f },            { 24, u }, // 28,    29, 3.3
			{  1144, f },            { 24, u }, // 30,    31, 3.3
			{ base - used, f },
		};
		CheckAllocator(&allocator, expectedBlocks);

		// Extend
		CheckTrue (allocator.Extend(p[0], 0));
		CheckTrue (allocator.Extend(p[0], 960));
		CheckFalse(allocator.Extend(p[0], 961));
		CheckTrue (allocator.Extend(p[6], 953));	// no split
		CheckTrue (allocator.Extend(p[8], 953));	// enough for split
		allocator.Free(p[21]);	// merge prev and next
		allocator.Free(p[31]);	// merge prev and next=LAST
		used -= 1144 + 8 + 24 + 8;
		expectedBlocks = {
			{   960, u },            { 24, u }, //  0,     1, 2.28
			{   968, u },            { 24, u }, //  2,     3, 2.28
			{   976, u },            { 24, u }, //  4,     5, 2.29
			{   984, u },            { 24, u }, //  6,     7, 2.29
			{   960, u }, { 24, f }, { 24, u }, //  8, x,  9, 2.28
			{   952, u }, { 40, f }, { 24, u }, // 10, x, 11, 2.28
			{  1008, f },            { 24, u }, // 12,    13, 2.31
			{  1016, f },            { 24, u }, // 14,    15, 2.31
			{  1024, f },            { 24, u }, // 16,    17, 3.0
			{  1048, f },            { 24, u }, // 18,    19, 3.0
			{  2176, f },            { 24, u }, // merged
			{  1088, f },            { 24, u }, // 24,    25, 3.2
			{  1112, f },            { 24, u }, // 26,    27, 3.2
			{  1120, f },            { 24, u }, // 28,    29, 3.3
			// 30, 31 merged into base
			{ base - used, f },
		};
		CheckAllocator(&allocator, expectedBlocks);

		// Huge allocation forcing multiple commits
		allocator.Alloc(4 * 1024 * 1024);
		base = 8 * 1024 * 1024 - 24;
		used += 4 * 1024 * 1024 + 8;
		expectedBlocks = {
			{   960, u },            { 24, u }, //  0,     1, 2.28
			{   968, u },            { 24, u }, //  2,     3, 2.28
			{   976, u },            { 24, u }, //  4,     5, 2.29
			{   984, u },            { 24, u }, //  6,     7, 2.29
			{   960, u }, { 24, f }, { 24, u }, //  8, x,  9, 2.28
			{   952, u }, { 40, f }, { 24, u }, // 10, x, 11, 2.28
			{  1008, f },            { 24, u }, // 12,    13, 2.31
			{  1016, f },            { 24, u }, // 14,    15, 2.31
			{  1024, f },            { 24, u }, // 16,    17, 3.0
			{  1048, f },            { 24, u }, // 18,    19, 3.0
			{  2176, f },            { 24, u }, // merged
			{  1088, f },            { 24, u }, // 24,    25, 3.2
			{  1112, f },            { 24, u }, // 26,    27, 3.2
			{  1120, f },            { 24, u }, // 28,    29, 3.3
			{ 4 * 1024 * 1024, u },
			{ base - used, f },
		};
		CheckAllocator(&allocator, expectedBlocks);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem