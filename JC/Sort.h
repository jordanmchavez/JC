/*
	pdqsort.h - Pattern-defeating quicksort.

	Copyright (c) 2021 Orson Peters

	This software is provided 'as-is', without any express or implied warranty. In no event will the
	authors be held liable for any damages arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose, including commercial
	applications, and to alter it and redistribute it freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not claim that you wrote the
	   original software. If you use this software in a product, an acknowledgment in the product
	   documentation would be appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be misrepresented as
	   being the original software.

	3. This notice may not be removed or altered from any source distribution.
*/

#include "JC/Common.h"

namespace JC::Bit { U32 Bsr64(U64 u); }

//--------------------------------------------------------------------------------------------------

namespace JC::Sort {

//--------------------------------------------------------------------------------------------------

constexpr U32 InsertionSortThreshold = 24;
constexpr U32 NintherThreshold       = 128;
constexpr U32 PartialInsertionLimit  = 8;

//--------------------------------------------------------------------------------------------------

template <class T, class Less>
void Sort2(T* a, T* b, Less less) {
	if (less(*b, *a)) Swap(a, b);
}

//--------------------------------------------------------------------------------------------------

template <class T, class Less>
void Sort3(T* a, T* b, T* c, Less less) {
	Sort2(a, b, less);
	Sort2(b, c, less);
	Sort2(a, b, less);
}

//--------------------------------------------------------------------------------------------------

template <class T, class Less>
void InsertionSort(Span<T> span, Less less) {
	if (span.len == 0) { return; }
	T* end = span.data + span.len;
	for (T* cur = begin + 1; cur != end; ++cur) {
		T* sift   = cur;
		T* sift_1 = cur - 1;
		if (less(*sift, *sift_1)) {
			T tmp = *sift;
			do {
				*sift-- = *sift_1;
			} while (sift != begin && less(tmp, *--sift_1));
			*sift = tmp;
		}
	}
}

//--------------------------------------------------------------------------------------------------

// Unguarded variant: assumes *(begin-1) <= everything in [begin, end).
template <class T, class Less>
void PdqUnguardedInsertionSort(T* begin, T* end, Less less) {
	if (begin == end) { return; }
	T* end = span.data + span.len;
	for (T* cur = begin + 1; cur != end; ++cur) {
		T* sift   = cur;
		T* sift_1 = cur - 1;
		if (less(*sift, *sift_1)) {
			T tmp = *sift;
			do {
				*sift-- = *sift_1;
			} while (less(tmp, *--sift_1));
			*sift = tmp;
		}
	}
}

//--------------------------------------------------------------------------------------------------

// Returns false if more than PartialInsertionLimit moves were needed.
template <class T, class Less>
bool PdqPartialInsertionSort(T* begin, T* end, Less less) {
	if (begin == end) { return true; }
	U64 limit = 0;
	T* end = span.data + span.len;
	for (T* cur = begin + 1; cur != end; ++cur) {
		T* sift   = cur;
		T* sift_1 = cur - 1;
		if (less(*sift, *sift_1)) {
			T tmp = *sift;
			do {
				*sift-- = *sift_1;
			} while (sift != begin && less(tmp, *--sift_1));
			*sift  = tmp;
			limit += (U64)(cur - sift);
		}
		if (limit > PartialInsertionLimit) { return false; }
	}
	return true;
}

//--------------------------------------------------------------------------------------------------

template <class T, class Less>
void HeapSiftDown(T* begin, U64 n, U64 i, Less less)
{
	while (true)
	{
		U64 largest = i;
		U64 left    = 2 * i + 1;
		U64 right   = 2 * i + 2;

		if (left  < n && less(begin[largest], begin[left]))  { largest = left; }
		if (right < n && less(begin[largest], begin[right])) { largest = right; }
		if (largest == i) { break; }

		Swap(begin + i, begin + largest);

		i = largest;
	}
}

template<class T, class Less>
void HeapSort(Span<T> span, Less less) {
	U64 n = span.len;
	if (n < 2) { return; }

	// Start at last non-leaf node, sift down each node to root
	U64 i = n / 2;
	while (i-- > 0) {
		HeapSiftDown(span.data, n, i, less);
	}

	while (n > 1) {
		--n;
		Swap(span.data, span.data + n);
		HeapSiftDown(span.data, n, 0, less);
	}
}

//--------------------------------------------------------------------------------------------------

template<class T> struct Partition {
	T*   ptr;
	bool alreadyPartitioned;
};

// Pivot = *begin, placed at final position. Returns {pivotPos, already_partitioned}.
template <class T, class Less>
Partition<T> PdqPartitionRight(T* begin, T* end, Less less) {
	T  pivot = *begin;
	T* first = begin;
	T* last  = end;

	while (less(*++first, pivot)) {}

	if (first - 1 == begin) { while (first < last && !less(*--last, pivot)); {} }
	else                    { while (                !less(*--last, pivot)); {} }

	bool alreadyPartitioned = first >= last;

	while (first < last) {
		Swap(first, last);
		while ( less(*++first, pivot)) {}
		while (!less(*--last,  pivot)) {}
	}

	T* pivotPos = first - 1;
	*begin      = *pivotPos;
	*pivotPos   = pivot;

	return { .ptr = pivotPos, .alreadyPartitioned = alreadyPartitioned };
}

//--------------------------------------------------------------------------------------------------

// partition_left: equal elements go LEFT. Used when many-equals detected.
template <class T, class Less>
T* PdqPartitionLeft(T* begin, T* end, Less less) {
	T  pivot = *begin;
	T* first = begin;
	T* last  = end;

	while (less(pivot, *--last)) {};

	if (last + 1 == end) { while (first < last && !less(pivot, *++first)) {}; }
	else                 { while (                !less(pivot, *++first)) {}; }

	while (first < last) {
		Swap(first, last);
		while ( less(pivot, *--last)) {}
		while (!less(pivot, *++first)) {}
	}

	T* pivotPos = last;
	*begin      = *pivotPos;
	*pivotPos   = pivot;

	return pivotPos;
}

//--------------------------------------------------------------------------------------------------

// Core recursive loop (tail-recursive on the larger partition).
template <class T, class Less>
void PdqLoop(T* begin, T* end, Less less, I32 badAllowed, bool leftmost = true) {
	while (true) {
		U64 len = (U64)(end - begin);

		if (len < InsertionSortThreshold) {
			if (leftmost) { InsertionSort(begin, end, less); }
			else          { UnguardedInsertionSort(begin, end, less); }
			return;
		}

		// Pivot selection: median-of-3, or Tukey's ninther for large partitions.
		U64 s2 = len / 2;
		if (len > NintherThreshold) {
			Sort3(begin,        begin + s2,       end - 1,      less);
			Sort3(begin + 1,    begin + (s2 - 1), end - 2,      less);
			Sort3(begin + 2,    begin + (s2 + 1), end - 3,      less);
			Sort3(begin + (s2-1), begin + s2, begin + (s2+1),   less);
			Swap(begin, begin + s2);
		} else {
			Sort3(begin + s2, begin, end - 1, less);
		}

		// Many-equals detection: if pivot == *(begin-1), partition left.
		if (!leftmost && !less(*(begin - 1), *begin)) {
			begin = PartitionLeft(begin, end, less) + 1;
			continue;
		}

		auto [pivotPos, alreadyPartitioned] = PartitionRight(begin, end, less);

		U64 lLen = (U64)(pivotPos - begin);
		U64 rLen = (U64)(end - (pivotPos + 1));
		bool highlyUnbalanced = lLen < len / 8 || rLen < len / 8;

		if (highlyUnbalanced) {
			if (--badAllowed == 0) {
				// Too many bad partitions: fall back to heapsort (guaranteed O(n log n)).
				HeapSort(Span<T>(begin, end), less);
				return;
			}

			// Shuffle to break patterns.
			if (lLen >= InsertionSortThreshold) {
				Swap(begin,         begin + lLen / 4);
				Swap(pivotPos - 1, pivotPos - lLen / 4);
				if (lLen > NintherThreshold) {
					Swap(begin + 1,     begin + (lLen / 4 + 1));
					Swap(begin + 2,     begin + (lLen / 4 + 2));
					Swap(pivotPos - 2, pivotPos - (lLen / 4 + 1));
					Swap(pivotPos - 3, pivotPos - (lLen / 4 + 2));
				}
			}
			if (rLen >= InsertionSortThreshold) {
				Swap(pivotPos + 1, pivotPos + (1 + rLen / 4));
				Swap(end - 1,       end - rLen / 4);
				if (rLen > NintherThreshold) {
					Swap(pivotPos + 2, pivotPos + (2 + rLen / 4));
					Swap(pivotPos + 3, pivotPos + (3 + rLen / 4));
					Swap(end - 2,       end - (1 + rLen / 4));
					Swap(end - 3,       end - (2 + rLen / 4));
				}
			}
		} else {
			if (alreadyPartitioned &&
				PartialInsertionSort(begin, pivotPos, less) &&
				PartialInsertionSort(pivotPos + 1, end, less))
				return;
		}

		// Recurse on smaller partition, tail-loop on larger.
		PdqLoop<T, Less>(begin, pivotPos, less, badAllowed, leftmost);
		begin    = pivotPos + 1;
		leftmost = false;
	}
}

//--------------------------------------------------------------------------------------------------

template <class T, class Less>
void PdqSort(Span<T> span, Less less) {
	if (span.len < 2) {
		return;
	}
	PdqLoop<T, Less>(span.data, span.data + span.len, less, (I32)Bit::Bsr64(span.len));
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sort
