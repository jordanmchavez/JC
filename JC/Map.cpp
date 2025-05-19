#include "JC/Map.h"

#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Key {
	U64 key;
	U64 hash;
};
Bool operator==(Key k1, Key k2) { return k1.key == k2.key; }
U64 Hash(Key k) { return k.hash; }

UnitTest("Map") {
	#define CheckMap(i, expectDf, expectKey) \
		CheckEq(map.buckets[i].df, (U32)expectDf); \
		CheckEq(map.elems[map.buckets[i].idx].key.key, (U64)expectKey)

	Map<Key, U64> map(testAllocator);
	CheckTrue(!map.FindOrNull(Key {0, 0}));

	map.Put(Key {0, 0}, (U64)0);
	CheckMap(0, 0x100 | 0, 0);

	map.Put(Key {1, 0}, (U64)1);
	map.Put(Key {2, 0}, (U64)2);
	map.Put(Key {3, 0}, (U64)3);
	if (CheckEq(map.elemsLen, (U64)4)) {
		CheckEq(map.elems[0].val, (U64)0);
		CheckEq(map.elems[1].val, (U64)1);
		CheckEq(map.elems[2].val, (U64)2);
		CheckEq(map.elems[3].val, (U64)3);
		CheckMap(0, 0x100 | 0, 0);
		CheckMap(1, 0x200 | 0, 1);
		CheckMap(2, 0x300 | 0, 2);
		CheckMap(3, 0x400 | 0, 3);
	}

	map.Put(Key {4, 4}, (U64)4);
	map.Put(Key {5, 5}, (U64)5);
	map.Put(Key {6, 6}, (U64)6);
	map.Put(Key {7, 7}, (U64)7);
	if (CheckEq(map.elemsLen, (U64)8)) {
		CheckEq(map.elems[0].val, (U64)0);
		CheckEq(map.elems[1].val, (U64)1);
		CheckEq(map.elems[2].val, (U64)2);
		CheckEq(map.elems[3].val, (U64)3);
		CheckEq(map.elems[4].val, (U64)4);
		CheckEq(map.elems[5].val, (U64)5);
		CheckEq(map.elems[6].val, (U64)6);
		CheckEq(map.elems[7].val, (U64)7);
		CheckMap(0, 0x100 | 0, 0);
		CheckMap(1, 0x200 | 0, 1);
		CheckMap(2, 0x300 | 0, 2);
		CheckMap(3, 0x400 | 0, 3);
		CheckMap(4, 0x100 | 4, 4);
		CheckMap(5, 0x100 | 5, 5);
		CheckMap(6, 0x100 | 6, 6);
		CheckMap(7, 0x100 | 7, 7);
	}

	map.Put(Key {8, 4}, (U64)8);
	if (CheckEq(map.elemsLen, (U64)9)) {
		CheckMap(0, 0x100 | 0, 0);
		CheckMap(1, 0x200 | 0, 1);
		CheckMap(2, 0x300 | 0, 2);
		CheckMap(3, 0x400 | 0, 3);
		CheckMap(4, 0x100 | 4, 4);
		CheckMap(5, 0x200 | 4, 8);
		CheckMap(6, 0x200 | 5, 5);
		CheckMap(7, 0x200 | 6, 6);
		CheckMap(8, 0x200 | 7, 7);
	}

	map.Put(Key { 9, 1}, (U64)9);
	map.Put(Key {10, 1}, (U64)10);
	map.Put(Key {11, 1}, (U64)11);
	if (CheckEq(map.elemsLen, (U64)12)) {
		CheckMap( 0, 0x100 | 0, 0);
		CheckMap( 1, 0x200 | 0, 1);
		CheckMap( 2, 0x300 | 0, 2);
		CheckMap( 3, 0x400 | 0, 3);
		CheckMap( 4, 0x400 | 1, 9);
		CheckMap( 5, 0x500 | 1, 10);
		CheckMap( 6, 0x600 | 1, 11);
		CheckMap( 7, 0x400 | 4, 4);
		CheckMap( 8, 0x500 | 4, 8);
		CheckMap( 9, 0x500 | 5, 5);
		CheckMap(10, 0x500 | 6, 6);
		CheckMap(11, 0x500 | 7, 7);
	}

	map.Remove(Key {10, 1});
	if (CheckEq(map.elemsLen, (U64)11)) {
		CheckEq(map.elems[10].key.key, (U64)11);
		CheckMap( 0, 0x100 | 0, 0);
		CheckMap( 1, 0x200 | 0, 1);
		CheckMap( 2, 0x300 | 0, 2);
		CheckMap( 3, 0x400 | 0, 3);
		CheckMap( 4, 0x400 | 1, 9);
		CheckMap( 5, 0x500 | 1, 11);
		CheckMap( 6, 0x300 | 4, 4);
		CheckMap( 7, 0x400 | 4, 8);
		CheckMap( 8, 0x400 | 5, 5);
		CheckMap( 9, 0x400 | 6, 6);
		CheckMap(10, 0x400 | 7, 7);
	}

	map.Remove(Key {0, 0});
	map.Remove(Key {1, 0});
	map.Remove(Key {2, 0});
	if (CheckEq(map.elemsLen, (U64)8)) {
		CheckEq(map.elems[0].key.key, (U64)11);
		CheckEq(map.elems[1].key.key, (U64)9);
		CheckEq(map.elems[2].key.key, (U64)8);
		CheckEq(map.elems[3].key.key, (U64)3);
		CheckEq(map.elems[4].key.key, (U64)4);
		CheckEq(map.elems[5].key.key, (U64)5);
		CheckEq(map.elems[6].key.key, (U64)6);
		CheckEq(map.elems[7].key.key, (U64)7);
		CheckMap(0, 0x100 | 0, 3);
		CheckMap(1, 0x100 | 1, 9);
		CheckMap(2, 0x200 | 1, 11);
		CheckEq(map.buckets[3].df, (U32)0);
		CheckMap(4, 0x100 | 4, 4);
		CheckMap(5, 0x200 | 4, 8);
		CheckMap(6, 0x200 | 5, 5);
		CheckMap(7, 0x200 | 6, 6);
		CheckMap(8, 0x200 | 7, 7);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC