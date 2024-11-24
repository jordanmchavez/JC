#include "JC/Tlsf.h"

#include "JC/Bit.h"
#include "JC/Err.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u64 BlockFreeBit     = 1 << 0;
static constexpr u64 BlockPrevFreeBit = 1 << 1;
static constexpr u64 BlockSizeMask    = ~(BlockFreeBit | BlockPrevFreeBit);

struct Block {
	Block* prevPhys = 0;
	u64    size     = 0;
	Block* nextFree = 0;
	Block* prevFree = 0;

	u64          Size()       const { return size & BlockSizeMask; }
	bool         IsFree()     const { return size & BlockFreeBit; }
	bool         IsPrevFree() const { return size & BlockPrevFreeBit; }
	Block*       Next()             { return (Block*)((u8*)this + (size & BlockSizeMask) + 8); }
	const Block* Next()       const { return (const Block*)((u8*)this + (size & BlockSizeMask) + 8); }
};

static constexpr u32 AlignSizeLog2    = 3;
static constexpr u32 AlignSize        = 1 << AlignSizeLog2;
static constexpr u32 SecondCountLog2  = 4;
static constexpr u32 SecondCount      = 1 << SecondCountLog2;    // 16
static constexpr u32 FirstShift       = SecondCountLog2 + AlignSizeLog2; // 7
static constexpr u32 FirstMax         = 40;
static constexpr u32 FirstCount       = FirstMax - FirstShift + 1;  // 40-7+1=34
static constexpr u32 SmallBlockSize   = 1 << FirstShift; // 128
static constexpr u64 BlockSizeMin     = sizeof(Block) - 8;	// 24
static constexpr u64 BlockSizeMax     = ((u64)1 << FirstMax) - 8;	// 1TB - 8 bytes

static_assert(AlignSize == SmallBlockSize / SecondCount);

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
		//const u32 b = Bsr64(size);
		//const u64 round = ((u64)1 << (b - SecondCountLog2)) - 1;
		//const u64 roundedSize = size + round;
		//const u64 roundedSize = size + ((u64)1 << (Bsr64(size) - SecondCountLog2)) - 1;
		const u32 bit = Bsr64(size);
		//const u32 f = bit - FirstShift + 1;
		//const u32 sMask = (SecondCount - 1);
		//const u32 sXor = 1 << SecondCountLog2;
		//const u32 sShift = (bit - SecondCountLog2);
		//u32 s = (u32)(size >> sShift);
		//u32 s1 = s & sMask;
		//u32 s2 = s ^ sXor;
		//s1;s2;f;
		return Index {
			.f = bit - FirstShift + 1,
			.s = (u32)(size >> (bit - SecondCountLog2)) & (SecondCount - 1),
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
				tlsf->first &= ~((u64)1 << idx.f);
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
	Block* const block = tlsf->blocks[idx.f][idx.s];
	Assert(block->Size() >= size);
	RemoveFreeBlock(tlsf, block, idx);
	return block;
}

//--------------------------------------------------------------------------------------------------

static void Split(TlsfObj* tlsf, Block* block, u64 size)
{
	Assert(block->Size() >= size + 8);
	Assert(!block->IsPrevFree());
	const u64 remSize = block->Size() - size - 8;
	block->size = size;	// !free, !prevFree 

	Block* const rem = block->Next();
	rem->size = remSize | BlockFreeBit;
	rem->prevPhys = block;
	InsertFreeBlock(tlsf, rem);
	rem->Next()->prevPhys = rem;
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

	if (size < 16) {
		size = 16;
	} else {
		size = AlignUp(size, AlignSize);
	}
	Block* block = AllocFreeBlock(tlsf, size);
	if (!block) {
		return 0;
	}
	Assert(!block->IsPrevFree());

	if (block->Size() - size >= sizeof(Block)) {
		const u64 remSize = block->Size() - size - 8;
		block->size = size;	// !free, !prevFree 

		Block* const rem = block->Next();
		rem->size = remSize | BlockFreeBit;
		rem->prevPhys = block;
		InsertFreeBlock(tlsf, rem);
		rem->Next()->prevPhys = rem;

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

	Block* block = (Block*)((u8*)ptr - 16);
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

Res<> Tlsf::CheckIntegrity() {
	TlsfObj* const tlsf = (TlsfObj*)opaque;
	for (u32 f = 0; f < FirstCount; f++) {
		const u32 fBit = tlsf->first & (1 << f);
		if (
			( fBit && !tlsf->second[f]) ||
			(!fBit &&  tlsf->second[f])
		) {
			return MakeErr(Err_BitMapsMismatch, "f", f, "first", tlsf->first, "second", tlsf->second[f]);
		}
		for (u32 s = 0; s < SecondCount; s++) {
			const u32 sBit = tlsf->second[f] & (1 << s);
			if (
				( sBit && tlsf->blocks[f][s] == &tlsf->nullBlock) ||
				(!sBit && tlsf->blocks[f][s] != &tlsf->nullBlock)
			) {
				return MakeErr(Err_FreeBlocksMismatch, "f", f, "s", s);
			}

			#define TlsfCheck(cond, ec, ...) if (!(cond)) { return MakeErr(ec, ##__VA_ARGS__); }

			for (Block* block = tlsf->blocks[f][s]; block != &tlsf->nullBlock; block = block->Next()) {
				TlsfCheck(block->IsFree(),               Err_NotMarkedFree, "f", f, "s", s);
				TlsfCheck(block->IsPrevFree(),           Err_NotCoalesced,  "f", f, "s", s);
				TlsfCheck(!block->Next()->IsFree(),      Err_NotCoalesced,  "f", f, "s", s);
				TlsfCheck(block->Next()->IsPrevFree(),   Err_NotMarkedFree, "f", f, "s", s);
				TlsfCheck(block->Size() >= BlockSizeMin, Err_BlockTooSmall, "f", f, "s", s);
				const Index idx = CalcIndex(block->Size());
				TlsfCheck(idx.f == f && idx.s == s,      Err_BlockIndex,    "f", f, "s", s, "size", block->Size());
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

UnitTest("Tlsf") {
	SubTest("CalcIndex") {
		#define CheckCalcIndex(size, first, second) \
		{ \
			const Index i = CalcIndex(size); \
			CheckEq(i.f, (u64)first); \
			CheckEq(i.s, (u64)second); \
		}

		CheckCalcIndex(  0, 0,  0);
		CheckCalcIndex(  8, 0,  1);
		CheckCalcIndex( 16, 0,  2);
		CheckCalcIndex( 24, 0,  3);
		CheckCalcIndex( 32, 0,  4);
		CheckCalcIndex( 40, 0,  5);
		CheckCalcIndex( 48, 0,  6);
		CheckCalcIndex( 56, 0,  7);
		CheckCalcIndex( 64, 0,  8);
		CheckCalcIndex( 72, 0,  9);
		CheckCalcIndex( 80, 0, 10);
		CheckCalcIndex( 88, 0, 11);
		CheckCalcIndex( 96, 0, 12);
		CheckCalcIndex(104, 0, 13);
		CheckCalcIndex(112, 0, 14);
		CheckCalcIndex(120, 0, 15);
		CheckCalcIndex(127, 0, 15);

		CheckCalcIndex(128, 1,  0);
		CheckCalcIndex(136, 1,  1);
		CheckCalcIndex(144, 1,  2);
		CheckCalcIndex(152, 1,  3);
		CheckCalcIndex(160, 1,  4);
		CheckCalcIndex(168, 1,  5);
		CheckCalcIndex(176, 1,  6);
		CheckCalcIndex(184, 1,  7);
		CheckCalcIndex(192, 1,  8);
		CheckCalcIndex(200, 1,  9);
		CheckCalcIndex(208, 1, 10);
		CheckCalcIndex(216, 1, 11);
		CheckCalcIndex(224, 1, 12);
		CheckCalcIndex(232, 1, 13);
		CheckCalcIndex(240, 1, 14);
		CheckCalcIndex(248, 1, 15);
		CheckCalcIndex(255, 1, 15);
		CheckCalcIndex(256, 2,  0);

		// index 17 = 19 ignored zeroes
		// increment 524288
		CheckCalcIndex( 8388608, 17,  0);
		CheckCalcIndex( 8912896, 17,  1);
		CheckCalcIndex( 9437184, 17,  2);
		CheckCalcIndex( 9961472, 17,  3);
		CheckCalcIndex(10485760, 17,  4);
		CheckCalcIndex(11010048, 17,  5);
		CheckCalcIndex(11534336, 17,  6);
		CheckCalcIndex(12058624, 17,  7);
		CheckCalcIndex(12582912, 17,  8);
		CheckCalcIndex(13107200, 17,  9);
		CheckCalcIndex(13631488, 17, 10);
		CheckCalcIndex(14155776, 17, 11);
		CheckCalcIndex(14680064, 17, 12);
		CheckCalcIndex(15204352, 17, 13);
		CheckCalcIndex(15728640, 17, 14);
		CheckCalcIndex(16252928, 17, 15);
		CheckCalcIndex(16777215, 17, 15);
		CheckCalcIndex(16777216, 18,  0);

		// index 33 = 35 ignored zeros
		// 1000000000000000000000000000000000000000 = 549755813888 = 0x80 0000 0000
		// increment 100000000000000000000000000000000000 = 34359738368 = 0x8 0000 0000
		CheckCalcIndex((u64)0x8000000000, 33,  0);
		CheckCalcIndex((u64)0x8800000000, 33,  1);
		CheckCalcIndex((u64)0x9000000000, 33,  2);
		CheckCalcIndex((u64)0x9800000000, 33,  3);
		CheckCalcIndex((u64)0xa000000000, 33,  4);
		CheckCalcIndex((u64)0xa800000000, 33,  5);
		CheckCalcIndex((u64)0xb000000000, 33,  6);
		CheckCalcIndex((u64)0xb800000000, 33,  7);
		CheckCalcIndex((u64)0xc000000000, 33,  8);
		CheckCalcIndex((u64)0xc800000000, 33,  9);
		CheckCalcIndex((u64)0xd000000000, 33, 10);
		CheckCalcIndex((u64)0xd800000000, 33, 11);
		CheckCalcIndex((u64)0xe000000000, 33, 12);
		CheckCalcIndex((u64)0xe800000000, 33, 13);
		CheckCalcIndex((u64)0xf000000000, 33, 14);
		CheckCalcIndex((u64)0xf800000000, 33, 15);
		CheckCalcIndex((u64)0xf8ffffffff, 33, 15);

		CheckCalcIndex((u64)0x10000000000, 34, 0);
	}

	SubTest("Main Flow") {
		constexpr u64 permSize = 4 * 1024 * 1024;
		u8* const perm = (u8*)Sys::VirtualAlloc(permSize);
		Tlsf tlsf;
		tlsf.Init(perm, permSize);
		TlsfObj* obj = (TlsfObj*)tlsf.opaque;

		CheckEq(obj->first, 1 << 15);
		CheckEq(obj->second[15], 1 << 15);
		const Block* block = obj->blocks[15][15];
		CheckEq(block->Size(), permSize - sizeof(TlsfObj) - 16);
		Check(block->IsFree());
		Check(!block->IsPrevFree());
		const Block* next = block->Next();
		CheckEq(next->Size(), 0);
		Check(!next->IsFree());
		Check(next->IsPrevFree());

		void* p = tlsf.Alloc(1);
		CheckEq(obj->first, 1 << 15);
		CheckEq(obj->second[15], 1 << 15);
		block = obj->blocks[15][15];
		CheckEq(block->Size(), permSize - sizeof(TlsfObj) - 16 - 24);
		Check(block->IsFree());
		Check(!block->IsPrevFree());
		next = block->Next();
		CheckEq(next->Size(), 0);
		Check(!next->IsFree());
		Check(next->IsPrevFree());

		tlsf.Free(p);

		Sys::VirtualFree(perm);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC