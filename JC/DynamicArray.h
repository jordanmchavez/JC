#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct DynamicArray {
	Mem  mem;
	U64  len = 0;
	U64  maxLen = 0;
	T*   data = 0;

	DynamicArray() = default;

	DynamicArray(Mem memIn, U64 maxLenIn, SrcLoc sl = SrcLoc::Here()) {
		Init(memIn, maxLenIn, sl);
	}

	DynamicArray(DynamicArray const&) = delete;
	DynamicArray& operator=(DynamicArray const&) = delete;

	void Init(Mem memIn, U64 maxLenIn, SrcLoc sl = SrcLoc::Here()) {
		mem    = memIn;
		len    = 0;
		maxLen = maxLenIn;
		data   = Mem::AllocT<T>(mem, maxLenIn, sl);
	}

	constexpr T      & operator[](U64 i)       { Assert(i < len); return data[i]; }
	constexpr T const& operator[](U64 i) const { Assert(i < len); return data[i]; }

	T* Add(SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > maxLen) {
			_GrowTo(len + 1, sl);
		}
		memset(&data[len], 0, sizeof(T));
		return &data[len++];
	}

	T* Add(T val, SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > maxLen) {
			_GrowTo(len + 1, sl);
		}
		data[len] = val;
		return &data[len++];
	}

	T* Add(T const* vals, U64 valsLen, SrcLoc sl = SrcLoc::Here()) {
		Assert(!valsLen || vals);
		if (len + valsLen > maxLen) {
			_GrowTo(len + valsLen, sl);
		}
		memcpy(data + len, vals, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(T const* begin, T const* end, SrcLoc sl = SrcLoc::Here()) {
		Assert((!begin && !end) || (begin && end && begin <= end));
		U64 const valsLen = (U64)(end - begin);
		if (len + valsLen > maxLen) {
			_GrowTo(len + valsLen, sl);
		}
		memcpy(data + len, begin, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(Span<T> vals, SrcLoc sl = SrcLoc::Here()) {
		if (len + vals.len > maxLen) {
			_GrowTo(len + vals.len, sl);
		}
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		T* const result = &data[len];
		len += vals.len;
		return result;
	}

	T* Add(Span<T const> vals, SrcLoc sl = SrcLoc::Here()) {
		if (len + vals.len > maxLen) {
			_GrowTo(len + vals.len, sl);
		}
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		T* const result = &data[len];
		len += vals.len;
		return result;
	}

	T* AddN(U64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > maxLen) {
			_GrowTo(len + n, sl);
		}
		T* res = data + len;
		memset(data + len, 0, n * sizeof(T));
		len += n;
		return res;
	}

	T* AddN(T val, U64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > maxLen) {
			_GrowTo(len + n, sl);
		}
		if constexpr (sizeof(T) == 1) {
			memset(data + len, (int)val, n);
		} else {
			T* const end = data + len + n;
			for (T* iter = data + len; iter < end; ++iter) {
				*iter = val;
			}
		}
		T* const result = &data[len];
		len += n;
		return result;
	}

	void Remove() {
		Assert(len > 0);
		len--;
	}

	void RemoveN(U64 n) {
		Assert(len >= n);
		len -= n;
	}

	void RemoveUnordered(U64 i) {
		Assert(len > 0);
		Assert(i < len);
		len--;
		data[i] = data[len];
	}

	void _GrowTo(U64 newLen, SrcLoc sl) {
		U64 newMaxLen = Max((maxLen * 2), Max(newLen, (U64)16));
		data = Mem::ReallocT<T>(mem, data, maxLen, newMaxLen, sl);
		maxLen = newMaxLen;
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC