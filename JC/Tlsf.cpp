#include "JC/Tlsf.h"

#include "JC/Bit.h"
#include "JC/UnitTest.h"

namespace JC::TLSF {

//--------------------------------------------------------------------------------------------------

static constexpr u64 FreeBit     = 1 << 0;
static constexpr u64 PrevFreeBit = 1 << 1;

struct Block {
	Block* prev     = 0;
	u64    size     = 0;
	Block* nextFree = 0;
	Block* prevFree = 0;

	u64          Size()       const { return size & ~(FreeBit | PrevFreeBit); }
	bool         IsFree()     const { return size & FreeBit; }
	bool         IsPrevFree() const { return size & PrevFreeBit; }
	Block*       Next()             { return (Block*)((u8*)this + Size() + 8); }
	const Block* Next()       const { return (const Block*)((u8*)this + Size() + 8); }
};

struct Chunk {
	u64    size = 0;
	Chunk* next = 0;
};
static_assert(sizeof(Chunk) % 8 == 0);
static_assert(alignof(Chunk) == 8);

static constexpr u32 AlignLog2        = 3;
static constexpr u32 Align            = 1 << AlignLog2;
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

struct AllocatorObj : Allocator {
    Block  nullBlock                       = {};
    u64    first                           = 0;
    u64    second[FirstCount]              = {};
    Block* blocks[FirstCount][SecondCount] = {};
	Chunk* chunks                          = 0;

	//-------------------------------------------------------------------------------------------------

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

	//-------------------------------------------------------------------------------------------------

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

	//-------------------------------------------------------------------------------------------------

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

	//-------------------------------------------------------------------------------------------------

	void* Alloc(void* ptr, u64 ptrSize, u64 size, u32 flags, SrcLoc) override {
		if (!ptr) {
			ptr = Alloc(size);
			if (!(flags & Mem::AllocFlag_NoInit)) {
				memset(ptr, 0, size);
			}

		} else if (size) {
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

	//-------------------------------------------------------------------------------------------------

	void* Alloc(u64 size) {
		Assert(size < BlockSizeMax);

		size = Max(Bit::AlignUp(size, Align), BlockSizeMin);
		u64 calcIndexSize = size;
		if (size >= SmallBlockSize) {
			// Round up to next block size
			const u64 round = ((u64)1 << (Bit::Bsr64(size) - SecondCountLog2)) - 1;
			calcIndexSize += round;
		}
		Index idx = CalcIndex(calcIndexSize);
		u64 sMap = second[idx.f] & ((u64)-1 << idx.s);
		if (!sMap) {
			const u32 fMap = first & ((u64)-1 << (idx.f + 1));
			if (!fMap) {
				return 0;
			}
			idx.f = Bit::Bsf64(fMap);
			sMap = second[idx.f];
		}
		Assert(sMap);
		idx.s = Bit::Bsf64(sMap);

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

	//-------------------------------------------------------------------------------------------------

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

	//-------------------------------------------------------------------------------------------------

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


	//-------------------------------------------------------------------------------------------------

	void Init() {
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
	}

	//-------------------------------------------------------------------------------------------------

	void AddMem(void* ptr, u64 size) override {
		Assert(ptr);
		Assert((u64)ptr % Align == 0);
		Assert(size % Align == 0);
		Assert(size >= sizeof(Chunk) + 24);

		Chunk* chunk = (Chunk*)ptr;
		chunk->next = chunks;
		chunk->size = size;
		chunks = chunk;

		Block* const block  = (Block*)(chunk + 1);
		block->prev         = 0;
		block->size         = (size - sizeof(Chunk) - 24) | FreeBit;	// 16 for the first block, 8 for the dummy last block
		Block* const last   = block->Next();
		last->prev          = block;
		last->size          = 0 | PrevFreeBit;
		InsertFreeBlock(block);
	}
};

//--------------------------------------------------------------------------------------------------

static constexpr u32 MaxAllocators = 32;
static AllocatorObj allocatorObjs[MaxAllocators];
static u32          allocatorObjsLen;

Allocator* CreateAllocator(void* ptr, u64 size) {
	Assert(((u64)ptr & (Align - 1)) == 0);
	Assert(allocatorObjsLen < MaxAllocators);
	AllocatorObj* allocatorObj = &allocatorObjs[allocatorObjsLen++];
	allocatorObj->AddMem(ptr, size);

	return allocatorObj;
}

//--------------------------------------------------------------------------------------------------

struct ExpectedBlock {
	u64  size;
	bool free;
};

void CheckAllocator(AllocatorObj* allocator, Span<Span<ExpectedBlock>> expectedChunks) {
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
				const Index idx = allocator->CalcIndex(block->Size());
				CheckEq(idx.f, f);
				CheckEq(idx.s, s);
				freeBlockCount[f][s]++;
			}
		}
	}

	const Span<ExpectedBlock>* expectedChunk    = expectedChunks.data;
	const Span<ExpectedBlock>* expectedChunksEnd = expectedChunks.End();

	for (Chunk* chunk = allocator->chunks; chunk; chunk = chunk->next) {
		CheckTrue(expectedChunk < expectedChunksEnd);

		const ExpectedBlock* expectedBlock     = expectedChunk->data;
		const ExpectedBlock* expectedBlocksEnd = expectedChunk->End();

		Block* prev     = 0;
		bool   prevFree = false;
		Block* block    = (Block*)(chunk + 1);
		u32 blockNum = 0;blockNum;
		while (block->Size() > 0) {
			blockNum++;
			CheckTrue(expectedBlock < expectedBlocksEnd);
			CheckEq(prev, block->prev);
			CheckEq(prevFree, block->IsPrevFree());
			CheckTrue(!prevFree || !block->IsFree());
			
			CheckEq(expectedBlock->size, block->Size());
			if (block->IsFree()) {
				CheckTrue(expectedBlock->free);
				const Index idx = allocator->CalcIndex(block->Size());
				CheckTrue(freeBlockCount[idx.f][idx.s] > 0);
				freeBlockCount[idx.f][idx.s]--;
			} else {
				CheckTrue(!expectedBlock->free);
			}

			expectedBlock++;
			prev     = block;
			prevFree = block->IsFree();
			block    = block->Next();
		}
		CheckEq(expectedBlock, expectedBlocksEnd);
		CheckEq(prev, block->prev);
		CheckEq(prevFree, block->IsPrevFree());
		CheckTrue(!block->IsFree());
		CheckEq((u8*)chunk + chunk->size, (u8*)block + 16);
		expectedChunk++;
	}
	CheckEq(expectedChunk, expectedChunksEnd);

	for (u32 f = 0; f < FirstCount; f++) {
		for (u32 s = 0; s < SecondCount; s++) {
			CheckTrue(!freeBlockCount[f][s]);
		}
	}
}

//--------------------------------------------------------------------------------------------------

UnitTest("Tlsf") {
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
			const Index i = allocator.CalcIndex(size); \
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
		constexpr u64 memSize = 64 * 1024;
		void* mem = Sys::VirtualAlloc(memSize);
		Defer { Sys::VirtualFree(mem); };
		allocator.Init();
		allocator.AddMem(mem, memSize);
		constexpr u64 base = memSize - sizeof(allocator) - sizeof(Chunk) - 24;
		const bool f = true;	// free
		const bool u = false;	// used
		Span<Span<ExpectedBlock>> expectedChunks = {
			{ { base, f } },
		};
		CheckAllocator(&allocator, expectedChunks);

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
		expectedChunks = {
			{
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
			},
		};
		CheckAllocator(&allocator, expectedChunks);

		// free without merge
		for (u32 i = 0; i <= 30; i += 2) { allocator.Free(p[i]); }
		expectedChunks = {
			{
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
			},
		};
		CheckAllocator(&allocator, expectedChunks);

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
		// alloc fail
		tmp = allocator.Alloc(base - used + 1);
		CheckFalse(tmp);
		expectedChunks = {
			{
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
			},
		};
		CheckAllocator(&allocator, expectedChunks);

		// Extend
		CheckFalse(allocator.Extend(0, 0));
		CheckTrue (allocator.Extend(p[0], 0));
		CheckTrue (allocator.Extend(p[0], 960));
		CheckFalse(allocator.Extend(p[0], 961));
		CheckTrue (allocator.Extend(p[6], 953));	// no split
		CheckTrue (allocator.Extend(p[8], 953));	// enough for split
		allocator.Free(p[21]);	// merge prev and next
		allocator.Free(p[31]);	// merge prev and next=LAST
		used -= 1144 + 8 + 24 + 8;
		expectedChunks = {
			{
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
			},
		};
		CheckAllocator(&allocator, expectedChunks);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::TLSF