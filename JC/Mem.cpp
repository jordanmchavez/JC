#include "JC/Mem.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Map.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Block {
    Block* prevPhys = 0;
    u64    size     = 0;
    Block* nextFree = 0;
    Block* prevFree = 0;

	u64    Size()   const { return size & BlockSizeMask; }
	bool   IsFree() const { return size & BlockFreeBit; }
	Block* Next()         { return (Block*)((u8*)this + (size & BlockSizeMask) + 8); }
};

struct Index {
	u32 f;
	u32 s;
};

static constexpr u32 AlignSizeLog2       = 3;
static constexpr u32 AlignSize           = 1 << AlignSizeLog2;
static constexpr u32 SecondCountLog2     = 4;
static constexpr u32 SecondCount         = 1 << SecondCountLog2;    // 16
static constexpr u32 FirstShift          = SecondCountLog2 + AlignSizeLog2; // 8
static constexpr u32 FirstMax            = 40;
static constexpr u32 FirstCount          = FirstMax - FirstShift + 1;  // 64 - 8 + 1 = 57
static constexpr u32 SmallBlockSize      = 1 << FirstShift; // 256
static constexpr u64 BlockSizeMin        = sizeof(Block) - 8;	// 24
static constexpr u64 BlockSizeMax        = ((u64)1 << FirstMax) - 8;
static constexpr u64 BlockFreeBit        = 1 << 0;
static constexpr u64 BlockPrevFreeBit    = 1 << 1;
static constexpr u64 BlockSizeMask       = ~(BlockFreeBit | BlockPrevFreeBit);
static constexpr u64 BlockStartOffset    = 16;	// right after Block.size

static_assert(AlignSize == SmallBlockSize / SecondCount);

struct Tlsf {
	u8*    begin                           = 0;
	u8*    end                             = 0;
    Block  nullBlock                       = {};
    u64    first                           = 0;
    u64    second[FirstCount]              = {};
    Block* blocks[FirstCount][SecondCount] = {};

	//----------------------------------------------------------------------------------------------

	void Init(void* beginIn, void* endIn) {
		Assert((u64)beginIn % AlignSize == 0);
		Assert((u64)endIn % AlignSize == 0);
		begin              = (u8*)beginIn;
		end                = (u8*)endIn;
		nullBlock.nextFree = &nullBlock;
		nullBlock.prevFree = &nullBlock;
		first              = 0;
		MemSet(second, 0, sizeof(second));
		for (u32 i = 0; i < FirstCount; i++) {
			for (u32 j = 0; j < SecondCount; j++) {
				blocks[i][j] = &nullBlock;
			}
		}
		Block* const block = (Block*)(begin - 8);
		block->size        = (u64)(end - begin) | BlockFreeBit;
		Block* const next  = block->Next();
		next->prevPhys     = block;
		next->size         = 0 | BlockPrevFreeBit;
		InsertFreeBlock(block);
	}

	//----------------------------------------------------------------------------------------------

	Index CalcIndex(u64 size) {
		if (size < SmallBlockSize) {
			return Index {
				.f = 0,
				.s = (u32)size / (SmallBlockSize / SecondCount),
			};
		} else {
			const u64 roundedSize = size + ((u64)1 << (Bsr64(size) - SecondCountLog2)) - 1;
			const u32 bit = Bsr64(roundedSize);
			return Index {
				.f = bit - FirstShift + 1,
				.s = (u32)(roundedSize >> (bit - SecondCountLog2)) & ~(SecondCount - 1),
			};
		}
	}

	//----------------------------------------------------------------------------------------------

	void InsertFreeBlock(Block* block) {
		const Index idx = CalcIndex(block->Size());
		Assert((u64)block % AlignSize  == 0);
		block->nextFree                  = blocks[idx.f][idx.s];
		block->prevFree                  = &nullBlock;
		blocks[idx.f][idx.s]->prevFree = block;
		blocks[idx.f][idx.s]           = block;
		first         |= (u64)1 << idx.f;
		second[idx.f] |= (u64)1 << idx.s;
	}

	//----------------------------------------------------------------------------------------------

	void RemoveFreeBlock(Block* block, Index idx) {
		Block* const n = block->nextFree;
		Block* const p = block->prevFree;
		n->prevFree = p;
		p->nextFree = n;
		if (blocks[idx.f][idx.s] == block) {
			blocks[idx.f][idx.s] = n;
			if (n == &nullBlock) {
				second[idx.f] &= ~((u64)1 << idx.s);
				if (!second[idx.f]) {
					first &= ((u64)1 << idx.f);
				}
			}
		}
	}

	void RemoveFreeBlock(Block* block) {
		RemoveFreeBlock(block, CalcIndex(block->Size()));
	}

	//----------------------------------------------------------------------------------------------

	Block* AllocFreeBlock(u64 size) {
		Index idx = CalcIndex(size);
		u64 secondMap = second[idx.f] & ((u64)(-1) << idx.s);
		if (!secondMap) {
			const u32 firstMap = first & ((u64)(-1) << (idx.f + 1));
			if (!firstMap) {
				return nullptr;
			}
			idx.f = Bsf64(firstMap);
			secondMap = second[idx.f];
			Assert(secondMap);
		}
		idx.s = Bsf64(secondMap);
		Block* block = blocks[idx.f][idx.s];
		Assert((block->size & BlockSizeMask) >= size);
		RemoveFreeBlock(block, idx);
		return block;
	}

	//----------------------------------------------------------------------------------------------

	void* Alloc(Block* block, u64 size) {
		Assert(size % AlignSize == 0);
		const u64 blockSize = block->Size();
		if (blockSize - size >= sizeof(Block)) {
			Block* const rem = (Block*)((u8*)block + size + 8);
			const u64 remSize = blockSize - size - 8;
			rem->size = remSize | BlockFreeBit;
			block->size = size;
			rem->prevPhys = block;
			InsertFreeBlock(rem);
		} else {
			block->size &= ~(BlockFreeBit | BlockPrevFreeBit);
			((Block*)((u8*)block + blockSize + 8))->size |= BlockPrevFreeBit;
		}
		return (u8*)block + 16;
	}

	//----------------------------------------------------------------------------------------------

	void* Alloc(u64 size) {
		Assert(size && size % AlignSize == 0);
		Assert(size <= BlockSizeMax);
		if (size < BlockSizeMin) { size = BlockSizeMin; }
		Block* block = AllocFreeBlock(size);
		if (!block) {
			return 0;

		}
		return Alloc(block, size);
	}

	//----------------------------------------------------------------------------------------------

	bool Extend(void* ptr, u64 size) {
		Assert(ptr);
		Assert(size && size % AlignSize == 0);
		Assert(size <= BlockSizeMax);
		if (size < BlockSizeMin) { size = BlockSizeMin; }
	}

	//----------------------------------------------------------------------------------------------

	void ExtendPool(u64 extend) {
		Assert(extend % AlignSize == 0);
		Block* last = (Block*)(end - 16);
		if (last->IsFree()) {
			RemoveFreeBlock(last);
			last->size += extend;
		} else {
			last = last->Next();
			last->size = (extend - 8) | BlockFreeBit;
		}
		Block* const next = last->Next();
		next->size = 0;
		next->prevPhys = last;
		InsertFreeBlock(last);
		end += extend;
	}

};

//--------------------------------------------------------------------------------------------------

struct MemObj : Mem {
	s8      name     = {};
	u64     bytes    = 0;
	u32     allocs   = 0;
	u32     children = 0;
	MemObj* parent   = 0;

	void* Alloc(u64 size, SrcLoc sl) override;
	bool  Extend(void* ptr, u64 size, SrcLoc sl) override;
	void  Free(void* ptr) override;
};

//--------------------------------------------------------------------------------------------------

struct TempMemObj : TempMem {
	void* Alloc(u64 size, SrcLoc sl) override;
	bool  Extend(void* ptr, u64 size, SrcLoc sl) override;
	u64   Mark() override;
	void  Reset(u64 mark) override;
};

//--------------------------------------------------------------------------------------------------

struct Trace {
	MemObj* mem    = 0;
	SrcLoc  sl     = {};
	u32     allocs = 0;
	u64     bytes  = 0;
};

//--------------------------------------------------------------------------------------------------

static constexpr u64   AlignUp   (u64   u, u64 align) { return (u + (align - 1)) & ~(align - 1); }
static constexpr u64   AlignDown (u64   u, u64 align) { return u & ~(align - 1); }
static constexpr void* AlignPtrUp(void* p, u64 align) {	return (void*)AlignUp((u64)p, align); }

//--------------------------------------------------------------------------------------------------

struct MemApiObj : MemApi {
	static constexpr u32 MaxMemObjs      = 1024;

	u8*              permBegin           = 0;
	u8*              permCommit          = 0;
	u8*              permEnd             = 0;
	Tlsf             tlsf                = {};
	MemObj           memObjs[MaxMemObjs] = {};
	MemObj*          freeMemObjs         = 0;	// parent is the "next" link field

	Array<Trace>     traces              = {};
	Map<u64, u64>    srcLocToTrace       = {};
	Map<void*, u64>  ptrToTrace          = {};
	MemLeakReporter* memLeakReporter     = 0;

	u8*              tempBegin           = 0;
	u8*              tempUsed            = 0;
	u8*              tempCommit          = 0;
	u8*              tempReserve         = 0;
	u8*              tempLastAlloc       = 0;
	TempMemObj       tempMemObj          = {};

	//----------------------------------------------------------------------------------------------

	void AddTrace(MemObj* mem, void* ptr, u64 size, SrcLoc sl) {
		u64 key = HashCombine(Hash(sl.file), &sl.line, sizeof(sl.line));
		if (Opt<u64> idx = srcLocToTrace.Find(key)) {
			Trace* trace = &traces[idx.val];
			trace->allocs++;
			trace->bytes += size;
			ptrToTrace.Put(ptr, idx.val);
		} else {
			traces.Add(Trace {
				.mem    = mem,
				.sl     = sl,
				.allocs = 1,
				.bytes  = size,
			});
			ptrToTrace.Put(ptr, traces.len - 1);
			srcLocToTrace.Put(key, traces.len - 1);
		}
	}

	//----------------------------------------------------------------------------------------------

	void RemoveTrace(void* ptr, u64 size) {
		if (Opt<u64> idx = ptrToTrace.Find(ptr); idx.hasVal) {
			Trace* trace = &traces[idx.val];
			Assert(trace->allocs > 0);
			trace->allocs--;
			trace->bytes -= size;
			ptrToTrace.Remove(ptr);
		}
	}

	//----------------------------------------------------------------------------------------------

	void Init(u64 permReserveSize, u64 tempReserveSize, MemLeakReporter* reporterIn) override {
		permBegin  = (u8*)Sys::VirtualReserve(permReserveSize);
		Sys::VirtualCommit(permBegin, 4096);
		permCommit = permBegin + 4096;
		permEnd    = permBegin + permReserveSize;

		tlsf.Init(permBegin, permCommit);

		freeMemObjs = &memObjs[1];	// memObjs[0] is reserved for internal allocations
		for (u32 i = 1; i < MaxMemObjs - 1; i++) {
			memObjs[i].parent = &memObjs[i + 1];
		}
		memObjs[MaxMemObjs - 1].parent = nullptr;

		traces.mem = &memObjs[0];
		srcLocToTrace.Init(&memObjs[0]);
		ptrToTrace.Init(&memObjs[0]);
		memLeakReporter = reporterIn;

		tempBegin     = (u8*)Sys::VirtualReserve(tempReserveSize);
		tempUsed      = tempBegin;
		tempCommit    = tempBegin;
		tempReserve   = tempBegin + tempReserveSize;
		tempLastAlloc = 0;
	}

	//----------------------------------------------------------------------------------------------

	Mem*     Perm() override { return &memObjs[0]; }
	TempMem* Temp() override { return &tempMemObj; }

	//----------------------------------------------------------------------------------------------

	void* Alloc(MemObj* mem, u64 size, SrcLoc sl) {
		if (size == 0) {
			return 0;
		}

		size = Max(AlignUp(size, AlignSize), BlockSizeMin);

		void* ptr = tlsf.Alloc(size);
		if (!ptr) {
			// For simplicity, we deliberately don't check if the last block is free and include its size in the calc
			const u64 commitSize = permCommit - permBegin;
			u64 extend = Max((u64)4096, commitSize * 2);
			while (extend - 8 < size) {
				extend *= 2;
				Assert(commitSize + extend <= BlockSizeMax);
			}
			Sys::VirtualCommit(permCommit, extend);
			permCommit += extend;

			tlsf.ExtendPool(extend);
			ptr = tlsf.Alloc(size);
			Assert(ptr);
		}

		AddTrace(mem, ptr, size, sl);

		return ptr;
	}

	//----------------------------------------------------------------------------------------------

	bool Extend(MemObj* mem, void* ptr, u64 size, SrcLoc sl) {
		if (!ptr) {
			false;
		}

		size = AlignUp(size, AlignSize);

		if (tlsf.Extend(ptr, size)) {
			RemoveTrace(mem, ptr);
			AddTrace(mem, ptr, size);
			return true;
		}
		Block* const block     = (Block*)((u8*)ptr - 16);
		const u64    blockSize = block->Size();

		Block* next = (Block*)((u8*)block + blockSize + 8);
		if (!(next->size & BlockFreeBit)) {
			return false;
		}

		const u64 nextSize     = next->size & BlockSizeMask;
		const u64 combinedSize = blockSize + nextSize + 8;
		if (combinedSize < size) {
			return false;
		}


		RemoveFreeBlock(next);
		block->size += nextSize + 8;	// doesn't affect flags
		next = (Block*)((u8*)block + (block->size & BlockSizeMask));
		Assert(!(next->size & BlockFreeBit));
		next->prevPhys = block;
		next->size &= ~(BlockFreeBit & BlockPrevFreeBit);

		if ((block->size & BlockSizeMask) - size >= sizeof(Block)) {
			Block* const rem = (Block*)((u8*)block + size + 8);
			const u64 remSize = (block->size & BlockSizeMask) - size - 8;
			rem->size = remSize | BlockFreeBit;
			block->size = size;
			rem->prevPhys = block;
			InsertFreeBlock(rem);
		}

		AddTrace(mem, ptr, size, sl);

		return true;
	}

	//----------------------------------------------------------------------------------------------

	void Free(void* ptr) {
		if (!ptr) {
			return;
		}

		Block* block = (Block*)((u8*)ptr + 8);
		Assert(!(block->size & BlockFreeBit));
		block->size |= BlockFreeBit;

		RemoveTrace(ptr, block->size & BlockSizeMask);

		Block* const next = (Block*)((u8*)block + (block->size & BlockSizeMask) + 8);
		next->size |= BlockPrevFreeBit;

		if (block->size & BlockPrevFreeBit) {
			Block* const prev = block->prevPhys;
			RemoveFreeBlock(prev);
			prev->size += (block->size & BlockSizeMask) + 8;
			block = prev;
		}
		next->prevPhys = block;

		if (next->size & BlockFreeBit) {
			RemoveFreeBlock(next);
			block->size += (next->size & BlockSizeMask) + 8;
			((Block*)((u8*)block + block->size + 8))->prevPhys = block;
		}

		InsertFreeBlock(block);
	}

	//----------------------------------------------------------------------------------------------

	Mem* CreateScope(s8 name, Mem* parent) override {
		Assert(!parent || ((MemObj*)parent >= &memObjs[1] && (MemObj*)parent <= &memObjs[MaxMemObjs]));
		Assert(freeMemObjs);
		MemObj* const m = freeMemObjs;
		freeMemObjs = freeMemObjs->parent;
		m->name     = name;
		m->bytes    = 0;
		m->allocs   = 0;
		m->children = 0;
		m->parent   = (MemObj*)parent;
		if (m->parent) {
			m->parent->children++;
		}
		return m;
	}

	//----------------------------------------------------------------------------------------------

	void DestroyScope(Mem* mem) override {
		MemObj* m = (MemObj*)mem;
		if (
			memLeakReporter && 
			(
				(m->bytes > 0 || m->allocs > 0) ||
				(m->children > 0)
			)
		) {
			memLeakReporter->Begin(m->name, m->bytes, m->allocs, m->children);
			for (u64 i = 0; i < traces.len; i++) {
				if (traces[i].mem == m) {
					memLeakReporter->Alloc(traces[i].sl, traces[i].bytes, traces[i].allocs);
				}
			}
			if (m->children > 0) {
				for (u32 i = 0; i < MaxMemObjs; i++) {
					if (memObjs[i].parent == m) {
						memLeakReporter->Child(memObjs[i].name, memObjs[i].bytes, memObjs[i].allocs);
					}
				}
			}
		}
		MemSet(m, 0, sizeof(MemObj));
		m->parent = freeMemObjs;
		freeMemObjs = m;
	}

	//----------------------------------------------------------------------------------------------

	void* TempAlloc(u64 size, SrcLoc) {
		size = AlignUp(size, AlignSize);
		Assert(tempCommit >= tempUsed);
		const u64 tempAvail = (u64)(tempCommit - tempUsed);
		if (tempAvail < size) {
			u64 extend = Max((u64)4096, (u64)(tempCommit - tempReserve) * 2);
			while (tempAvail + extend < size) {
				extend *= 2;
				Assert(tempCommit + extend < tempReserve);
			}
			Sys::VirtualCommit(tempCommit, extend);
			tempCommit += extend;
		}
		tempLastAlloc = tempUsed;
		void* p = tempUsed;
		tempUsed += size;
		return p;
	}

	//----------------------------------------------------------------------------------------------

	bool TempExtend(void* ptr, u64 size, SrcLoc) {
		if (ptr != tempLastAlloc) {
			return false;
		}

		size = AlignUp(size, AlignSize);
		tempUsed = tempLastAlloc;

		Assert(tempCommit >= tempUsed);
		const u64 tempAvail = (u64)(tempCommit - tempUsed);
		if (tempAvail < size) {
			u64 extend = Max((u64)4096, (u64)(tempCommit - tempReserve) * 2);
			while (tempAvail + extend < size) {
				extend *= 2;
				Assert(tempCommit + extend < tempReserve);
			}
			Sys::VirtualCommit(tempCommit, extend);
			tempCommit += extend;
		}
		void* p = tempUsed;
		tempUsed += size;
		return p;
	}

	//----------------------------------------------------------------------------------------------

	u64 TempMark() {
		Assert(tempUsed >= tempBegin);
		return tempUsed - tempBegin;
	}

	//----------------------------------------------------------------------------------------------

	void TempReset(u64 mark) {
		Assert(tempUsed >= tempBegin);
		Assert(mark <= (u64)(tempUsed - tempBegin));
		tempUsed = tempBegin + mark;
	}
};

static MemApiObj memApiObj;

MemApi* MemApi::Get() {
	return &memApiObj;
}

//--------------------------------------------------------------------------------------------------

void* MemObj::Alloc(u64 size, SrcLoc sl)                 { return memApiObj.Alloc(this, size, sl); }
bool  MemObj::Extend(void* ptr, u64 size, SrcLoc sl)     { return memApiObj.Extend(this, ptr, size, sl); }
void  MemObj::Free(void* ptr)                            { memApiObj.Free(ptr); }

void* TempMemObj::Alloc(u64 size, SrcLoc sl)             { return memApiObj.TempAlloc(size, sl); }
bool  TempMemObj::Extend(void* ptr, u64 size, SrcLoc sl) { return memApiObj.TempExtend(ptr, size, sl); }
u64   TempMemObj::Mark()                                 { return memApiObj.TempMark(); }
void  TempMemObj::Reset(u64 mark)                        { memApiObj.TempReset(mark); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC