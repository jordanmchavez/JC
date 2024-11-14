#include "JC/Tlsf.h"

#include "JC/Bit.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u32 AlignSizeLog2  = 3;
static constexpr u32 AlignSize      = 1 << AlignSizeLog2;
static constexpr u32 SlCountLog2    = 4;
static constexpr u32 SlCount        = 1 << SlCountLog2;    // 16
static constexpr u32 FlShift        = SlCountLog2 + AlignSizeLog2; // 8
static constexpr u32 FlMax          = 32;
static constexpr u32 FlCount        = FlMax - FlShift + 1;  // 32 - 8 + 1 = 25
static constexpr u32 SmallBlockSize = 1 << FlShift; // 256
static_assert(AlignSize == SmallBlockSize / SlCount);

// size 32 bytes
struct Block {
    Block* prevPhys;
    u64    size;
	//--- user data starts here for used blocks ---
    Block* nextFree;
    Block* prevFree;
};

static constexpr u32 BlockHeaderOverhead = sizeof(u64);	// 8
static constexpr u64 BlockSizeMin = sizeof(Block) - sizeof(Block*);	// 24
static constexpr u64 BlockSizeMax = (u64)1 << FlMax;	// 4 gb
static constexpr u64 BlockFreeBit = 1 << 0;
static constexpr u64 BlockPrevFreeBit = 1 << 1;
static constexpr u64 BlockSizeMask = ~(BlockFreeBit | BlockPrevFreeBit);
static constexpr u64 BlockOverhead = sizeof(Block*);
static constexpr u64 BlockStartOffset = 16;	// right after size

struct Control {
    Block  nullBlock;
    u32    fl;
    u32    sl[FlCount];
    Block* blocks[FlCount][SlCount];
};

static constexpr u64 AlignUp(u64 x, u64 align) {
	return (x + (align - 1)) & ~(align - 1);
}

static constexpr u64 AlignDown(u64 x, u64 align) {
	return x & ~(align - 1);
}

static constexpr void* AlignPtrUp(void* p, u64 align) {
	return (void*)AlignUp((u64)p, align);
}

void Tlsf_CalcIndexes(u64 size, u32* fl, u32* sl) {
	if (size < SmallBlockSize) {
		*fl = 0;
		*sl = (u32)size / (SmallBlockSize / SlCount);
	} else {
		u32 f = Tlsf_BitScanReverse64(size);
		*sl = (u32)(size >> (f - SlCountLog2)) & ~(SlCount - 1);
		*fl = f - FlShift + 1;
	}
}

void Tlsf_InsertFreeBlock(Control* c, Block* block, u32 fl, u32 sl) {
	JC_ASSERT((u64)block % AlignSize  == 0);
	block->nextFree = c->blocks[fl][sl];
	block->prevFree = &c->nullBlock;
	c->blocks[fl][sl]->prevFree = block;
	c->blocks[fl][sl] = block;
	c->fl |= 1 << fl;
	c->sl[fl] |= 1 << sl;
}

void Tlsf_RemoveFreeBlock(Control* c, Block* block, u32 fl, u32 sl) {
	Block* const n = block->nextFree;
	Block* const p = block->prevFree;
	n->prevFree = p;
	p->nextFree = n;
	if (c->blocks[fl][sl] == block) {
		c->blocks[fl][sl] = n;
		if (n == &c->nullBlock) {
			c->sl[fl] &= ~(1 << sl);
			if (!c->sl[fl]) {
				c->fl &= (1 << fl);
			}
		}
	}
}

