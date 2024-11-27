#include "JC/Common.h"
#include "JC/Bit.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u64 AlignSize      = 8;
static constexpr u64 AlignSizeLog2  = 3;
static constexpr u64 FirstMax       = 40;
static constexpr u64 SecondCount    = 5;
static constexpr u64 SecondCount    = 32;
static constexpr u64 FirstShift     = 8;	// SecondCount + AlignSizeLog2
static constexpr u64 FirstCount     = 33;	// FirstMax - FirstShift + 1
static constexpr u64 SmallBlockSize = 256;	// 1 << FirstShift
static constexpr u64 FreeBit        = 1 << 0;
static constexpr u64 PrevFreeBit    = 1 << 1;
static constexpr u64 BlockSizeMin   = 24;	// sizeof(Block) - 8;
static constexpr u64 BlockSizeMax   = (u64)1 << FirstMax;

struct Block {
	Block* prev;
	u64    size;
	Block* nextFree;
	Block* prevFree;

	constexpr u64          Size()       const { return size & ~(FreeBit | PrevFreeBit); }
	constexpr bool         IsFree()     const { return size & FreeBit; }
	constexpr bool         IsPrevFree() const { return size & PrevFreeBit; }
	constexpr       Block* Next()             { return       (Block*)((u8*)this + Size() + 8); }
	constexpr const Block* Next()       const { return (const Block*)((u8*)this + Size() + 8); }
};

struct Index {
	u64 f;
	u64 s;
};

struct Ctx {
	Block  nullBlock;
	u64    first;
	u64    second[FirstCount];
	Block* blocks[FirstCount][SecondCount];
};

static void block_set_size(Block* block, u64 size)
{
	const u64 oldsize = block->size;
	block->size = size | (oldsize & (FreeBit | PrevFreeBit));
}


Index CalcIndex(u64 size) {
	if (size < SmallBlockSize) {
		return Index {
			.f = 0,
			.s = size / (SmallBlockSize / SecondCount),
		};
	} else {
		const u32 bit = Bsr64(size);
		return Index {
			.f = bit - (FirstShift - 1),
			.s = (size >> (bit - SecondCount)) ^ (1 << SecondCount),
		};
	}
}

void Tlsf_RemoveBlock(Ctx* ctx, Block* block, Index idx) {
	Block* const prev = block->prevFree;
	Block* const next = block->nextFree;
	next->prevFree = prev;
	prev->nextFree = next;
	if (ctx->blocks[idx.f][idx.s] == block) {
		ctx->blocks[idx.f][idx.s] = next;
		if (next == &ctx->nullBlock) {
			ctx->second[idx.f] &= ~((u64)1 << idx.s);
			if (!ctx->second[idx.f]) {
				ctx->first &= ~((u64)1 << idx.f);
			}
		}
	}
}

void Tlsf_RemoveBlock(Ctx* ctx, Block* block) {
	Tlsf_RemoveBlock(ctx, block, CalcIndex(block->Size()));
}

void Tlsf_InsertBlock(Ctx* ctx, Block* block, Index idx) {
	Block* current = ctx->blocks[idx.f][idx.s];
	block->nextFree = current;
	block->prevFree = &ctx->nullBlock;
	current->prevFree = block;
	ctx->blocks[idx.f][idx.s] = block;
	ctx->first |= ((u64)1 << idx.f);
	ctx->second[idx.f] |= ((u64)1 << idx.s);
}

void Tlsf_InsertBlock(Ctx* ctx, Block* block) {
	Tlsf_InsertBlock(ctx, block, CalcIndex(block->Size()));
}

static void control_construct(Ctx* ctx)
{
	int i, j;

	ctx->nullBlock.nextFree = &ctx->nullBlock;
	ctx->nullBlock.prevFree = &ctx->nullBlock;

	ctx->first = 0;
	for (i = 0; i < FirstCount; ++i)
	{
		ctx->second[i] = 0;
		for (j = 0; j < SecondCount; ++j)
		{
			ctx->blocks[i][j] = &ctx->nullBlock;
		}
	}
}

pool_t tlsf_add_pool(Ctx* ctx, void* mem, u64 bytes)
{
	Block* block;
	Block* next;

	const u64 pool_overhead = 16;
	const u64 pool_bytes = align_down(bytes - pool_overhead, AlignSize);

	block = (Block*)((u8*)mem - 8);
	block_set_size(block, pool_bytes);
	block->size |= FreeBit;
	block->size &= ~PrevFreeBit;
	Tlsf_InsertBlock(tlsf_cast(Ctx*, ctx), block);

	/* Split the block to create a zero-size sentinel block. */
	Block* next = block->Next();
	next->prev = block;

	block_set_size(next, 0);
	next->size &= ~FreeBit;
	next->size |= PrevFreeBit;

	return mem;
}

void* Tlsf_Alloc(Ctx* ctx, u64 size) {
	if (!size) {
		return 0;
	}
	Assert(size < BlockSizeMax);

	size = Max(AlignUp(size, AlignSize), BlockSizeMin);
	if (size >= SmallBlockSize) {
		size += ((u64)1 << (Bsr64(size) - SecondCount)) - 1;
	}

	Index idx = CalcIndex(size);
	u64 sMap = ctx->second[idx.f] & ((u64)-1 << idx.s);
	if (!sMap) {
		const u64 fMap = ctx->first & ((u64)-1 << (idx.f + 1));
		if (!fMap) {
			return 0;
		}
		idx.f = Bsf64(fMap);
		sMap = ctx->second[idx.f];
		Assert(sMap);
	}
	idx.s = Bsf64(sMap);

	Block* block = ctx->blocks[idx.f][idx.s];
	Assert(block != &ctx->nullBlock);
	Assert(block->Size() >= size);
	Assert(block->IsFree());
	Assert(!block->IsPrevFree());

	Tlsf_RemoveBlock(ctx, block, idx);

	if (block->Size() >= sizeof(Block) + size) {
		Block* rem = (Block*)((u8*)block + size + 8);
		rem->prev = block;
		rem->size = (block->Size() - size - 8) | FreeBit;

		block->size = size;

		Block* next = rem->Next();
		Assert(next->IsPrevFree());
		next->prev = rem;

		Tlsf_InsertBlock(ctx, rem);
	}

	Block* next = block->Next();
	next->size &= ~PrevFreeBit;
	block->size &= ~FreeBit;

	return (u8*)block + 16;
}

bool Tlsf_Extend(Ctx* ctx, void* ptr, u64 size) {
	if (!ptr) {
		return false;
	}

	size = Max(AlignUp(size, AlignSize), BlockSizeMin);
	Assert(size < BlockSizeMax);

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

	const u64 combined = blockSize + next->Size() + 8;
	if (size > combined) {
		return false;
	}

	Tlsf_RemoveBlock(ctx, next);

	block->size += next->Size() + 8;
	next = block->Next();
	next->prev = block;
	next->size &= ~PrevFreeBit;

	if (block->Size() >= sizeof(Block) + size) {
		block->size = size;

		Block* rem = (Block*)((u8*)block + size + 8);
		rem->prev = block;
		rem->size = (block->Size() - size - 8) | FreeBit;

		next->prev = rem;
		next->size |= PrevFreeBit;

		Tlsf_InsertBlock(ctx, rem);
	}
	return true;
}

void Tlsf_Free(Ctx* ctx, void* ptr) {
	if (!ptr) {
		return;
	}

	Block* block = (Block*)((u8*)ptr - 16);
	Assert(!block->IsFree());
	block->size |= FreeBit;

	Block* const next = block->Next();
	next->size |= PrevFreeBit;

	if (block->IsPrevFree()) {
		Block* prev = block->prev;
		Assert(prev->IsFree());
		Tlsf_RemoveBlock(ctx, prev);
		prev->size += block->Size() + 8;
		next->prev = prev;
		block = prev;
	}

	if (next->IsFree()) {
		Tlsf_RemoveBlock(ctx, next);
		block->size += next->Size() + 8;
		block->Next()->prev = block;
	}

	Tlsf_InsertBlock(ctx, block);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC