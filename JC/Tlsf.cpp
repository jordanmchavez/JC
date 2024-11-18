#include "JC/Tlsf.h"
#include "JC/Bit.h"

namespace JC {

//--------------------------------------------------------------------------------------------------



void Tlsf_RemoveFreeBlock(Control* c, Block* block, u32 fl, u32 sl) {
	Block* const n = block->nextFree;
	Block* const p = block->prevFree;
	n->prevFree = p;
	p->nextFree = n;
	if (blocks[fl][sl] == block) {
		blocks[fl][sl] = n;
		if (n == &nullBlock) {
			sl[fl] &= ~(1 << sl);
			if (!sl[fl]) {
				fl &= (1 << fl);
			}
		}
	}
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
	u32 slMap = sl[fl] & (0xffffffff << sl);
	if (!slMap) {
		const u32 flMap = fl & (0xffffffff << (fl + 1));
		if (!flMap) {
			return nullptr;
		}
		fl = Tlsf_BitScanForward32(flMap);
		slMap = sl[fl];
	}
	JC_ASSERT(slMap);
	sl = Tlsf_BitScanForward32(slMap);
	block = blocks[fl][sl];
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