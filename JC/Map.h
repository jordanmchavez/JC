#pragma once

#include "JC/Common.h"
#include "JC/Allocator.h"
#include "JC/Hash.h"

namespace JC {

//-------------------------------------------------------------------------------------------------

template <class K, class V>
struct Map {
	struct Bucket {
		u32 df;	// top 3 bytes are distance, bottom byte is fingerprint
		u64 idx;
	};

	struct Elem {
		K key;
		V val;
	};

	Allocator*  alloc      = nullptr;
	Bucket*     buckets    = nullptr;
	u64         bucketsLen = 0;
	Elem*       elems      = nullptr;
	u64         elemsLen   = 0;
	u64         elemsCap   = 0;
	u8          mask       = 0;

	void Init(Allocator* inAlloc, SrcLoc srcLoc = SrcLoc::Here()) {
		alloc      = inAlloc;
		buckets    = (Bucket*)alloc->Alloc(16u * sizeof(Bucket), srcLoc);
		bucketsLen = 16;
		elems      = nullptr;
		elemsLen   = 0;
		elemsCap   = 0;
		mask       = 0xf;
		memset(buckets, 0, 16u * sizeof(Bucket));
	}
	void Shutdown() {
		alloc->Free(buckets, bucketsLen * sizeof(Bucket));
		alloc->Free(elems, elemsCap * sizeof(Elem));
		buckets    = nullptr;
		bucketsLen = 0;
		elems      = nullptr;
		elemsLen   = 0;
		elemsCap   = 0;
		mask       = 0;
	}

	Opt<V*> Find(K k) {
		u64 h = Hash(k);
		u32 df = 0x100 | (h & 0xff);
		u64 i = h & mask;
		Bucket* bucket = &buckets[i];
		if (df == bucket->df && elems[bucket->idx].key == k) {
			return &elems[bucket->idx].val;
		}

		df += 0x100;
		i = (i + 1 == bucketsLen) ? 0 : i + 1;
		bucket = &buckets[i];
		if (df == bucket->df && elems[bucket->idx].key == k) {
			return &elems[bucket->idx].val;
		}

		df += 0x100;
		i = (i + 1 == bucketsLen) ? 0 : i + 1;
		bucket = &buckets[i];
		while (true) {
			if (df == bucket->df) {
				if (elems[bucket->idx].key == k) {
					return &elems[bucket->idx].val;
				}
			} else if (df > bucket->df) {
				return nullOpt;
			}
			df += 0x100;
			i = (i + 1 == bucketsLen) ? 0 : i + 1;
			bucket = &buckets[i];
		}
	}

	V* Put(K k, V v, SrcLoc srcLoc = SrcLoc::Here()) {
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
					elems = (Elem*)alloc->Realloc(elems, elemsCap * sizeof(Elem), newCap * sizeof(Elem), srcLoc);
					elemsCap = newCap;
				}
				elems[elemsLen++] = Elem { .key = k, .val = v };
				if (elemsLen > (7 * (bucketsLen >> 3))) {	// max load factor = 7/8 = 87.5%
					u64 newBucketsLen = bucketsLen << 1;
					buckets = (Bucket*)alloc->Realloc(buckets, bucketsLen * sizeof(Bucket), newBucketsLen * sizeof(Bucket), srcLoc);
					memset(buckets, 0, newBucketsLen * sizeof(Bucket));
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
			memset(buckets, 0, sizeof(Bucket * bucketsLen));
		}
		elemsLen = 0;
	}
};

//-------------------------------------------------------------------------------------------------

}	// namesapce JC