void Tlsf_Create(void* mem, u64 size) {
    JC_ASSERT((u64)mem % AlignSize == 0);
    Control* c = (Control*)mem;
    c->nullBlock.nextFree = &c->nullBlock;
    c->nullBlock.prevFree = &c->nullBlock;
	c->fl = 0;
	for (u32 i = 0; i < FlCount; i++) {
		c->sl[i] = 0;
		for (u32 j = 0; j < SlCount; j++) {
			c->blocks[i][j] = &c->nullBlock;
		}
	}

	const u64 poolSize = AlignDown(size - sizeof(Control) - 2 * BlockOverhead, AlignSize);
	JC_ASSERT(poolSize >= BlockSizeMin && poolSize <= BlockSizeMax);

	Block* const block = (Block*)((u8*)mem + sizeof(Control) - sizeof(Block*));
	block->prevPhys = nullptr;
	block->size = poolSize | BlockFreeBit;

	u32 fl, sl;
	Tlsf_CalcIndexes(poolSize, &fl, &sl);
	Tlsf_InsertFreeBlock(c, block, fl, sl);

	Block* const next = (Block*)((u8*)block + poolSize - sizeof(Block*));
	next->prevPhys = block;
	next->size = 0 | BlockPrevFreeBit;
}

void* Tlsf_Malloc(Control* c, u64 size) {
	JC_ASSERT(size <= BlockSizeMax);
	if (!size) {
		return nullptr;
	}
	size = AlignUp(size, AlignSize);

	Block* block;
	u32 fl, sl;
	if (size < SmallBlockSize) {
		fl = 0;
		sl = (u32)size / (SmallBlockSize / SlCount);
	} else {
		const u64 roundedSize = size + (1 << (Tlsf_BitScanReverse64(size) - SlCountLog2)) - 1;
		u32 f = Tlsf_BitScanReverse64(roundedSize);
		sl = (u32)(roundedSize >> (f - SlCountLog2)) & ~(SlCount - 1);
		fl = f - FlShift + 1;
	}

	if (fl >= FlCount) {
		return nullptr;
	}
	u32 slMap = c->sl[fl] & (0xffffffff << sl);
	if (!slMap) {
		const u32 flMap = c->fl & (0xffffffff << (fl + 1));
		if (!flMap) {
			return nullptr;
		}
		fl = Tlsf_BitScanForward32(flMap);
		slMap = c->sl[fl];
	}
	JC_ASSERT(slMap);
	sl = Tlsf_BitScanForward32(slMap);
	block = c->blocks[fl][sl];
	JC_ASSERT((block->size & BlockSizeMask) >= size);
	Tlsf_RemoveFreeBlock(c, block, fl, sl);

	if ((block->size & BlockSizeMask) - size >= sizeof(Block))
	{
		Block* rem = (Block*)((u8*)block + size + 8);
		const u64 remSize = (block->size & BlockSizeMask) - size - 8;
		rem->size = remSize | BlockFreeBit;
		block->size = size;
		rem->prevPhys = block;
		Tlsf_CalcIndexes(remSize, &fl, &sl);
		Tlsf_InsertFreeBlock(c, rem, fl, sl);
	} else {
		block->size &= ~(BlockFreeBit | BlockPrevFreeBit);
		((Block*)((u8*)block + block->size + 8))->size |= BlockPrevFreeBit;
	}

	return (u8*)block + 16;
}

void Tlsf_Free(Control* c, void* p) {
	if (!p) {
		return;
	}
	Block* block = (Block*)((u8*)p + 8);
	JC_ASSERT(!(block->size & BlockFreeBit));
	block->size |= BlockFreeBit;

	Block* next = (Block*)((u8*)block + (block->size & BlockSizeMask) + 8);
	next->size |= BlockPrevFreeBit;

	if (block->size & BlockPrevFreeBit) {
		Block* prev = block->prevPhys;
		u32 fl, sl;
		Tlsf_CalcIndexes(prev->size & BlockSizeMask, &fl, &sl);
		Tlsf_RemoveFreeBlock(c, prev, fl, sl);
		prev->size += (block->size & BlockSizeMask) + 8;
		block = prev;
	}
	next->prevPhys = block;

	if (next->size & BlockFreeBit) {
		u32 fl, sl;
		Tlsf_CalcIndexes(next->size & BlockSizeMask, &fl, &sl);
		Tlsf_RemoveFreeBlock(c, next, fl, sl);
		block->size += (next->size & BlockSizeMask) + 8;
		((Block*)((u8*)block + block->size + 8))->prevPhys = block;
	}

	u32 fl, sl;
	Tlsf_CalcIndexes(block->size & BlockSizeMask, &fl, &sl);
	Tlsf_InsertFreeBlock(c, block, fl, sl);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC