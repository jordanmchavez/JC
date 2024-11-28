#include "JC/Tlsf.h"

#include "JC/Bit.h"
#include "JC/Mem.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u64 FreeBit     = 1 << 0;
static constexpr u64 PrevFreeBit = 1 << 1;

struct Block {
	Block* prev     = 0;
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
static constexpr u32 SecondCountLog2  = 5;
static constexpr u32 SecondCount      = 1 << SecondCountLog2;    // 32
static constexpr u32 FirstShift       = SecondCountLog2 + AlignSizeLog2; // 8
static constexpr u32 FirstMax         = 40;
static constexpr u32 FirstCount       = FirstMax - FirstShift + 1;  // 40-8+1=33
static constexpr u32 SmallBlockSize   = 1 << FirstShift; // 256
static constexpr u64 BlockSizeMin     = 24;	// nextFree + prevFree + next->prev
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
		const u32 bit = Bsr64(size);
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
	Assert(size >= sizeof(Ctx));
	opaque = (u64)ptr;
	Ctx* ctx = (Ctx*)ptr;

	ctx->nullBlock.prev     = 0;
	ctx->nullBlock.size     = 0;
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
	void* const chunk       = AlignPtrUp((u8*)ptr + sizeof(Ctx), AlignSize);
	const u64   chunkOffset = (u8*)chunk - (u8*)ptr;
	Assert(size > chunkOffset);
	AddChunk(chunk, size - chunkOffset);
}

//--------------------------------------------------------------------------------------------------

