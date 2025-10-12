#pragma once

#include "JC/Common_Std.h"
#include "JC/Common_Mem.h"

namespace JC {

//-------------------------------------------------------------------------------------------------

template <class K, class V> struct Map {
	struct Bucket {
		U32 df  = 0;	// top 3 bytes are distance, bottom byte is fingerprint
		U64 idx = 0;
	};

	struct Elem {
		K key = {};
		V val = {};
	};

	Mem::Mem mem;
	Bucket   initBuckets[16] = {};
	Bucket*  buckets         = initBuckets;
	U64      bucketsCap      = 16;
	Elem*    elems           = 0;
	U64      elemsLen        = 0;
	U64      elemsCap        = 0;
	U64      mask            = 16 - 1;

	Map() = default;

	explicit Map(Mem::Mem mem_) {
		mem = mem_;
	}

	void Init(Mem::Mem mem_) {
		mem = mem_;
	}

	V* FindOrNull(K k) const {
		U64 h = Hash(k);
		U32 df = 0x100 | (h & 0xff);
		U64 i = h & mask;
		Bucket* bucket = &buckets[i];
		if (df == bucket->df && elems[bucket->idx].key == k) {
			return &elems[bucket->idx].val;
		}

		df += 0x100;
		i = (i + 1 == bucketsCap) ? 0 : i + 1;
		bucket = &buckets[i];
		if (df == bucket->df && elems[bucket->idx].key == k) {
			return &elems[bucket->idx].val;
		}

		df += 0x100;
		i = (i + 1 == bucketsCap) ? 0 : i + 1;
		bucket = &buckets[i];
		while (true) {
			if (df == bucket->df) {
				if (elems[bucket->idx].key == k) {
					return &elems[bucket->idx].val;
				}
			} else if (df > bucket->df) {
				return 0;
			}
			df += 0x100;
			i = (i + 1 == bucketsCap) ? 0 : i + 1;
			bucket = &buckets[i];
		}
	}

	V* FindChecked(K k) {
		V* v = FindOrNull(k);
		JC_ASSERT(v);
		return v;
	}

	V* Put(K k, V v, SrcLoc sl = SrcLoc::Here()) {
		U64 h = Hash(k);
		U32 df = 0x100 | (h & 0xff);
		U64 i = h & mask;
		while (true) {
			Bucket* bucket = &buckets[i];
			if (df == bucket->df) {
				if (k == elems[bucket->idx].key) {
					elems[bucket->idx].val = (V)v;
					return &elems[bucket->idx].val;
				}
			} else if (df > bucket->df) {
				if (elemsLen >= elemsCap) {
					U64 newCap = Max(16ull, elemsCap * 2u);
					if (!Mem::ExtendT<Elem>(mem, elems, newCap, sl)) {
						Elem* const newElems = Mem::AllocT<Elem>(mem, newCap);
						memcpy(newElems, elems, elemsLen * sizeof(elems[0]));
						elems = newElems
					}
					elemsCap = newCap;
				}
				elems[elemsLen++] = Elem { .key = k, .val = v };
				if (elemsLen > (7 * (bucketsCap >> 3))) {	// max load factor = 7/8 = 87.5%
					U64 newBucketsCap = bucketsCap << 1;
					allocator->Free(buckets, bucketsCap);
					buckets = allocator->AllocT<Bucket>(newBucketsCap);
					memset(buckets, 0, newBucketsCap * sizeof(Bucket));
					bucketsCap = newBucketsCap;
					mask = newBucketsCap - 1;

					for (U64 j = 0; j < elemsLen; ++j) {
						h = Hash(elems[j].key);
						df = 0x100 | (h & 0xff);
						i = h & mask;
						while (df < buckets[i].df) {
							df += 0x100;
							i = (i + 1 == bucketsCap) ? 0 : i + 1;
						}
						Bucket b = { .df = df, .idx = j };
						while (buckets[i].df != 0) {
							Bucket t = buckets[i];
							buckets[i] = b;
							b = t;
							b.df += 0x100;
							i = (i + 1 == bucketsCap) ? 0 : i + 1;
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
						i = (i + 1 == bucketsCap) ? 0 : i + 1;
					};
					buckets[i] = b;
				}
				return &elems[elemsLen - 1].val;
			}
			df += 0x100;
			i = (i + 1 == bucketsCap) ? 0 : i + 1;
		}
	}

	void Remove(K k) {
		U64 h = Hash(k);
		U32 df = 0x100 | (h & 0xff);
		U64 i = h & mask;
		while (df < buckets[i].df) {
			df += 0x100;
			i = (i + 1 == bucketsCap) ? 0 : i + 1;
		}
		while (df == buckets[i].df && k != elems[buckets[i].idx].key) {
			df += 0x100;
			i = (i + 1 == bucketsCap) ? 0 : i + 1;
		}
		if (df != buckets[i].df) {
			return;
		}

		U64 ei = buckets[i].idx;
		U64 next = (i + 1 == bucketsCap) ? 0 : i + 1;
		while (buckets[next].df >= 0x200) {
			buckets[i].df = buckets[next].df - 0x100;
			buckets[i].idx = buckets[next].idx;
			i = next;
			next = (next + 1 == bucketsCap) ? 0 : next + 1;
		}
		buckets[i] = {};

		if (ei != elemsLen - 1) {
			elems[ei] = elems[elemsLen - 1];
			i = Hash(elems[ei].key) & mask;
			while (buckets[i].idx != elemsLen - 1) {
				i = (i + 1 == bucketsCap) ? 0 : i + 1;
			}
			buckets[i].idx = ei;
		}
		--elemsLen;

		return;
	}

	void Clear() {
		if (buckets != nullptr) {
			MemSet(buckets, 0, bucketsCap * sizeof(Bucket));
		}
		elemsLen = 0;
	}

	void Free(SrcLoc* sl = SrcLoc::Here()) {
		if (buckets != initBuckets) {
			allocator->Alloc(buckets, bucketsCap * sizeof(Bucket), 0, sl);
		}
		memset(initBuckets, 0, sizeof(initBuckets));
		allocator->Free(elems, elemsCap * sizeof(Elem), sl);
		buckets    = initBuckets;
		bucketsCap = 16;
		elems      = 0;
		elemsLen   = 0;
		elemsCap   = 0;
		mask       = 16 - 1;
	}
};

//-------------------------------------------------------------------------------------------------

}	// namesapce JC