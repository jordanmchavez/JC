#include "JC/Map.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Key {
	u64 key;
	u64 hash;
};
bool operator==(Key k1, Key k2) { return k1.key == k2.key; }
u64 Hash(Key k) { return k.hash; }

TEST("Map") {
	#define CHECK_MAP(i, expectDf, expectKey) \
		CHECK_EQ(map.buckets[i].df, (u32)expectDf); \
		CHECK_EQ(map.elems[map.buckets[i].idx].key.key, (u64)expectKey)

	Map<Key, u64> map;
	map.Init(UnitTest::GetTempAllocator());
	CHECK(!map.Find(Key {0, 0}));

	map.Put(Key {0, 0}, (u64)0);
	CHECK_MAP(0, 0x100 | 0, 0);

	map.Put(Key {1, 0}, (u64)1);
	map.Put(Key {2, 0}, (u64)2);
	map.Put(Key {3, 0}, (u64)3);
	if (CHECK_EQ(map.elemsLen, (u64)4)) {
		CHECK_EQ(map.elems[0].val, (u64)0);
		CHECK_EQ(map.elems[1].val, (u64)1);
		CHECK_EQ(map.elems[2].val, (u64)2);
		CHECK_EQ(map.elems[3].val, (u64)3);
		CHECK_MAP(0, 0x100 | 0, 0);
		CHECK_MAP(1, 0x200 | 0, 1);
		CHECK_MAP(2, 0x300 | 0, 2);
		CHECK_MAP(3, 0x400 | 0, 3);
	}

	map.Put(Key {4, 4}, (u64)4);
	map.Put(Key {5, 5}, (u64)5);
	map.Put(Key {6, 6}, (u64)6);
	map.Put(Key {7, 7}, (u64)7);
	if (CHECK_EQ(map.elemsLen, (u64)8)) {
		CHECK_EQ(map.elems[0].val, (u64)0);
		CHECK_EQ(map.elems[1].val, (u64)1);
		CHECK_EQ(map.elems[2].val, (u64)2);
		CHECK_EQ(map.elems[3].val, (u64)3);
		CHECK_EQ(map.elems[4].val, (u64)4);
		CHECK_EQ(map.elems[5].val, (u64)5);
		CHECK_EQ(map.elems[6].val, (u64)6);
		CHECK_EQ(map.elems[7].val, (u64)7);
		CHECK_MAP(0, 0x100 | 0, 0);
		CHECK_MAP(1, 0x200 | 0, 1);
		CHECK_MAP(2, 0x300 | 0, 2);
		CHECK_MAP(3, 0x400 | 0, 3);
		CHECK_MAP(4, 0x100 | 4, 4);
		CHECK_MAP(5, 0x100 | 5, 5);
		CHECK_MAP(6, 0x100 | 6, 6);
		CHECK_MAP(7, 0x100 | 7, 7);
	}

	map.Put(Key {8, 4}, (u64)8);
	if (CHECK_EQ(map.elemsLen, (u64)9)) {
		CHECK_MAP(0, 0x100 | 0, 0);
		CHECK_MAP(1, 0x200 | 0, 1);
		CHECK_MAP(2, 0x300 | 0, 2);
		CHECK_MAP(3, 0x400 | 0, 3);
		CHECK_MAP(4, 0x100 | 4, 4);
		CHECK_MAP(5, 0x200 | 4, 8);
		CHECK_MAP(6, 0x200 | 5, 5);
		CHECK_MAP(7, 0x200 | 6, 6);
		CHECK_MAP(8, 0x200 | 7, 7);
	}

	map.Put(Key { 9, 1}, (u64)9);
	map.Put(Key {10, 1}, (u64)10);
	map.Put(Key {11, 1}, (u64)11);
	if (CHECK_EQ(map.elemsLen, (u64)12)) {
		CHECK_MAP( 0, 0x100 | 0, 0);
		CHECK_MAP( 1, 0x200 | 0, 1);
		CHECK_MAP( 2, 0x300 | 0, 2);
		CHECK_MAP( 3, 0x400 | 0, 3);
		CHECK_MAP( 4, 0x400 | 1, 9);
		CHECK_MAP( 5, 0x500 | 1, 10);
		CHECK_MAP( 6, 0x600 | 1, 11);
		CHECK_MAP( 7, 0x400 | 4, 4);
		CHECK_MAP( 8, 0x500 | 4, 8);
		CHECK_MAP( 9, 0x500 | 5, 5);
		CHECK_MAP(10, 0x500 | 6, 6);
		CHECK_MAP(11, 0x500 | 7, 7);
	}

	map.Remove(Key {10, 1});
	if (CHECK_EQ(map.elemsLen, (u64)11)) {
		CHECK_EQ(map.elems[10].key.key, (u64)11);
		CHECK_MAP( 0, 0x100 | 0, 0);
		CHECK_MAP( 1, 0x200 | 0, 1);
		CHECK_MAP( 2, 0x300 | 0, 2);
		CHECK_MAP( 3, 0x400 | 0, 3);
		CHECK_MAP( 4, 0x400 | 1, 9);
		CHECK_MAP( 5, 0x500 | 1, 11);
		CHECK_MAP( 6, 0x300 | 4, 4);
		CHECK_MAP( 7, 0x400 | 4, 8);
		CHECK_MAP( 8, 0x400 | 5, 5);
		CHECK_MAP( 9, 0x400 | 6, 6);
		CHECK_MAP(10, 0x400 | 7, 7);
	}

	map.Remove(Key {0, 0});
	map.Remove(Key {1, 0});
	map.Remove(Key {2, 0});
	if (CHECK_EQ(map.elemsLen, (u64)8)) {
		CHECK_EQ(map.elems[0].key.key, (u64)11);
		CHECK_EQ(map.elems[1].key.key, (u64)9);
		CHECK_EQ(map.elems[2].key.key, (u64)8);
		CHECK_EQ(map.elems[3].key.key, (u64)3);
		CHECK_EQ(map.elems[4].key.key, (u64)4);
		CHECK_EQ(map.elems[5].key.key, (u64)5);
		CHECK_EQ(map.elems[6].key.key, (u64)6);
		CHECK_EQ(map.elems[7].key.key, (u64)7);
		CHECK_MAP(0, 0x100 | 0, 3);
		CHECK_MAP(1, 0x100 | 1, 9);
		CHECK_MAP(2, 0x200 | 1, 11);
		CHECK_EQ(map.buckets[3].df, (u32)0);
		CHECK_MAP(4, 0x100 | 4, 4);
		CHECK_MAP(5, 0x200 | 4, 8);
		CHECK_MAP(6, 0x200 | 5, 5);
		CHECK_MAP(7, 0x200 | 6, 6);
		CHECK_MAP(8, 0x200 | 7, 7);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC