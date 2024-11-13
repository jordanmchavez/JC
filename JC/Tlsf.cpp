#include "JC/Tlsf.h"

#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanReverse64)
#pragma intrinsic(_BitScanForward)

namespace JC {

//--------------------------------------------------------------------------------------------------


i32 TLsf_BitScanReverse32(u32 x)
{
	u32 u;
	return _BitScanReverse((unsigned long*)&u, x) ? u : -1;
}

i32 TLsf_BitScanReverse64(u32 x)
{
	u32 u;
	return _BitScanReverse64((unsigned long*)&u, x) ? u : -1;
}

i32 Tlsf_BitScanFoward32(u32 x)
{
	u32 u;
	return _BitScanForward((unsigned long*)&u, x) ? u : -1;
}


struct Tlsf { u64 Handle = 0; };

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
    Block* prev;
    u64    size;
    Block* nextFree;
    Block* prevFree;
};

static constexpr u32 BlockHeaderOverhead = sizeof(u64);	// 8
static constexpr u64 BlockSizeMin = sizeof(Block) - sizeof(Block*);	// 24
static constexpr u64 BlockSizeMax = (u64)1 << FlMax;	// 4 gb
static constexpr u64 BlockFreeBit = 1 << 0;
static constexpr u64 BlockPrevFreeBit = 1 << 1;
static constexpr u64 BlockOverhead = sizeof(Block*);

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

void Tlsf_MappingInsert(u64 size, u32* fl, u32* sl) {
	if (size < SmallBlockSize) {
		*fl = 0;
		*sl = (u32)size / (SmallBlockSize / SlCount);
	} else {
		u32 f = Tlsf_BitScanReverse64(size);
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
	block->size = poolSize | BlockFreeBit;

	u32 fl, sl;
	mapping_insert(poolSize, &fl, &sl);
	insert_free_block(c, block, fl, sl);


	Block* const next = (Block*)((u8*)block + poolSize - sizeof(Block*));
	block->prev = next;
	next->size = 0 | BlockPrevFreeBit;
}

void* Tlsf_Malloc(Control* c, u64 size) {
	JC_ASSERT(size <= BlockSizeMax);
	if (!size) {
		return nullptr;
	}
	if (size) {
		const u64 alignedSize = AlignUp(size, AlignSize);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC