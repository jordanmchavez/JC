typedef void* tlsf_t;
typedef void* pool_t;
typedef void (*tlsf_walker)(void* ptr, u64 size, int used, void* user);

/*
** Architecture-specific bit manipulation routines.
**
** TLSF achieves O(1) cost for malloc and free operations by limiting
** the search for a free block to a free list of guaranteed size
** adequate to fulfill the request, combined with efficient free list
** queries using bitmasks and architecture-specific bit-manipulation
** routines.
**
** Most modern processors provide instructions to count leading zeroes
** in a word, find the lowest and highest set bit, etc. These
** specific implementations will be used when available, falling back
** to a reasonably efficient generic implementation.
**
** NOTE: TLSF spec relies on ffs/fls returning value 0..31.
** ffs/fls return 1-32 by default, returning 0 for error.
*/

#if defined (_MSC_VER) && (_MSC_VER >= 1400) && (defined (_M_IX86) || defined (_M_X64))
	#include <intrin.h>

	#pragma intrinsic(_BitScanReverse)
	#pragma intrinsic(_BitScanForward)

	int TLsf_BitScanReverse32(unsigned int word)
	{
		unsigned long index;
		return _BitScanReverse(&index, word) ? index : -1;
	}

	int Tlsf_BitScanForward(unsigned int word)
	{
		unsigned long index;
		return _BitScanForward(&index, word) ? index : -1;
	}

#else
	#error("Unsupported compiler")
#endif

int Tlsf_BitScanReverse64(u64 size)
{
	int high = (int)(size >> 32);
	int bits = 0;
	if (high)
	{
		bits = 32 + TLsf_BitScanReverse32(high);
	}
	else
	{
		bits = TLsf_BitScanReverse32((int)size & 0xffffffff);

	}
	return bits;
}

static const u64 BlockOverhead = 8;

/* User data starts directly after the size field in a used block. */
static const u64 block_start_offset =
	offsetof(Block, size) + sizeof(u64);

/*
** A free block must be large enough to store its header minus the size of
** the prev_phys_block field, and no larger than the number of addressable
** bits for FL_INDEX.
*/
static u64 block_size(const Block* block)
{
	return block->size & ~(BlockFreeBit | BlockPrevFreeBit);
}

static void block_set_size(Block* block, u64 size)
{
	const u64 oldsize = block->size;
	block->size = size | (oldsize & (BlockFreeBit | BlockPrevFreeBit));
}

static int block_is_last(const Block* block)
{
	return block_size(block) == 0;
}

static int block_is_free(const Block* block)
{
	return tlsf_cast(int, block->size & BlockFreeBit);
}

static void block_set_free(Block* block)
{
	block->size |= BlockFreeBit;
}

static void block_set_used(Block* block)
{
	block->size &= ~BlockFreeBit;
}

static int block_is_prev_free(const Block* block)
{
	return tlsf_cast(int, block->size & BlockPrevFreeBit);
}

static void block_set_prev_free(Block* block)
{
	block->size |= BlockPrevFreeBit;
}

static void block_set_prev_used(Block* block)
{
	block->size &= ~BlockPrevFreeBit;
}

static Block* block_from_ptr(const void* ptr)
{
	return tlsf_cast(Block*,
		tlsf_cast(unsigned char*, ptr) - block_start_offset);
}

static void* block_to_ptr(const Block* block)
{
	return tlsf_cast(void*,
		tlsf_cast(unsigned char*, block) + block_start_offset);
}

/* Return location of next block after block of given size. */
static Block* offset_to_block(const void* ptr, u64 size)
{
	return (Block*)((u64)ptr + size);
}

/* Return location of previous block. */
static Block* block_prev(const Block* block)
{
	tlsf_assert(block_is_prev_free(block) && "previous block must be free");
	return block->prev_phys_block;
}

/* Return location of next existing block. */
static Block* block_next(const Block* block)
{
	Block* next = offset_to_block(block_to_ptr(block),
		block_size(block) - BlockOverhead);
	tlsf_assert(!block_is_last(block));
	return next;
}

/* Link a new block with its physical neighbor, return the neighbor. */
static Block* block_link_next(Block* block)
{
	Block* next = block_next(block);
	next->prev_phys_block = block;
	return next;
}

static void block_mark_as_free(Block* block)
{
	/* Link the block to the next block, first. */
	Block* next = block_link_next(block);
	block_set_prev_free(next);
	block_set_free(block);
}

static void block_mark_as_used(Block* block)
{
	Block* next = block_next(block);
	block_set_prev_used(next);
	block_set_used(block);
}

static u64 align_up(u64 x, u64 align)
{
	return (x + (align - 1)) & ~(align - 1);
}

static u64 align_down(u64 x, u64 align)
{
	return x - (x & (align - 1));
}

static void* align_ptr(const void* ptr, u64 align)
{
	const ptrdiff_t aligned =
		(tlsf_cast(ptrdiff_t, ptr) + (align - 1)) & ~(align - 1);
	tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
	return tlsf_cast(void*, aligned);
}

/*
** Adjust an allocation size to be aligned to word size, and no smaller
** than internal minimum.
*/
static u64 adjust_request_size(u64 size, u64 align)
{
	u64 adjust = 0;
	if (size)
	{
		const u64 aligned = align_up(size, align);

		/* aligned sized must not exceed BlockSizeMax or we'll go out of bounds on sl */
		if (aligned < BlockSizeMax) 
		{
			adjust = tlsf_max(aligned, BlockSizeMin);
		}
	}
	return adjust;
}

/*
** TLSF utility functions. In most cases, these are direct translations of
** the documentation found in the white paper.
*/

static void mapping_insert(u64 size, int* fli, int* sli)
{
	int fl, sl;
	if (size < SmallBlockSize)
	{
		/* Store small blocks in first list. */
		fl = 0;
		sl = tlsf_cast(int, size) / (SmallBlockSize / SlCount);
	}
	else
	{
		fl = Tlsf_BitScanReverse64(size);
		sl = tlsf_cast(int, size >> (fl - SlCountLog2)) ^ (1 << SlCountLog2);
		fl -= (FlShift - 1);
	}
	*fli = fl;
	*sli = sl;
}

/* This version rounds up to the next block size (for allocations) */
static void mapping_search(u64 size, int* fli, int* sli)
{
	if (size >= SmallBlockSize)
	{
		const u64 round = (1 << (Tlsf_BitScanReverse64(size) - SlCountLog2)) - 1;
		size += round;
	}
	mapping_insert(size, fli, sli);
}

static Block* search_suitable_block(Control* control, int* fli, int* sli)
{
	int fl = *fli;
	int sl = *sli;

	/*
	** First, search for a block in the list associated with the given
	** fl/sl index.
	*/
	unsigned int sl_map = control->sl[fl] & (~0U << sl);
	if (!sl_map)
	{
		/* No block exists. Search in the next largest first-level list. */
		const unsigned int fl_map = control->fl & (~0U << (fl + 1));
		if (!fl_map)
		{
			/* No free blocks available, memory has been exhausted. */
			return 0;
		}

		fl = Tlsf_BitScanForward(fl_map);
		*fli = fl;
		sl_map = control->sl[fl];
	}
	tlsf_assert(sl_map && "internal error - second level bitmap is null");
	sl = Tlsf_BitScanForward(sl_map);
	*sli = sl;

	/* Return the first block in the free list. */
	return control->blocks[fl][sl];
}

/* Remove a free block from the free list.*/
static void remove_free_block(Control* control, Block* block, int fl, int sl)
{
	Block* prev = block->prev_free;
	Block* next = block->next_free;
	tlsf_assert(prev && "prev_free field can not be null");
	tlsf_assert(next && "next_free field can not be null");
	next->prev_free = prev;
	prev->next_free = next;

	/* If this block is the head of the free list, set new head. */
	if (control->blocks[fl][sl] == block)
	{
		control->blocks[fl][sl] = next;

		/* If the new head is null, clear the bitmap. */
		if (next == &control->nullBlock)
		{
			control->sl[fl] &= ~(1U << sl);

			/* If the second bitmap is now empty, clear the fl bitmap. */
			if (!control->sl[fl])
			{
				control->fl &= ~(1U << fl);
			}
		}
	}
}

/* Insert a free block into the free block list. */
static void insert_free_block(Control* control, Block* block, int fl, int sl)
{
	Block* current = control->blocks[fl][sl];
	block->next_free = current;
	block->prev_free = &control->nullBlock;
	current->prev_free = block;

	tlsf_assert(block_to_ptr(block) == align_ptr(block_to_ptr(block), AlignSize)
		&& "block not aligned properly");
	/*
	** Insert the new block at the head of the list, and mark the first-
	** and second-level bitmaps appropriately.
	*/
	control->blocks[fl][sl] = block;
	control->fl |= (1U << fl);
	control->sl[fl] |= (1U << sl);
}

/* Remove a given block from the free list. */
static void block_remove(Control* control, Block* block)
{
	int fl, sl;
	mapping_insert(block_size(block), &fl, &sl);
	remove_free_block(control, block, fl, sl);
}

/* Insert a given block into the free list. */
static void block_insert(Control* control, Block* block)
{
	int fl, sl;
	mapping_insert(block_size(block), &fl, &sl);
	insert_free_block(control, block, fl, sl);
}

static int block_can_split(Block* block, u64 size)
{
	return block_size(block) >= sizeof(Block) + size;
}

/* Split a block into two, the second of which is free. */
static Block* block_split(Block* block, u64 size)
{
	/* Calculate the amount of space left in the remaining block. */
	Block* remaining =
		offset_to_block(block_to_ptr(block), size - BlockOverhead);

	const u64 remain_size = block_size(block) - (size + BlockOverhead);

	tlsf_assert(block_to_ptr(remaining) == align_ptr(block_to_ptr(remaining), AlignSize)
		&& "remaining block not aligned properly");

	tlsf_assert(block_size(block) == remain_size + size + BlockOverhead);
	block_set_size(remaining, remain_size);
	tlsf_assert(block_size(remaining) >= BlockSizeMin && "block split with invalid size");

	block_set_size(block, size);
	block_mark_as_free(remaining);

	return remaining;
}

/* Absorb a free block's storage into an adjacent previous free block. */
static Block* block_absorb(Block* prev, Block* block)
{
	tlsf_assert(!block_is_last(prev) && "previous block can't be last");
	/* Note: Leaves flags untouched. */
	prev->size += block_size(block) + BlockOverhead;
	block_link_next(prev);
	return prev;
}

/* Merge a just-freed block with an adjacent previous free block. */
static Block* block_merge_prev(Control* control, Block* block)
{
	if (block_is_prev_free(block))
	{
		Block* prev = block_prev(block);
		tlsf_assert(prev && "prev physical block can't be null");
		tlsf_assert(block_is_free(prev) && "prev block is not free though marked as such");
		block_remove(control, prev);
		block = block_absorb(prev, block);
	}

	return block;
}

/* Merge a just-freed block with an adjacent free block. */
static Block* block_merge_next(Control* control, Block* block)
{
	Block* next = block_next(block);
	tlsf_assert(next && "next physical block can't be null");

	if (block_is_free(next))
	{
		tlsf_assert(!block_is_last(block) && "previous block can't be last");
		block_remove(control, next);
		block = block_absorb(block, next);
	}

	return block;
}

/* Trim any trailing block space off the end of a block, return to pool. */
static void block_trim_free(Control* control, Block* block, u64 size)
{
	tlsf_assert(block_is_free(block) && "block must be free");
	if (block_can_split(block, size))
	{
		Block* remaining_block = block_split(block, size);
		block_link_next(block);
		block_set_prev_free(remaining_block);
		block_insert(control, remaining_block);
	}
}

/* Trim any trailing block space off the end of a used block, return to pool. */
static void block_trim_used(Control* control, Block* block, u64 size)
{
	tlsf_assert(!block_is_free(block) && "block must be used");
	if (block_can_split(block, size))
	{
		/* If the next block is free, we must coalesce. */
		Block* remaining_block = block_split(block, size);
		block_set_prev_used(remaining_block);

		remaining_block = block_merge_next(control, remaining_block);
		block_insert(control, remaining_block);
	}
}

static Block* block_trim_free_leading(Control* control, Block* block, u64 size)
{
	Block* remaining_block = block;
	if (block_can_split(block, size))
	{
		/* We want the 2nd block. */
		remaining_block = block_split(block, size - BlockOverhead);
		block_set_prev_free(remaining_block);

		block_link_next(block);
		block_insert(control, block);
	}

	return remaining_block;
}

static Block* block_locate_free(Control* control, u64 size)
{
	int fl = 0, sl = 0;
	Block* block = 0;

	if (size)
	{
		mapping_search(size, &fl, &sl);
		
		/*
		** mapping_search can futz with the size, so for excessively large sizes it can sometimes wind up 
		** with indices that are off the end of the block array.
		** So, we protect against that here, since this is the only callsite of mapping_search.
		** Note that we don't need to check sl, since it comes from a modulo operation that guarantees it's always in range.
		*/
		if (fl < FlCount)
		{
			block = search_suitable_block(control, &fl, &sl);
		}
	}

	if (block)
	{
		tlsf_assert(block_size(block) >= size);
		remove_free_block(control, block, fl, sl);
	}

	return block;
}

static void* block_prepare_used(Control* control, Block* block, u64 size)
{
	void* p = 0;
	if (block)
	{
		tlsf_assert(size && "size must be non-zero");
		block_trim_free(control, block, size);
		block_mark_as_used(block);
		p = block_to_ptr(block);
	}
	return p;
}

typedef struct integrity_t
{
	int prev_status;
	int status;
} integrity_t;

#define tlsf_insist(x) { tlsf_assert(x); if (!(x)) { status--; } }

static void integrity_walker(void* ptr, u64 size, int used, void* user)
{
	Block* block = block_from_ptr(ptr);
	integrity_t* integ = tlsf_cast(integrity_t*, user);
	const int this_prev_status = block_is_prev_free(block) ? 1 : 0;
	const int this_status = block_is_free(block) ? 1 : 0;
	const u64 this_block_size = block_size(block);

	int status = 0;
	(void)used;
	tlsf_insist(integ->prev_status == this_prev_status && "prev status incorrect");
	tlsf_insist(size == this_block_size && "block size incorrect");

	integ->prev_status = this_status;
	integ->status += status;
}

int tlsf_check(tlsf_t tlsf)
{
	int i, j;

	Control* control = tlsf_cast(Control*, tlsf);
	int status = 0;

	/* Check that the free lists and bitmaps are accurate. */
	for (i = 0; i < FlCount; ++i)
	{
		for (j = 0; j < SlCount; ++j)
		{
			const int fl_map = control->fl & (1U << i);
			const int sl_list = control->sl[i];
			const int sl_map = sl_list & (1U << j);
			const Block* block = control->blocks[i][j];

			/* Check that first- and second-level lists agree. */
			if (!fl_map)
			{
				tlsf_insist(!sl_map && "second-level map must be null");
			}

			if (!sl_map)
			{
				tlsf_insist(block == &control->nullBlock && "block list must be null");
				continue;
			}

			/* Check that there is at least one free block. */
			tlsf_insist(sl_list && "no free blocks in second-level map");
			tlsf_insist(block != &control->nullBlock && "block should not be null");

			while (block != &control->nullBlock)
			{
				int fli, sli;
				tlsf_insist(block_is_free(block) && "block should be free");
				tlsf_insist(!block_is_prev_free(block) && "blocks should have coalesced");
				tlsf_insist(!block_is_free(block_next(block)) && "blocks should have coalesced");
				tlsf_insist(block_is_prev_free(block_next(block)) && "block should be free");
				tlsf_insist(block_size(block) >= BlockSizeMin && "block not minimum size");

				mapping_insert(block_size(block), &fli, &sli);
				tlsf_insist(fli == i && sli == j && "block size indexed in wrong list");
				block = block->next_free;
			}
		}
	}

	return status;
}

#undef tlsf_insist

static void default_walker(void* ptr, u64 size, int used, void* user)
{
	(void)user;
	printf("\t%p %s size: %x (%p)\n", ptr, used ? "used" : "free", (unsigned int)size, block_from_ptr(ptr));
}

void tlsf_walk_pool(pool_t pool, tlsf_walker walker, void* user)
{
	tlsf_walker pool_walker = walker ? walker : default_walker;
	Block* block =
		offset_to_block(pool, -(int)BlockOverhead);

	while (block && !block_is_last(block))
	{
		pool_walker(
			block_to_ptr(block),
			block_size(block),
			!block_is_free(block),
			user);
		block = block_next(block);
	}
}

u64 tlsf_block_size(void* ptr)
{
	u64 size = 0;
	if (ptr)
	{
		const Block* block = block_from_ptr(ptr);
		size = block_size(block);
	}
	return size;
}

int tlsf_check_pool(pool_t pool)
{
	/* Check that the blocks are physically correct. */
	integrity_t integ = { 0, 0 };
	tlsf_walk_pool(pool, integrity_walker, &integ);

	return integ.status;
}

int test_ffs_fls()
{
	/* Verify ffs/fls work properly. */
	int rv = 0;
	rv += (Tlsf_BitScanForward(0) == -1) ? 0 : 0x1;
	rv += (TLsf_BitScanReverse32(0) == -1) ? 0 : 0x2;
	rv += (Tlsf_BitScanForward(1) == 0) ? 0 : 0x4;
	rv += (TLsf_BitScanReverse32(1) == 0) ? 0 : 0x8;
	rv += (Tlsf_BitScanForward(0x80000000) == 31) ? 0 : 0x10;
	rv += (Tlsf_BitScanForward(0x80008000) == 15) ? 0 : 0x20;
	rv += (TLsf_BitScanReverse32(0x80000008) == 31) ? 0 : 0x40;
	rv += (TLsf_BitScanReverse32(0x7FFFFFFF) == 30) ? 0 : 0x80;
	rv += (Tlsf_BitScanReverse64(0x80000000) == 31) ? 0 : 0x100;
	rv += (Tlsf_BitScanReverse64(0x100000000) == 32) ? 0 : 0x200;
	rv += (Tlsf_BitScanReverse64(0xffffffffffffffff) == 63) ? 0 : 0x400;
	if (rv)
	{
		printf("test_ffs_fls: %x ffs/fls tests failed.\n", rv);
	}
	return rv;
}

void* tlsf_malloc(tlsf_t tlsf, u64 size)
{
	Control* control = tlsf_cast(Control*, tlsf);
	const u64 adjust = adjust_request_size(size, AlignSize);
	Block* block = block_locate_free(control, adjust);
	return block_prepare_used(control, block, adjust);
}

void tlsf_free(tlsf_t tlsf, void* ptr)
{
	/* Don't attempt to free a NULL pointer. */
	if (ptr)
	{
		Control* control = tlsf_cast(Control*, tlsf);
		Block* block = block_from_ptr(ptr);
		tlsf_assert(!block_is_free(block) && "block already marked as free");
		block_mark_as_free(block);
		block = block_merge_prev(control, block);
		block = block_merge_next(control, block);
		block_insert(control, block);
	}
}

/*
** The TLSF block information provides us with enough information to
** provide a reasonably intelligent implementation of realloc, growing or
** shrinking the currently allocated block as required.
**
** This routine handles the somewhat esoteric edge cases of realloc:
** - a non-zero size with a null pointer will behave like malloc
** - a zero size with a non-null pointer will behave like free
** - a request that cannot be satisfied will leave the original buffer
**   untouched
** - an extended buffer size will leave the newly-allocated area with
**   contents undefined
*/
void* tlsf_realloc(tlsf_t tlsf, void* ptr, u64 size)
{
	Control* control = tlsf_cast(Control*, tlsf);
	void* p = 0;

	/* Zero-size requests are treated as free. */
	if (ptr && size == 0)
	{
		tlsf_free(tlsf, ptr);
	}
	/* Requests with NULL pointers are treated as malloc. */
	else if (!ptr)
	{
		p = tlsf_malloc(tlsf, size);
	}
	else
	{
		Block* block = block_from_ptr(ptr);
		Block* next = block_next(block);

		const u64 cursize = block_size(block);
		const u64 combined = cursize + block_size(next) + BlockOverhead;
		const u64 adjust = adjust_request_size(size, AlignSize);

		tlsf_assert(!block_is_free(block) && "block already marked as free");

		/*
		** If the next block is used, or when combined with the current
		** block, does not offer enough space, we must reallocate and copy.
		*/
		if (adjust > cursize && (!block_is_free(next) || adjust > combined))
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
	}

	return p;
}
