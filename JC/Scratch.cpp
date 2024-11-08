#include "JC/Common.h"

namespace JC {


Block* AllocBlock(u64 size, Allocator* backing) {
	const u32 i = Log2(size) - MinBlockLog2;
	BlockMeta* meta = *blockMetas[i];
	Block* block = meta->blocks;
	if (block) {
		meta->free--;
		meta->unused = Min(meta->unused, meta->free);
	} else {
		block = (Block*)vmem->Alloc(size);
	}
	*block = Block {
		.size = size,
		.last = sizeof(*block),
		.used = sizeof(*block),
	};
	return block;
}

void Freeblock(Block* block) {
	BlockMeta* meta = &blockMetas[Log2(size) - MinBlockLog]2;
	block->next = meta->blocks;
	meta->blocks = block;
	meta->free++;
}

void* FrameAlloc(u64 size) {
	size = Align(size, 8);
	return BlockAlloc(&frameBlock, nullptr, 0, size, frameAllocatorBacking);
}

static u32 blockFrameCheck = 0;

void Tick() {
	free each frame blocks;
	frameBlocks = &Empty;

	for (u32 i = 0; i < blockMetasLen; i++) {
		if (blockMetas[i]->unused > 0) {
			blockMetas[i]->framesWithUnused++;
		} else {
			blockMetas[i]->framesWithUnused = 0;
		}
		blockMetas[i]->unused = blockMetas[i]->free;
	}

	if (blockMetas[blockFrameCheck]->framesWithUnused > 30) {
		if (Block* block = blockMetas[blockFrameCheck]->blocks) {
			blockMetas[blockFrameCheck] = block->next;
			blockMetas[blockFrameCheck]->free--;
			blockMetas[blockFrameCheck]->unused = Min(blockMetas[blockFrameCheck]->unused, blockMetas[blockFrameCheck]->free);
			vmem->Unmap(block);
		}
	}
}

}