void Tlsf::AddChunk(void* ptr, u64 size){
	Ctx* const ctx = (Ctx*)opaque;

	Assert(ptr);
	Assert((u64)ptr % AlignSize == 0);
	Assert(size % AlignSize == 0);
	Assert(size >= sizeof(Chunk) + 24);

	Chunk* chunk = (Chunk*)ptr;
	chunk->next = ctx->chunks;
	chunk->size = size;
	ctx->chunks = chunk;


	Block* const block  = (Block*)(chunk + 1);
	block->prev         = 0;
	block->size         = (size - sizeof(Chunk) - 24) | FreeBit;	// 16 for the first block, 8 for the dummy last block
	Block* const last   = block->Next();
	last->prev          = block;
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
	u64 calcIndexSize = size;
	if (size >= SmallBlockSize) {
		// Round up to next block size
		const u64 round = ((u64)1 << (Bsr64(size) - SecondCountLog2)) - 1;
		calcIndexSize += round;
	}
	Index idx = CalcIndex(calcIndexSize);
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
		Assert(rem->Next() == next);
		block->size = size;	// !free, !prevFree 
		next->prev = rem;

		InsertFreeBlock(ctx, rem);

	} else {
		block->size &= ~FreeBit;
		next->size  &= ~PrevFreeBit;
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

struct ExpectedBlock {
	u64  size;
	bool free;
};


void CheckTlsf(Tlsf tlsf, Span<Span<ExpectedBlock>> expectedChunks) {
	Ctx* const ctx = (Ctx*)tlsf.opaque;

	u32 freeBlockCount[FirstCount][SecondCount] = {};

	for (u32 f = 0; f < FirstCount; f++) {
		const u64 fBit = ctx->first & ((u64)1 << f);
		CheckTrue(
			( fBit &&  ctx->second[f]) ||
			(!fBit && !ctx->second[f])
		);
		for (u32 s = 0; s < SecondCount; s++) {
			const u64 sBit = ctx->second[f] & ((u64)1 << s);
			CheckTrue(
				( sBit && ctx->blocks[f][s] != &ctx->nullBlock) ||
				(!sBit && ctx->blocks[f][s] == &ctx->nullBlock)
			);

			for (Block* block = ctx->blocks[f][s]; block != &ctx->nullBlock; block = block->nextFree) {
				CheckTrue(block->IsFree());
				CheckTrue(!block->IsPrevFree());
				CheckTrue(!block->Next()->IsFree());
				CheckTrue(block->Next()->IsPrevFree());
				CheckTrue(block->Size() >= BlockSizeMin);
				const Index idx = CalcIndex(block->Size());
				CheckEq(idx.f, f);
				CheckEq(idx.s, s);
				freeBlockCount[f][s]++;
			}
		}
	}

	const Span<ExpectedBlock>* expectedChunk    = expectedChunks.data;
	const Span<ExpectedBlock>* expectedChunksEnd = expectedChunks.End();

	for (Chunk* chunk = ctx->chunks; chunk; chunk = chunk->next) {
		CheckTrue(expectedChunk < expectedChunksEnd);

		const ExpectedBlock* expectedBlock     = expectedChunk->data;
		const ExpectedBlock* expectedBlocksEnd = expectedChunk->End();

		Block* prev     = 0;
		bool   prevFree = false;
		Block* block    = (Block*)(chunk + 1);
		u32 blockNum = 0;blockNum;
		while (block->Size() > 0) {
			blockNum++;
			CheckTrue(expectedBlock < expectedBlocksEnd);
			CheckEq(prev, block->prev);
			CheckEq(prevFree, block->IsPrevFree());
			CheckTrue(!prevFree || !block->IsFree());
			
			CheckEq(expectedBlock->size, block->Size());
			if (block->IsFree()) {
				CheckTrue(expectedBlock->free);
				const Index idx = CalcIndex(block->Size());
				CheckTrue(freeBlockCount[idx.f][idx.s] > 0);
				freeBlockCount[idx.f][idx.s]--;
			} else {
				CheckTrue(!expectedBlock->free);
			}

			expectedBlock++;
			prev     = block;
			prevFree = block->IsFree();
			block    = block->Next();
		}
		CheckEq(expectedBlock, expectedBlocksEnd);
		CheckEq(prev, block->prev);
		CheckEq(prevFree, block->IsPrevFree());
		CheckTrue(!block->IsFree());
		CheckEq((u8*)chunk + chunk->size, (u8*)block + 16);
		expectedChunk++;
	}
	CheckEq(expectedChunk, expectedChunksEnd);

	for (u32 f = 0; f < FirstCount; f++) {
		for (u32 s = 0; s < SecondCount; s++) {
			CheckTrue(!freeBlockCount[f][s]);
		}
	}
}

//--------------------------------------------------------------------------------------------------

struct TlsfTestPerm {
	static void* Mem() {
		static void* mem = 0;
		if (!mem) {
			mem = Sys::VirtualAlloc(Size());
		}
		return mem;
	}

	static constexpr u64 Size() {
		return 65536;
	}
};

UnitTest("Tlsf::CalcIndex") {
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
	CheckCalcIndex((u64)0x10000000000, 33, 0);
}

UnitTest("Tlsf") {
	Tlsf tlsf;
	void* perm = TlsfTestPerm::Mem();
	constexpr u64 permSize = TlsfTestPerm::Size();
	tlsf.Init(perm, permSize);
	constexpr u64 base = permSize - sizeof(Ctx) - sizeof(Chunk) - 24;
	const bool f = true;	// free
	const bool u = false;	// used
	Span<Span<ExpectedBlock>> expectedChunks = {
		{ { base, f } },
	};
	CheckTlsf(tlsf, expectedChunks);

	void* p[64] = {};
	u32 pn = 0;
	void* x[64] = {};
	u32 xn = 0;
	u64 used = 0;

	#define PAlloc(size) p[pn++] = tlsf.Alloc(size); used += size + 8
	#define XAlloc() x[xn++] = tlsf.Alloc(24); used += 24 + 8

	SubTest("Alloc") {
		XAlloc();
		PAlloc(984);
		XAlloc();
		PAlloc(1000);
		XAlloc();
		PAlloc(1016);
		XAlloc();

		SubTest("From larger first and second") {
			expectedChunks = {
				{
					{   24, u }, // x0
					{  984, u }, // p0
					{   24, u }, // x1
					{ 1000, u }, // p1
					{   24, u }, // x2
					{ 1016, u }, // p2
					{   24, u }, // x3
					{ base - used, f },
				},
			};
			CheckTlsf(tlsf, expectedChunks);
		}

		SubTest("From exact") {
			tlsf.Free(p[0]);
			tlsf.Free(p[1]);
			tlsf.Free(p[2]);
			tlsf.Alloc(984);
			expectedChunks = {
				{
					{   24, u }, // x0
					{  984, u }, // p0
					{   24, u }, // x1
					{ 1000, f }, // p1
					{   24, u }, // x2
					{ 1016, f }, // p2
					{   24, u }, // x3
					{ base - used, f },
				},
			};
			CheckTlsf(tlsf, expectedChunks);
		}
		SubTest("No split") {
			tlsf.Free(p[1]);
			tlsf.Alloc(1000 - 25);
			expectedChunks = {
				{
					{   24, u }, // x0
					{  984, u }, // p0
					{   24, u }, // x1
					{ 1000, u }, // p1
					{   24, u }, // x2
					{ 1016, u }, // p2
					{   24, u }, // x3
					{ base - used, f },
				},
			};
			CheckTlsf(tlsf, expectedChunks);
		}
		SubTest("Split to new block") {
			tlsf.Free(p[1]);
			tlsf.Alloc(1000 - 24);
			expectedChunks = {
				{
					{   24, u }, // x0
					{  984, u }, // p0
					{   24, u }, // x1
					{  976, u }, // p1
					{   24, f },
					{   24, u }, // x2
					{ 1016, u }, // p2
					{   24, u }, // x3
					{ base - used, f },
				},
			};
			CheckTlsf(tlsf, expectedChunks);
		}
	}
	// alloc
		// no split
		// split
		// split and merge with next free
	// extend
		// block big enough
		// next free but not big enough
		// next big enough none leftover / some leftover
	// free
		// merge none
		// merge prev
		// merge next
		// merge both
		// merge begin
		// merge end
	// alloc rounding
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC