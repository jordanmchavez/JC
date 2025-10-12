#pragma once

#include "JC/Common_Assert.h"
#include "JC/Common_Mem.h"
#include "JC/Common_Std.h"

namespace JC {

//-------------------------------------------------------------------------------------------------

template <class K, class V> struct Map {
	struct Bucket {
		U32 df;	// top 3 bytes are distance, bottom byte is fingerprint
		U64 idx;
	};

	struct Elem {
		K key;
		V val;
	};

	Bucket*  buckets;
	Elem*    elems;
	U64      elemsLen;
	U64      cap;

	void Init(Mem::Mem mem, U64 cap_) {
		Assert((cap & (cap - 1)) == 0);
		buckets    = Mem::AllocT<Bucket>(mem, cap);
		elems      = Mem::AllocT<Elem>(mem, cap);
		elemsLen   = 0;
		cap        = cap_;
	}

	V* FindOrNull(K k) const {
		U64 h = Hash(k);
		U32 df = 0x100 | (h & 0xff);
		U64 i = h & (cap - 1);
		Bucket* bucket = &buckets[i];
		if (df == bucket->df && elems[bucket->idx].key == k) {
			return &elems[bucket->idx].val;
		}

		df += 0x100;
		i = (i + 1 == cap) ? 0 : i + 1;
		bucket = &buckets[i];
		if (df == bucket->df && elems[bucket->idx].key == k) {
			return &elems[bucket->idx].val;
		}

		df += 0x100;
		i = (i + 1 == cap) ? 0 : i + 1;
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
			i = (i + 1 == cap) ? 0 : i + 1;
			bucket = &buckets[i];
		}
	}

	V* FindChecked(K k) {
		V* v = FindOrNull(k);
		Assert(v);
		return v;
	}

	V* Put(K k, V v) {
		U64 h = Hash(k);
		U32 df = 0x100 | (h & 0xff);
		U64 i = h & (cap - 1);
		while (true) {
			Bucket* bucket = &buckets[i];
			if (df == bucket->df) {
				if (k == elems[bucket->idx].key) {
					elems[bucket->idx].val = (V)v;
					return &elems[bucket->idx].val;
				}
			} else if (df > bucket->df) {
				Assert(elemsLen < cap);
				elems[elemsLen++] = Elem { .key = k, .val = v };
				Bucket b = { .df = df, .idx = elemsLen - 1 };
				while (buckets[i].df != 0) {
					Bucket t = buckets[i];
					buckets[i] = b;
					b = t;
					b.df += 0x100;
					i = (i + 1 == cap) ? 0 : i + 1;
				};
				buckets[i] = b;
				return &elems[elemsLen - 1].val;
			}
			df += 0x100;
			i = (i + 1 == cap) ? 0 : i + 1;
		}
	}

	void Remove(K k) {
		U64 h = Hash(k);
		U32 df = 0x100 | (h & 0xff);
		U64 i = h & (cap - 1);
		while (df < buckets[i].df) {
			df += 0x100;
			i = (i + 1 == cap) ? 0 : i + 1;
		}
		while (df == buckets[i].df && k != elems[buckets[i].idx].key) {
			df += 0x100;
			i = (i + 1 == cap) ? 0 : i + 1;
		}
		if (df != buckets[i].df) {
			return;
		}

		U64 ei = buckets[i].idx;
		U64 next = (i + 1 == cap) ? 0 : i + 1;
		while (buckets[next].df >= 0x200) {
			buckets[i].df = buckets[next].df - 0x100;
			buckets[i].idx = buckets[next].idx;
			i = next;
			next = (next + 1 == cap) ? 0 : next + 1;
		}
		buckets[i] = {};

		if (ei != elemsLen - 1) {
			elems[ei] = elems[elemsLen - 1];
			i = Hash(elems[ei].key) & (cap - 1);
			while (buckets[i].idx != elemsLen - 1) {
				i = (i + 1 == cap) ? 0 : i + 1;
			}
			buckets[i].idx = ei;
		}
		--elemsLen;

		return;
	}
};

//-------------------------------------------------------------------------------------------------

}	// namesapce JC