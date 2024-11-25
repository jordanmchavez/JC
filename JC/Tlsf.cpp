#include "JC/Tlsf.h"

#include "JC/Bit.h"
#include "JC/Err.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u64 BlockFreeBit     = 1 << 0;
static constexpr u64 BlockPrevFreeBit = 1 << 1;

struct Block {
	Block* prevPhys = 0;
	u64    size     = 0;
	Block* nextFree = 0;
	Block* prevFree = 0;

	u64          Size()       const { return size & ~(BlockFreeBit | BlockPrevFreeBit); }
	bool         IsFree()     const { return size & BlockFreeBit; }
	bool         IsPrevFree() const { return size & BlockPrevFreeBit; }
	Block*       Next()             { return (Block*)((u8*)this + Size() + 8); }
	const Block* Next()       const { return (const Block*)((u8*)this + Size() + 8); }
};

struct Chunk {
	u64    size = 0;
	Chunk* next = 0;
};
static_assert(sizeof(Chunk) % 8 == 0);
static_assert(alignof(Chunk) == 8);

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

struct Ctx {
    Block  nullBlock                       = {};
    u64    first                           = 0;
    u64    second[FirstCount]              = {};
    Block* blocks[FirstCount][SecondCount] = {};
	Chunk* chunks                          = 0;
};
static_assert(sizeof(Ctx) % 8 == 0);
static_assert(alignof(Ctx) == 8);

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

static void InsertFreeBlock(Ctx* ctx, Block* block) {
	const Index idx = CalcIndex(block->Size());
	Assert((u64)block % AlignSize  == 0);
	block->nextFree = ctx->blocks[idx.f][idx.s];
	block->prevFree = &ctx->nullBlock;
	ctx->blocks[idx.f][idx.s]->prevFree = block;
	ctx->blocks[idx.f][idx.s]           = block;
	ctx->first         |= (u64)1 << idx.f;
	ctx->second[idx.f] |= (u64)1 << idx.s;
}

//--------------------------------------------------------------------------------------------------

static void RemoveFreeBlock(Ctx* ctx, Block* block, Index idx) {
	Block* const n = block->nextFree;
	Block* const p = block->prevFree;
	n->prevFree = p;
	p->nextFree = n;
	if (ctx->blocks[idx.f][idx.s] == block) {
		ctx->blocks[idx.f][idx.s] = n;
		if (n == &ctx->nullBlock) {
			ctx->second[idx.f] &= ~((u64)1 << idx.s);
			if (!ctx->second[idx.f]) {
				ctx->first &= ~((u64)1 << idx.f);
			}
		}
	}
}

static void RemoveFreeBlock(Ctx* ctx, Block* block) { RemoveFreeBlock(ctx, block, CalcIndex(block->Size()));  }

//--------------------------------------------------------------------------------------------------

static Block* AllocFreeBlock(Ctx* ctx, u64 size) {
	Index idx = CalcIndex(size);
	u64 secondMap = ctx->second[idx.f] & ((u64)(-1) << idx.s);
	if (!secondMap) {
		const u32 firstMap = ctx->first & ((u64)(-1) << (idx.f + 1));
		if (!firstMap) {
			return nullptr;
		}
		idx.f = Bsf64(firstMap);
		secondMap = ctx->second[idx.f];
		Assert(secondMap);
	}
	idx.s = Bsf64(secondMap);
	Block* const block = ctx->blocks[idx.f][idx.s];
	Assert(block->Size() >= size);
	RemoveFreeBlock(ctx, block, idx);
	return block;
}

//--------------------------------------------------------------------------------------------------

static void Split(Ctx* ctx, Block* block, u64 size)
{
	Assert(block->Size() >= size + 8);
	Assert(!block->IsPrevFree());
	const u64 remSize = block->Size() - size - 8;
	block->size = size;	// !free, !prevFree 

	Block* const rem = block->Next();
	rem->size = remSize | BlockFreeBit;
	rem->prevPhys = block;
	InsertFreeBlock(ctx, rem);
	rem->Next()->prevPhys = rem;
}

//--------------------------------------------------------------------------------------------------

void Tlsf::Init(void* ptr, u64 size) {
	opaque = (u64)ptr;
	Ctx* ctx = (Ctx*)ptr;

	ctx->nullBlock.nextFree = &ctx->nullBlock;
	ctx->nullBlock.prevFree = &ctx->nullBlock;

	ctx->first = 0;
	MemSet(ctx->second, 0, sizeof(ctx->second));
	for (u32 i = 0; i < FirstCount; i++) {
		for (u32 j = 0; j < SecondCount; j++) {
			ctx->blocks[i][j] = &ctx->nullBlock;
		}
	}

	ctx->chunks = 0;
	void* const chunk = AlignPtrUp((u8*)ptr + sizeof(Ctx), AlignSize);
	AddChunk(chunk, size - (u64)((u8*)chunk - (u8*)ptr));
}

//--------------------------------------------------------------------------------------------------

void Tlsf::AddChunk(void* ptr, u64 size){
	Ctx* const ctx = (Ctx*)opaque;

	Assert(ptr);
	Assert((u64)ptr % AlignSize == 0);
	Assert(size % AlignSize == 0);

	Chunk* chunk = (Chunk*)ptr;
	chunk->next = ctx->chunks;
	chunk->size = size;
	ctx->chunks = chunk;


	Block* const block  = (Block*)((u8*)ptr + sizeof(Chunk) - 8);
	block->prevPhys     = 0;
	block->size         = (size - sizeof(Chunk) - 16) | BlockFreeBit;
	Block* const last   = block->Next();
	last->prevPhys      = block;
	last->size          = 0 | BlockPrevFreeBit;
	InsertFreeBlock(ctx, block);
}

//--------------------------------------------------------------------------------------------------

void* Tlsf::Alloc(u64 size) {
	Ctx* const ctx = (Ctx*)opaque;

	Assert(size);
	Assert(size <= BlockSizeMax);

	if (size < 16) {
		size = 16;
	} else {
		size = AlignUp(size, AlignSize);
	}
	Block* block = AllocFreeBlock(ctx, size);
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
		InsertFreeBlock(ctx, rem);
		rem->Next()->prevPhys = rem;

	} else {
		block->size &= ~(BlockFreeBit | BlockPrevFreeBit);
		block->Next()->size |= BlockPrevFreeBit;
	}

	return (u8*)block + 16;
}

//--------------------------------------------------------------------------------------------------

bool Tlsf::Extend(void* ptr, u64 size) {
	Ctx* const ctx = (Ctx*)opaque;

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

	RemoveFreeBlock(ctx, next, CalcIndex(nextSize));
	block->size += nextSize + 8;	// doesn't affect flags
	next = block->Next();
	Assert(!next->IsFree());
	next->prevPhys = block;
	next->size &= ~(BlockFreeBit & BlockPrevFreeBit);

	if (block->Size()- size >= sizeof(Block)) {
		Split(ctx, block, size);
	}

	return true;
}

//--------------------------------------------------------------------------------------------------

void Tlsf::Free(void* ptr) {
	Ctx* const ctx = (Ctx*)opaque;

	Block* block = (Block*)((u8*)ptr - 16);
	Assert(!block->IsFree());
	block->size |= BlockFreeBit;
	Block* const next = block->Next();
	next->size |= BlockPrevFreeBit;

	if (block->IsPrevFree()) {
		Block* const prev = block->prevPhys;
		RemoveFreeBlock(ctx, prev);
		prev->size += block->Size() + 8;
		block = prev;
	}
	next->prevPhys = block;

	if (next->size & BlockFreeBit) {
		RemoveFreeBlock(ctx, next);
		block->size += next->Size() + 8;
		block->Next()->prevPhys = block;
	}

	InsertFreeBlock(ctx, block);
}

//--------------------------------------------------------------------------------------------------

#define TlsfCheck(cond, ec, ...) if (!(cond)) { return MakeErr(ec, ##__VA_ARGS__); }

Res<> Tlsf::Check() {
	u32 freeBlockCount[FirstCount][SecondCount] = {};

	Ctx* const ctx = (Ctx*)opaque;
	for (u32 f = 0; f < FirstCount; f++) {
		const u64 fBit = ctx->first & ((u64)1 << f);
		if (
			( fBit && !ctx->second[f]) ||
			(!fBit &&  ctx->second[f])
		) {
			return MakeErr(Err_BitMapsMismatch, "f", f, "first", ctx->first, "second", ctx->second[f]);
		}
		for (u32 s = 0; s < SecondCount; s++) {
			const u64 sBit = ctx->second[f] & ((u64)1 << s);
			if (
				( sBit && ctx->blocks[f][s] == &ctx->nullBlock) ||
				(!sBit && ctx->blocks[f][s] != &ctx->nullBlock)
			) {
				return MakeErr(Err_FreeBlocksMismatch, "f", f, "s", s);
			}

			for (Block* block = ctx->blocks[f][s]; block != &ctx->nullBlock; block = block->Next()) {
				TlsfCheck(block->IsFree(),               Err_NotMarkedFree, "f", f, "s", s);
				TlsfCheck(block->IsPrevFree(),           Err_NotCoalesced,  "f", f, "s", s);
				TlsfCheck(!block->Next()->IsFree(),      Err_NotCoalesced,  "f", f, "s", s);
				TlsfCheck(block->Next()->IsPrevFree(),   Err_NotMarkedFree, "f", f, "s", s);
				TlsfCheck(block->Size() >= BlockSizeMin, Err_BlockTooSmall, "f", f, "s", s);
				const Index idx = CalcIndex(block->Size());
				TlsfCheck(idx.f == f && idx.s == s,      Err_BlockBadIndex, "f", f, "s", s, "size", block->Size());
				freeBlockCount[f][s]++;
			}
		}
	}

	// TODO: count free blocks of each f/s size and ensure chunk walk covers them all

	for (Chunk* chunk = ctx->chunks; chunk; chunk = chunk->next) {
		Block* block = (Block*)((u8*)chunk + sizeof(Chunk) - 8);
		bool prevFree = block->IsPrevFree();
		Block* prev = block;
		TlsfCheck(!block->prevPhys, Err_BadFirstBlock);
		TlsfCheck(!prevFree,        Err_BadFirstBlock);
		while (block->Size() > 0) {
			TlsfCheck(prev == block->prevPhys,         Err_PrevBlockMismatch);
			TlsfCheck(prevFree == block->IsPrevFree(), Err_PrevBlockMismatch);
			TlsfCheck(!(prevFree && block->IsFree()),  Err_NotCoalesced);
			
			if (block->IsFree()) {
				const Index idx = CalcIndex(block->Size());
				TlsfCheck(freeBlockCount[idx.f][idx.s] > 0, Err_TooManyFreeBlocks, "f", idx.f, "s", idx.s);
				freeBlockCount[idx.f][idx.s]--;
			}

			prev = block;
			prevFree = block->IsPrevFree();
			block = block->Next();
		}
		TlsfCheck(prev == block->prevPhys,         Err_PrevBlockMismatch);
		TlsfCheck(prevFree == block->IsPrevFree(), Err_PrevBlockMismatch);
		TlsfCheck(!block->IsFree(),                Err_NotCoalesced);
	}

	for (u32 f = 0; f < FirstCount; f++) {
		for (u32 s = 0; s < SecondCount; s++) {
			TlsfCheck(!freeBlockCount[f][s], Err_NotEnoughFreeBlocks, "f", f, "s", s, "free", freeBlockCount[f][s]);
		}
	}

	return Ok();
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
		CheckCalcIndex((u64)0x10000000000, 34, 0);
	}

	SubTest("Main Flow") {
		constexpr u64 permSize = 4 * 1024 * 1024;
		u8* const perm = (u8*)Sys::VirtualAlloc(permSize);
		Tlsf tlsf;
		tlsf.Init(perm, permSize);
		CheckTrue(tlsf.Check());

		void* p = tlsf.Alloc(1);
		CheckTrue(tlsf.Check());

		tlsf.Free(p);
		CheckTrue(tlsf.Check());

		Sys::VirtualFree(perm);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC