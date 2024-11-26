#include "JC/Tlsf.h"

#include "JC/Bit.h"
#include "JC/Err.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u64 FreeBit     = 1 << 0;
static constexpr u64 PrevFreeBit = 1 << 1;

struct Block {
	Block* prev = 0;
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
	block->prev     = 0;
	block->size         = (size - sizeof(Chunk) - 16) | FreeBit;
	Block* const last   = block->Next();
	last->prev      = block;
	last->size          = 0 | PrevFreeBit;
	InsertFreeBlock(ctx, block);
}

//--------------------------------------------------------------------------------------------------

void* Tlsf::Alloc(u64 size) {
	Ctx* const ctx = (Ctx*)opaque;

	if (!size) {
		return 0;
	}
	Assert(size < BlockSizeMax);

	size = Max(AlignUp(size, AlignSize), BlockSizeMin);
	if (size >= SmallBlockSize) {
		size += ((u64)1 << (Bsr64(size) - SecondCountLog2)) - 1;
	}

	Index idx = CalcIndex(size);
	u64 sMap = ctx->second[idx.f] & ((u64)-1 << idx.s);
	if (!sMap) {
		const u32 fMap = ctx->first & ((u64)-1 << (idx.f + 1));
		if (!fMap) {
			return 0;
		}
		idx.f = Bsf64(fMap);
		sMap = ctx->second[idx.f];
	}
	Assert(sMap);
	idx.s = Bsf64(sMap);

	Block* const block = ctx->blocks[idx.f][idx.s];
	Assert(block != &ctx->nullBlock);
	Assert(block->Size() >= size);
	Assert(block->IsFree());
	Assert(!block->IsPrevFree());
	
	RemoveFreeBlock(ctx, block, idx);

	Block* const next = block->Next();
	Assert(next->IsPrevFree());
	if (block->Size() >= sizeof(Block) + size) {
		Block* const rem = (Block*)((u8*)block + size + 8);
		rem->prev = block;
		rem->size = (block->Size() - size - 8) | FreeBit;
		block->size = size;	// !free, !prevFree 
		next->prev = rem;

		InsertFreeBlock(ctx, rem);

	} else {
		block->size &= ~FreeBit;
		next->size |= PrevFreeBit;
	}

	return (u8*)block + 16;
}

//--------------------------------------------------------------------------------------------------

bool Tlsf::Extend(void* ptr, u64 size) {
	Ctx* const ctx = (Ctx*)opaque;

	if (!ptr) {
		return false;
	}

	size = Max(AlignUp(size, AlignSize), BlockSizeMin);
	Assert(size <= BlockSizeMax);

	Block* const block = (Block*)((u8*)ptr - 16);
	Assert(!block->IsFree());

	const u64 blockSize = block->Size();
	if (size <= blockSize) {
		return true;
	}

	Block* next = block->Next();
	if (!next->IsFree()) {
		return false;
	}

	const u64 nextSize = next->Size();
	const u64 combinedSize = blockSize + nextSize + 8;
	if (size > combinedSize) {
		return false;
	}

	RemoveFreeBlock(ctx, next, CalcIndex(nextSize));

	block->size += nextSize + 8;	// doesn't affect flags
	next = block->Next();
	next->prev = block;
	next->size &= ~ PrevFreeBit;

	if (block->Size() >= sizeof(Block) + size) {
		block->size = size;

		Block* const rem = block->Next();
		rem->prev = block;
		rem->size = (block->Size() - size - 8) | FreeBit;

		next->prev = rem;
		next->size |= PrevFreeBit;

		InsertFreeBlock(ctx, rem);
	}

	return true;
}

//--------------------------------------------------------------------------------------------------

void Tlsf::Free(void* ptr) {
	Ctx* const ctx = (Ctx*)opaque;

	if (!ptr) {
		return;
	}

	Block* block = (Block*)((u8*)ptr - 16);
	Assert(!block->IsFree());
	block->size |= FreeBit;

	Block* const next = block->Next();
	next->size |= PrevFreeBit;

	if (block->IsPrevFree()) {
		Block* const prev = block->prev;
		Assert(prev->IsFree());
		RemoveFreeBlock(ctx, prev);
		prev->size += block->Size() + 8;
		next->prev = prev;
		block = prev;
	}
	if (next->IsFree()) {
		RemoveFreeBlock(ctx, next);
		block->size += next->Size() + 8;
		block->Next()->prev = block;
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
		TlsfCheck(!block->prev, Err_BadFirstBlock);
		TlsfCheck(!prevFree,        Err_BadFirstBlock);
		while (block->Size() > 0) {
			TlsfCheck(prev == block->prev,         Err_PrevBlockMismatch);
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
		TlsfCheck(prev == block->prev,         Err_PrevBlockMismatch);
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