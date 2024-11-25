#include "JC/Common.h"
#include "JC/Bit.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u64 SecondCountLog2 = 5;
static constexpr u64 AlignSizeLog2   = 3;
static constexpr u64 AlignSize       = 8;
static constexpr u64 FirstMax        = 32;
static constexpr u64 SecondCount     = (1 << SecondCountLog2);
static constexpr u64 FirstShift      = (SecondCountLog2 + AlignSizeLog2);
static constexpr u64 FirstCount      = (FirstMax - FirstShift + 1);
static constexpr u64 SmallBlockSize  = (1 << FirstShift);
static constexpr u64 FreeBit         = 1 << 0;
static constexpr u64 PrevFreeBit     = 1 << 1;
static constexpr u64 BlockSizeMax    = (u64)1 << FirstMax;

struct Block {
	Block* prevPhys;
	u64	size;
	Block* nextFree;
	Block* prevFree;

	constexpr u64          Size()       const { return size & ~(FreeBit | PrevFreeBit); }
	constexpr bool         IsFree()     const { return size & FreeBit; }
	constexpr bool         IsPrevFree() const { return size & PrevFreeBit; }
	constexpr Block*	   Next()             { return (Block*)((u8*)this + Size() + 8); }
	constexpr const Block* Next()       const { return (const Block*)((u8*)this + Size() + 8); }
};

struct Index {
	u64 f;
	u64 s;
};

static const u64 BlockSizeMin =  sizeof(Block) - 8;

struct Tlsf
{
	Block nullBlock;
	unsigned int first;
	unsigned int second[FirstCount];
	Block* blocks[FirstCount][SecondCount];
};

static void block_set_size(Block* block, u64 size)
{
	const u64 oldsize = block->size;
	block->size = size | (oldsize & (FreeBit | PrevFreeBit));
}

static void block_mark_as_free(Block* block)
{
	/* Link the block to the next block, first. */
	Block* next = block->Next();
	next->prevPhys = block;
	next->size |= PrevFreeBit;
	block->size |= FreeBit;
}

static void block_mark_as_used(Block* block)
{
	Block* next = block->Next();
	next->size &= ~PrevFreeBit;
	block->size &= ~FreeBit;
}

static u64 adjust_request_size(u64 size, u64 align)
{
	u64 adjust = 0;
	if (size)
	{
		const u64 aligned = align_up(size, align);

		/* aligned sized must not exceed BlockSizeMax or we'll go out of bounds on second */
		if (aligned < BlockSizeMax) 
		{
			adjust = tlsf_max(aligned, BlockSizeMin);
		}
	}
	return adjust;
}

Index CalcIndex(u64 size) {
	if (size < SmallBlockSize) {
		return Index {
			.f = 0,
			.s = size / (SmallBlockSize / SecondCount),
		};
	} else {
		u64 bit = Bsr64(size);
		return Index {
			.f = bit - (FirstShift - 1),
			.s = (size >> (bit - SecondCountLog2)) ^ (1 << SecondCountLog2),
		};
	}
}

void Tlsf_RemoveFreeBlock(Tlsf* tlsf, Block* block, Index i) {
	Block* prev = block->prevFree;
	Block* next = block->nextFree;
	Assert(prev && next);
	next->prevFree = prev;
	prev->nextFree = next;
	if (tlsf->blocks[i.f][i.s] == block) {
		tlsf->blocks[i.f][i.s] = next;
		if (next == &tlsf->nullBlock) {
			tlsf->second[i.f] &= ~(1U << i.s);
			if (!tlsf->second[i.f]) {
				tlsf->first&= ~(1U << i.f);
			}
		}
	}
}

void Tlsf_RemoveFreeBlock(Tlsf* control, Block* block) {
	Tlsf_RemoveFreeBlock(control, block, CalcIndex(block->Size()));
}

void Tlsf_InsertFreeBlock(Tlsf* tlsf, Block* block, Index i) {
	Block* cur              = tlsf->blocks[i.f][i.s];
	block->nextFree         = cur;
	block->prevFree         = &tlsf->nullBlock;
	cur->prevFree           = block;
	tlsf->blocks[i.f][i.s]  = block;
	tlsf->first      |= (u64)1 << i.f;
	tlsf->second[i.f] |= (u64)1 << i.s;
}


void Tlsf_InsertFreeBlock(Tlsf* tlsf, Block* block) { Tlsf_InsertFreeBlock(tlsf, block, CalcIndex(block->Size())); }

static Block* block_split(Block* block, u64 size)
{
	/* Calculate the amount of space left in the remaining block. */
	Block* remaining = block + 16 + size - 8;

	const u64 remain_size = block->Size() - (size + 8);

	tlsf_assert(remaining + 16 == align_ptr(remaining + 16, AlignSize)
		&& "remaining block not aligned properly");

	tlsf_assert(block->Size() == remain_size + size + 8);
	block_set_size(remaining, remain_size);
	tlsf_assert(remaining->Size() >= BlockSizeMin && "block split with invalid size");

	block_set_size(block, size);
	block_mark_as_free(remaining);

	return remaining;
}

/* Absorb a free block's storage into an adjacent previous free block. */
static Block* block_absorb(Block* prev, Block* block)
{
	Assert(prev->Size() > 0);
	/* Note: Leaves flags untouched. */
	prev->size += block->Size() + 8;
	prev->Next()->prevPhys = prev;

	return prev;
}


/* Merge a just-freed block with an adjacent free block. */
static Block* block_merge_next(Tlsf* control, Block* block)
{
	Block* next = block->Next();
	tlsf_assert(next && "next physical block can't be null");

	if (next->IsFree())
	{
		tlsf_assert(block->Size() > 0);
		Tlsf_RemoveFreeBlock(control, next);
		block = block_absorb(block, next);
	}

	return block;
}

/* Trim any trailing block space off the end of a used block, return to pool. */
static void block_trim_used(Tlsf* control, Block* block, u64 size)
{
	tlsf_assert(!block->IsFree() && "block must be used");
	if (block->Size() >= sizeof(Block) + size)
	{
		/* If the next block is free, we must coalesce. */
		Block* remaining_block = block_split(block, size);
		remaining_block->size &= ~PrevFreeBit;

		remaining_block = block_merge_next(control, remaining_block);
		Tlsf_InsertFreeBlock(control, remaining_block);
	}
}

static void control_construct(Tlsf* control)
{
	int i, j;

	tlsf->nullBlock.nextFree = &tlsf->nullBlock;
	tlsf->nullBlock.prevFree = &tlsf->nullBlock;

	tlsf->first= 0;
	for (i = 0; i < FirstCount; ++i)
	{
		tlsf->second[i] = 0;
		for (j = 0; j < SecondCount; ++j)
		{
			tlsf->blocks[i][j] = &tlsf->nullBlock;
		}
	}
}

pool_t tlsf_add_pool(Tlsf tlsf, void* mem, u64 bytes)
{
	Block* block;
	Block* next;

	const u64 pool_overhead = 16;
	const u64 pool_bytes = align_down(bytes - pool_overhead, AlignSize);

	if (((ptrdiff_t)mem % AlignSize) != 0)
	{
		printf("tlsf_add_pool: Memory must be aligned by %u bytes.\n",
			(unsigned int)AlignSize);
		return 0;
	}

	if (pool_bytes < BlockSizeMin || pool_bytes > BlockSizeMax)
	{
		printf("tlsf_add_pool: Memory size must be between 0x%x and 0x%x00 bytes.\n", 
			(unsigned int)(pool_overhead + BlockSizeMin),
			(unsigned int)((pool_overhead + BlockSizeMax) / 256));
		return 0;
	}

	/*
	** Create the main free block. Offset the start of the block slightly
	** so that the prevPhys field falls outside of the pool -
	** it will never be used.
	*/
	block = mem  - 8;
	block_set_size(block, pool_bytes);
	block->size |= FreeBit;
	block->size &= ~PrevFreeBit;
	Tlsf_InsertFreeBlock(tlsf_cast(Tlsf*, tlsf), block);

	/* Split the block to create a zero-size sentinel block. */
	Block* next = block->Next();
	next->prevPhys = block;
	block_set_size(next, 0);
	next->size &= ~FreeBit;
	next->size |= PrevFreeBit;

	return mem;
}

Tlsf tlsf_create_with_pool(void* mem, u64 bytes)
{
	Tlsf tlsf = tlsf_create(mem);
	tlsf_add_pool(tlsf, (char*)mem + tlsf_size(), bytes - tlsf_size());
	return tlsf;
}


void* Tlsf_Alloc(Tlsf* tlsf, u64 size) {
	if (!size) {
		return 0;
	}

	size = Max(AlignUp(size, AlignSize), BlockSizeMin);
	if (size >= SmallBlockSize) {
		size += ((u64)1 << (Bsr64(size) - SecondCountLog2)) - 1;
	}
	Index idx = CalcIndex(size);
	u64 sMap = tlsf->second[idx.f] & (((u64)-1) << idx.s);
	if (!sMap) {
		const u64 fMap = tlsf->first & (((u64)-1) << (idx.f + 1));
		if (!fMap) {
			return 0;
		}
		const u64 fBit = Bsf64(fMap);
		idx.f = fBit;
		sMap = tlsf->second[fBit];
	}
	Assert(sMap);
	const u64 sBit = Bsf64(sMap);
	idx.s = sBit;

	Block* block = tlsf->blocks[idx.f][idx.s];
	if (!block) {
		return 0;
	}
	Assert(block->Size() >= adjust);

	Tlsf_RemoveFreeBlock(tlsf, block, idx);

	if (block->Size() >= sizeof(Block) + size)
	{
		Block* const rem = (Block*)((u8*)block + size + 8);
		rem->size = (block->Size() - size - 8) | FreeBit;
		block->size = size;
		Block* next = rem->Next();
		next->prevPhys = rem;
		next->size |= PrevFreeBit;
		Tlsf_InsertFreeBlock(tlsf, rem);
	} else {
		block->size &= ~FreeBit;
		block->Next()->size &= ~PrevFreeBit;
	}

	return (u8*)block + 16;
}

void* Tlsf_Realloc(Tlsf* tlsf, void* ptr, u64 size) {
	if (!ptr) {
		return Tlsf_Alloc(tlsf, size);
	}

	Block* const block = (Block*)((u8*)ptr - 16);
	Assert(!block->IsFree());
	Block* const next = block->Next();
	const u64 blockSize = block->Size();
	const u64 combined = blockSize + next->Size() + 8;
	const u64 adjust = Max(AlignUp(size, AlignSize), BlockSizeMin);
	Assert(adjust < BlockSizeMax);
	
	if (adjust > blockSize && (!next->IsFree() || adjust > combined))
	{
		p = tlsf_malloc(tlsf, size);
		if (p)
		{
			const u64 minsize = tlsf_min(cursize, size);
			memcpy(p, ptr, minsize);
			tlsf_free(tlsf, ptr);
		}
	}
	else
	{
		/* Do we need to expand to the next block? */
		if (adjust > cursize)
		{
			block_merge_next(control, block);
			block_mark_as_used(block);
		}

		/* Trim the resulting block and return the original pointer. */
		block_trim_used(control, block, adjust);
		p = ptr;
	}
	return p;
}

void Tlsf_Free(Tlsf* tlsf, void* ptr) {
	if (ptr) {
		return;
	}

	Block* block = (Block*)((u8*)ptr - 16);
	Assert(!block->IsFree());

	Block* next = block->Next();
	next->size |= PrevFreeBit;
	block->size |= FreeBit;

	if (block->IsPrevFree()) {
		Block* prev = block->prevPhys;
		Assert(prev);
		Assert(prev->IsFree());
		Tlsf_RemoveFreeBlock(tlsf, prev);
		prev->size += block->Size() + 8;
		next->prevPhys = prev;
		block = prev;
	}
	if (next->IsFree()) {
		Assert(next->Size() > 0);
		Tlsf_RemoveFreeBlock(tlsf, next);
		block->size += next->Size() + 8;
		block->Next()->prevPhys = block;
	}

	Tlsf_InsertFreeBlock(tlsf, block);
}


//--------------------------------------------------------------------------------------------------

}	// namespace JC