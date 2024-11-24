#include "JC/Tlsf.h"

#include "JC/Bit.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u32 AlignSizeLog2    = 3;
static constexpr u32 AlignSize        = 1 << AlignSizeLog2;
static constexpr u32 SecondCountLog2  = 4;
static constexpr u32 SecondCount      = 1 << SecondCountLog2;    // 16
static constexpr u32 FirstShift       = SecondCountLog2 + AlignSizeLog2; // 8
static constexpr u32 FirstMax         = 40;
static constexpr u32 FirstCount       = FirstMax - FirstShift + 1;  // 64 - 8 + 1 = 57
static constexpr u32 SmallBlockSize   = 1 << FirstShift; // 256
static constexpr u64 BlockFreeBit     = 1 << 0;
static constexpr u64 BlockPrevFreeBit = 1 << 1;
static constexpr u64 BlockSizeMask    = ~(BlockFreeBit | BlockPrevFreeBit);

static_assert(AlignSize == SmallBlockSize / SecondCount);

struct Block {
	Block* prevPhys = 0;
	u64    size     = 0;
	Block* nextFree = 0;
	Block* prevFree = 0;

	u64    Size()       const { return size & BlockSizeMask; }
	bool   IsFree()     const { return size & BlockFreeBit; }
	bool   IsPrevFree() const { return size & BlockPrevFreeBit; }
	Block* Next()             { return (Block*)((u8*)this + (size & BlockSizeMask) + 8); }
};

static constexpr u64 BlockSizeMin = sizeof(Block) - 8;	// 24
static constexpr u64 BlockSizeMax = ((u64)1 << FirstMax) - 8;


struct Index {
	u32 f;
	u32 s;
};

struct TlsfObj {
    Block  nullBlock                       = {};
    u64    first                           = 0;
    u64    second[FirstCount]              = {};
    Block* blocks[FirstCount][SecondCount] = {};
};

//--------------------------------------------------------------------------------------------------

static Index CalcIndex(u64 size) {
	if (size < SmallBlockSize) {
		return Index {
			.f = 0,
			.s = (u32)size / (SmallBlockSize / SecondCount),
		};
	} else {
		const u64 roundedSize = size + ((u64)1 << (Bsr64(size) - SecondCountLog2)) - 1;
		const u32 bit = Bsr64(roundedSize);
		return Index {
			.f = bit - FirstShift + 1,
			.s = (u32)(roundedSize >> (bit - SecondCountLog2)) & ~(SecondCount - 1),
		};
	}
}

//--------------------------------------------------------------------------------------------------

static void InsertFreeBlock(TlsfObj* tlsf, Block* block) {
	const Index idx = CalcIndex(block->Size());
	Assert((u64)block % AlignSize  == 0);
	block->nextFree = tlsf->blocks[idx.f][idx.s];
	block->prevFree = &tlsf->nullBlock;
	tlsf->blocks[idx.f][idx.s]->prevFree = block;
	tlsf->blocks[idx.f][idx.s]           = block;
	tlsf->first         |= (u64)1 << idx.f;
	tlsf->second[idx.f] |= (u64)1 << idx.s;
}

//--------------------------------------------------------------------------------------------------

static void RemoveFreeBlock(TlsfObj* tlsf, Block* block, Index idx) {
	Block* const n = block->nextFree;
	Block* const p = block->prevFree;
	n->prevFree = p;
	p->nextFree = n;
	if (tlsf->blocks[idx.f][idx.s] == block) {
		tlsf->blocks[idx.f][idx.s] = n;
		if (n == &tlsf->nullBlock) {
			tlsf->second[idx.f] &= ~((u64)1 << idx.s);
			if (!tlsf->second[idx.f]) {
				tlsf->first &= ((u64)1 << idx.f);
			}
		}
	}
}

static void RemoveFreeBlock(TlsfObj* tlsf, Block* block) { RemoveFreeBlock(tlsf, block, CalcIndex(block->Size()));  }

//--------------------------------------------------------------------------------------------------

static Block* AllocFreeBlock(TlsfObj* tlsf, u64 size) {
	Index idx = CalcIndex(size);
	u64 secondMap = tlsf->second[idx.f] & ((u64)(-1) << idx.s);
	if (!secondMap) {
		const u32 firstMap = tlsf->first & ((u64)(-1) << (idx.f + 1));
		if (!firstMap) {
			return nullptr;
		}
		idx.f = Bsf64(firstMap);
		secondMap = tlsf->second[idx.f];
		Assert(secondMap);
	}
	idx.s = Bsf64(secondMap);
	Block* block = tlsf->blocks[idx.f][idx.s];
	Assert((block->size & BlockSizeMask) >= size);
	RemoveFreeBlock(tlsf, block, idx);
	return block;
}

//--------------------------------------------------------------------------------------------------

static void Split(TlsfObj* tlsf, Block* block, u64 size)
{
	Block* const rem = block->Next();
	const u64 remSize = block->Size() - size - 8;
	rem->size = remSize | BlockFreeBit;
	block->size = size;
	rem->prevPhys = block;
	InsertFreeBlock(tlsf, rem);
}

//--------------------------------------------------------------------------------------------------

void Tlsf::Init(void* ptr, u64 size) {
	opaque = (u64)ptr;
	TlsfObj* tlsf = (TlsfObj*)ptr;

	tlsf->nullBlock.nextFree = &tlsf->nullBlock;
	tlsf->nullBlock.prevFree = &tlsf->nullBlock;

	tlsf->first = 0;
	MemSet(tlsf->second, 0, sizeof(tlsf->second));
	for (u32 i = 0; i < FirstCount; i++) {
		for (u32 j = 0; j < SecondCount; j++) {
			tlsf->blocks[i][j] = &tlsf->nullBlock;
		}
	}

	void* const chunk    = AlignPtrUp((u8*)ptr + sizeof(TlsfObj), AlignSize);
	u64 const chunkSize = size - (u64)((u8*)chunk - (u8*)ptr);
	AddChunk(chunk, chunkSize);
}

//--------------------------------------------------------------------------------------------------

void Tlsf::AddChunk(void* ptr, u64 size){
	TlsfObj* const tlsf = (TlsfObj*)opaque;

	Assert(ptr);
	Assert((u64)ptr % AlignSize == 0);
	Assert(size % AlignSize == 0);

	Block* const block  = (Block*)((u8*)ptr - 8);
	block->size         = (size - 16) | BlockFreeBit;
	Block* const last   = block->Next();
	last->prevPhys      = block;
	last->size          = 0 | BlockPrevFreeBit;
	InsertFreeBlock(tlsf, block);
}

//--------------------------------------------------------------------------------------------------

void* Tlsf::Alloc(u64 size) {
	TlsfObj* const tlsf = (TlsfObj*)opaque;

	Assert(size);
	Assert(size <= BlockSizeMax);

	size = Max(AlignUp(size, AlignSize), BlockSizeMin);
	Block* block = AllocFreeBlock(tlsf, size);
	if (!block) {
		return 0;
	}

	if (block->Size() - size >= sizeof(Block)) {
		Split(tlsf, block, size);
	} else {
		block->size &= ~(BlockFreeBit | BlockPrevFreeBit);
		block->Next()->size |= BlockPrevFreeBit;
	}

	return (u8*)block + 16;
}

//--------------------------------------------------------------------------------------------------

bool Tlsf::Extend(void* ptr, u64 size) {
	TlsfObj* const tlsf = (TlsfObj*)opaque;

	Assert(ptr);
	size = Max(AlignUp(size, AlignSize), BlockSizeMin);
	Assert(size <= BlockSizeMax);

	Block* const block = (Block*)((u8*)ptr - 16);
	const u64 blockSize = block->Size();
	if (blockSize - 8 >= size) {
		return true;
	}

	Block* next = block->Next();
	if (!next->IsFree()) {
		return false;
	}

	const u64 nextSize = next->Size();
	const u64 combinedSize = blockSize + nextSize + 8;
	if (combinedSize < size) {
		return false;
	}

	RemoveFreeBlock(tlsf, next, CalcIndex(nextSize));
	block->size += nextSize + 8;	// doesn't affect flags
	next = block->Next();
	Assert(!next->IsFree());
	next->prevPhys = block;
	next->size &= ~(BlockFreeBit & BlockPrevFreeBit);

	if (block->Size()- size >= sizeof(Block)) {
		Split(tlsf, block, size);
	}

	return true;
}

//--------------------------------------------------------------------------------------------------

void Tlsf::Free(void* ptr) {
	TlsfObj* const tlsf = (TlsfObj*)opaque;

	Block* block = (Block*)((u8*)ptr - 8);
	Assert(!block->IsFree());
	block->size |= BlockFreeBit;
	Block* const next = block->Next();
	next->size |= BlockPrevFreeBit;

	if (block->IsPrevFree()) {
		Block* const prev = block->prevPhys;
		RemoveFreeBlock(tlsf, prev);
		prev->size += block->Size() + 8;
		block = prev;
	}
	next->prevPhys = block;

	if (next->size & BlockFreeBit) {
		RemoveFreeBlock(tlsf, next);
		block->size += next->Size() + 8;
		block->Next()->prevPhys = block;
	}

	InsertFreeBlock(tlsf, block);
}

//--------------------------------------------------------------------------------------------------

UnitTest("Tlsf") {
	constexpr u64 permSize = 4 * 1024 * 1024;
	void* const perm = Sys::VirtualAlloc(permSize);

	Tlsf tlsf;
	tlsf.Init(perm, permSize);


	Sys::VirtualFree(perm);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC