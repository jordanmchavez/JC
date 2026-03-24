/*
Array  = Fixed-sized array
MArray = Memory-allocated, fixed-sized array. Allocats the buffer once then never expands it
DArray = Dynamic array, like std::vector. Doubles in size on exceed capacity.
*/
#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T, U64 MaxLen> struct Array {
	U64 len = 0;
	T   data[MaxLen];

	Array(Array const&) = delete;
	Array& operator=(Array const&) = delete;

	constexpr       T& operator[](U64 i)       { Assert(i < len); return data[i]; }
	constexpr const T& operator[](U64 i) const { Assert(i < len); return data[i]; }

	operator Span<T>()             { return Span<T>      (data, len); }
	operator Span<T const>() const { return Span<T const>(data, len); }

	T* Add() {
		Assert(len + 1 <= MaxLen);
		memset(&data[len], 0, sizeof(T));
		return &data[len++];
	}

	T* Add(T val) {
		Assert(len + 1 <= MaxLen);
		data[len] = val;
		return &data[len++];
	}

	T* Add(const T* vals, U64 valsLen) {
		Assert(!valsLen || vals);
		Assert(len + valsLen <= MaxLen);
		memcpy(data + len, vals, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(const T* begin, const T* end) {
		Assert((!begin && !end) || (begin && end && begin <= end));
		U64 const valsLen = (U64)(end - begin);
		Assert(len + valsLen <= MaxLen);
		memcpy(data + len, begin, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(Span<T> vals) {
		Assert(len + vals.len <= MaxLen);
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		T* const result = &data[len];
		len += vals.len;
		return result;
	}

	T* Add(Span<T const> vals) {
		Assert(len + vals.len <= MaxLen);
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		T* const result = &data[len];
		len += vals.len;
		return result;
	}

	T* AddN(T val, U64 n) {
		Assert(len + n <= MaxLen);
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

	void Remove(U64 n) {
		Assert(len >= n);
		len -= n;
	}

	void RemoveUnordered(U64 i) {
		Assert(len > 0);
		Assert(i < len);
		len--;
		data[i] = data[len];
	}

	T* Extend(U64 n) {
		Assert(len + n <= MaxLen);
		T* res = data + len;
		memset(data + len, 0, n * sizeof(T));
		len += n;
		return res;
	}

	bool HasCapacity(U64 n = 1) {
		return len + n <= MaxLen;
	}
};

//--------------------------------------------------------------------------------------------------

template <class T> struct MArray {
	U64  len = 0;
	U64  maxLen = 0;
	T*   data = 0;

	MArray() = default;

	MArray(Mem mem, U64 maxLenIn, SrcLoc sl = SrcLoc::Here()) {
		Init(mem, maxLenIn, sl);
	}

	MArray(MArray const&) = delete;
	MArray& operator=(MArray const&) = delete;

	void Init(Mem mem, U64 maxLenIn, SrcLoc sl = SrcLoc::Here()) {
		len    = 0;
		maxLen = maxLenIn;
		data   = Mem::AllocT<T>(mem, maxLenIn, sl);
	}

	constexpr       T& operator[](U64 i)       { Assert(i < len); return data[i]; }
	constexpr const T& operator[](U64 i) const { Assert(i < len); return data[i]; }

	operator Span<T>()             { return Span<T>      (data, len); }
	operator Span<T const>() const { return Span<T const>(data, len); }

	T* Add() {
		Assert(len + 1 <= maxLen);
		memset(&data[len], 0, sizeof(T));
		return &data[len++];
	}

	T* Add(T val) {
		Assert(len + 1 <= maxLen);
		data[len] = val;
		return &data[len++];
	}

	T* Add(const T* vals, U64 valsLen) {
		Assert(!valsLen || vals);
		Assert(len + valsLen <= maxLen);
		memcpy(data + len, vals, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(const T* begin, const T* end) {
		Assert((!begin && !end) || (begin && end && begin <= end));
		U64 const valsLen = (U64)(end - begin);
		Assert(len + valsLen <= maxLen);
		memcpy(data + len, begin, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(Span<T> vals) {
		Assert(len + vals.len <= maxLen);
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		T* const result = &data[len];
		len += vals.len;
		return result;
	}

	T* Add(Span<T const> vals) {
		Assert(len + vals.len <= maxLen);
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		T* const result = &data[len];
		len += vals.len;
		return result;
	}

	T* AddN(T val, U64 n) {
		Assert(len + n <= maxLen);
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

	void Remove(U64 n) {
		Assert(len >= n);
		len -= n;
	}

	void RemoveUnordered(U64 i) {
		Assert(len > 0);
		Assert(i < len);
		len--;
		data[i] = data[len];
	}

	T* Extend(U64 n) {
		Assert(len + n <= maxLen);
		T* res = data + len;
		memset(data + len, 0, n * sizeof(T));
		len += n;
		return res;
	}

	bool HasCapacity(U64 n = 1) {
		return len + n <= maxLen;
	}
};

//--------------------------------------------------------------------------------------------------

template <class T> struct DArray {
	Mem  mem;
	U64  len = 0;
	U64  maxLen = 0;
	T*   data = 0;

	DArray() = default;

	DArray(Mem memIn, U64 maxLenIn, SrcLoc sl = SrcLoc::Here()) {
		Init(memIn, maxLenIn, sl);
	}

	DArray(DArray const&) = delete;
	DArray& operator=(DArray const&) = delete;

	void Init(Mem memIn, U64 maxLenIn, SrcLoc sl = SrcLoc::Here()) {
		mem    = memIn;
		len    = 0;
		maxLen = maxLenIn;
		data   = Mem::AllocT<T>(mem, maxLenIn, sl);
	}

	constexpr       T& operator[](U64 i)       { Assert(i < len); return data[i]; }
	constexpr const T& operator[](U64 i) const { Assert(i < len); return data[i]; }

	operator Span<T>()             { return Span<T>      (data, len); }
	operator Span<T const>() const { return Span<T const>(data, len); }

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

	T* Add(const T* vals, U64 valsLen, SrcLoc sl = SrcLoc::Here()) {
		Assert(!valsLen || vals);
		if (len + valsLen > maxLen) {
			_GrowTo(len + valsLen, sl);
		}
		memcpy(data + len, vals, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(const T* begin, const T* end, SrcLoc sl = SrcLoc::Here()) {
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

	T* Extend(U64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > maxLen) {
			_GrowTo(len + n, sl);
		}
		T* res = data + len;
		memset(data + len, 0, n * sizeof(T));
		len += n;
		return res;
	}

	void _GrowTo(U64 newLen, SrcLoc sl) {
		U64 newMaxLen = Max((maxLen * 2), Max(newLen, (U64)16));
		data = Mem::ReallocT<T>(mem, data, maxLen, newMaxLen, sl);
		maxLen = newMaxLen;
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC