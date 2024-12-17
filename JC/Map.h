#pragma once

#include "JC/Common.h"

namespace JC {

//-------------------------------------------------------------------------------------------------

template <class K, class V> struct Map {
	struct Bucket {
		u32 df  = 0;	// top 3 bytes are distance, bottom byte is fingerprint
		u64 idx = 0;
	};

	struct Elem {
		K key = {};
		V val = {};
	};

	Arena*  arena      = 0;
	Bucket* buckets    = 0;
	u64     bucketsLen = 0;
	Elem*   elems      = 0;
	u64     elemsLen   = 0;
	u64     elemsCap   = 0;
	u8      mask       = 0;

	Map() = default;

	Map(Arena* arena, SrcLoc sl = SrcLoc::Here()) {
		Init(arena, sl);
	}

	void Init(Arena* arenaIn, SrcLoc sl = SrcLoc::Here()) {
		arena        = arenaIn;
		buckets    = (Bucket*)arena->Alloc(16 * sizeof(Bucket), sl);
		bucketsLen = 16;
		elems      = 0;
		elemsLen   = 0;
		elemsCap   = 0;
		mask       = 0xf;
		MemSet(buckets, 0, 16 * sizeof(Bucket));
	}

	Opt<V> Find(K k) const {
		u64 h = Hash(k);
		u32 df = 0x100 | (h & 0xff);
		u64 i = h & mask;
		Bucket* bucket = &buckets[i];
		if (df == bucket->df && elems[bucket->idx].key == k) {
			return elems[bucket->idx].val;
		}

		df += 0x100;
		i = (i + 1 == bucketsLen) ? 0 : i + 1;
		bucket = &buckets[i];
		if (df == bucket->df && elems[bucket->idx].key == k) {
			return elems[bucket->idx].val;
		}

		df += 0x100;
		i = (i + 1 == bucketsLen) ? 0 : i + 1;
		bucket = &buckets[i];
		while (true) {
			if (df == bucket->df) {
				if (elems[bucket->idx].key == k) {
					return elems[bucket->idx].val;
				}
			} else if (df > bucket->df) {
				return nullOpt;
			}
			df += 0x100;
			i = (i + 1 == bucketsLen) ? 0 : i + 1;
			bucket = &buckets[i];
		}
	}

	V* Put(K k, V v, SrcLoc sl = SrcLoc::Here()) {
		u64 h = Hash(k);
		u32 df = 0x100 | (h & 0xff);
		u64 i = h & mask;
		while (true) {
			Bucket* bucket = &buckets[i];
			if (df == bucket->df) {
				if (k == elems[bucket->idx].key) {
					elems[bucket->idx].val = (V)v;
					return &elems[bucket->idx].val;
				}
			} else if (df > bucket->df) {
				if (elemsLen >= elemsCap) {
					u64 newCap = Max(16ull, elemsCap * 2u);
					if (!arena->Extend(elems, elemsCap * sizeof(Elem), newCap * sizeof(Elem), sl)) {
						Elem* newElems = (Elem*)arena->Alloc(newCap * sizeof(Elem), sl);
						MemCpy(newElems, elems, elemsLen);
						elems = newElems;
					}
					elemsCap = newCap;
				}
				elems[elemsLen++] = Elem { .key = k, .val = v };
				if (elemsLen > (7 * (bucketsLen >> 3))) {	// max load factor = 7/8 = 87.5%
					u64 newBucketsLen = bucketsLen << 1;
					if (!arena->Extend(buckets, bucketsLen * sizeof(Bucket), newBucketsLen * sizeof(Bucket), sl)) {
						buckets = (Bucket*)arena->Alloc(newBucketsLen * sizeof(Bucket), sl);
					}
					MemSet(buckets, 0, newBucketsLen * sizeof(Bucket));
					bucketsLen = newBucketsLen;
					mask = (1u << newBucketsLen) - 1u;
					for (u64 j = 0; j < elemsLen; ++j) {
						h = Hash(elems[j].key);
						df = 0x100 | (h & 0xff);
						i = h & mask;
						while (df < buckets[i].df) {
							df += 0x100;
							i = (i + 1 == bucketsLen) ? 0 : i + 1;
						}
						Bucket b = { .df = df, .idx = j };
						while (buckets[i].df != 0) {
							Bucket t = buckets[i];
							buckets[i] = b;
							b = t;
							b.df += 0x100;
							i = (i + 1 == bucketsLen) ? 0 : i + 1;
						};
						buckets[i] = b;
					}
				} else {
					Bucket b = { .df = df, .idx = elemsLen - 1 };
					while (buckets[i].df != 0) {
						Bucket t = buckets[i];
						buckets[i] = b;
						b = t;
						b.df += 0x100;
						i = (i + 1 == bucketsLen) ? 0 : i + 1;
					};
					buckets[i] = b;
				}
				return &elems[elemsLen - 1].val;
			}
			df += 0x100;
			i = (i + 1 == bucketsLen) ? 0 : i + 1;
		}
	}

	void Remove(K k) {
		u64 h = Hash(k);
		u32 df = 0x100 | (h & 0xff);
		u64 i = h & mask;
		while (df < buckets[i].df) {
			df += 0x100;
			i = (i + 1 == bucketsLen) ? 0 : i + 1;
		}
		while (df == buckets[i].df && k != elems[buckets[i].idx].key) {
			df += 0x100;
			i = (i + 1 == bucketsLen) ? 0 : i + 1;
		}
		if (df != buckets[i].df) {
			return;
		}

		u64 ei = buckets[i].idx;
		u64 next = (i + 1 == bucketsLen) ? 0 : i + 1;
		while (buckets[next].df >= 0x200) {
			buckets[i].df = buckets[next].df - 0x100;
			buckets[i].idx = buckets[next].idx;
			i = next;
			next = (next + 1 == bucketsLen) ? 0 : next + 1;
		}
		buckets[i] = {};

		if (ei != elemsLen - 1) {
			elems[ei] = elems[elemsLen - 1];
			i = Hash(elems[ei].key) & mask;
			while (buckets[i].idx != elemsLen - 1) {
				i = (i + 1 == bucketsLen) ? 0 : i + 1;
			}
			buckets[i].idx = ei;
		}
		--elemsLen;

		return;
	}

	void Clear() {
		if (buckets != nullptr) {
			MemSet(buckets, 0, bucketsLen * sizeof(Bucket));
		}
		elemsLen = 0;
	}
};

//-------------------------------------------------------------------------------------------------

}	// namesapce JC