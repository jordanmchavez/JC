#pragma once

#include "JC/Common.h"

namespace JC {

//-------------------------------------------------------------------------------------------------

constexpr u64 AlignPow2(u64 u);

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
	u64     bucketsCap = 0;
	Elem*   elems      = 0;
	u64     elemsLen   = 0;
	u64     elemsCap   = 0;
	u64     mask       = 0;

	Map() = default;

	Map(Arena* arenaIn, SrcLoc sl = SrcLoc::Here()) {
		Init(arenaIn, 16, 0, sl);
	}

	Map(Arena* arenaIn, u64 bucketsCapIn, u64 elemsCapIn, SrcLoc sl = SrcLoc::Here()) {
		Init(arenaIn, bucketsCapIn, elemsCapIn, sl);
	}

	void Init(Arena* arenaIn, SrcLoc sl = SrcLoc::Here()) {
		Init(arenaIn, 16, 0, sl);
	}

	void Init(Arena* arenaIn, u64 bucketsCapIn, u64 elemsCapIn, SrcLoc sl = SrcLoc::Here()) {
		Assert(bucketsCapIn > 0);
		bucketsCapIn = AlignPow2(bucketsCapIn);

		arena      = arenaIn;
		buckets    = arena->AllocT<Bucket>(bucketsCapIn, sl);
		bucketsCap = bucketsCapIn;
		elems      = arena->AllocT<Elem>(elemsCapIn);
		elemsLen   = 0;
		elemsCap   = elemsCapIn;
		mask       = bucketsCapIn - 1;
		MemSet(buckets, 0, bucketsCapIn * sizeof(Bucket));
	}

	V* Find(K k) const {
		u64 h = Hash(k);
		u32 df = 0x100 | (h & 0xff);
		u64 i = h & mask;
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
					if (!arena->ExtendT(elems, elemsCap, newCap, sl)) {
						Elem* newElems = arena->AllocT<Elem>(newCap , sl);
						MemCpy(newElems, elems, elemsLen * sizeof(Elem));
						elems = newElems;
					}
					elemsCap = newCap;
				}
				elems[elemsLen++] = Elem { .key = k, .val = v };
				if (elemsLen > (7 * (bucketsCap >> 3))) {	// max load factor = 7/8 = 87.5%
					u64 newBucketsCap = bucketsCap << 1;
					if (!arena->ExtendT<Bucket>(buckets, bucketsCap, newBucketsCap, sl)) {
						buckets = arena->AllocT<Bucket>(newBucketsCap, sl);
					}
					bucketsCap = newBucketsCap;
					mask = newBucketsCap - 1;
					for (u64 j = 0; j < elemsLen; ++j) {
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
		u64 h = Hash(k);
		u32 df = 0x100 | (h & 0xff);
		u64 i = h & mask;
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

		u64 ei = buckets[i].idx;
		u64 next = (i + 1 == bucketsCap) ? 0 : i + 1;
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
};

//-------------------------------------------------------------------------------------------------

}	// namesapce JC