#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct Array {
	T*   data = 0;
	U64  len = 0;
	U64  cap = 0;

	Array() = default;

	Array(Mem mem, U64 capIn, SrcLoc sl = SrcLoc::Here()) {
		Init(mem, capIn, sl);
	}

	void Init(Mem mem, U64 capIn, SrcLoc sl = SrcLoc::Here()) {
		data = Mem::AllocT<T>(mem, capIn, sl);
		len  = 0;
		cap  = capIn;
	}

	constexpr       T& operator[](U64 i)       { Assert(i < len); return data[i]; }
	constexpr const T& operator[](U64 i) const { Assert(i < len); return data[i]; }

	operator Span<T>() const { return Span(data, len); }

	constexpr const T* Begin() const { return data; }
	constexpr const T* End()   const { return data + len; }

	T* Add() {
		Assert(len < cap);
		memset(&data[len], 0, sizeof(data[len]));
		return &data[len++];
	}

	T* Add(T val) {
		Assert(len < cap);
		data[len] = val;
		return &data[len++];
	}

	void Add(const T* vals, U64 valsLen) {
		Assert(!valsLen || vals);
		Assert(len + valsLen <= cap);
		memcpy(data + len, vals, valsLen * sizeof(T));
		len += valsLen;
	}

	void Add(const T* begin, const T* end) {
		Assert(begin <= end);
		U64 const valsLen = (U64)(end - begin);
		Assert(len + valsLen <= cap);
		memcpy(data + len, begin, valsLen * sizeof(T));
		len += valsLen;
	}

	void Add(Span<T> vals) {
		Assert(len + vals.len <= cap);
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		len += vals.len;
	}

	void Fill(T val, U64 n) {
		Assert(len + n <= cap);
		if constexpr (sizeof(T) == 1) {
			memset(data + len, (int)val, n);
		} else {
			T* const end = data + len + n;
			for (T* iter = data + len; iter < end; ++iter) {
				*iter = val;
			}
		}
		len += n;
	}

	void Insert(U64 i, T val) {
		Assert(len < cap);
		memmove(data + i + 1, data + i, (len - i) * sizeof(T));
		data[i] = val;
		len++;
	}

	void Remove() {
		len--;
	}

	void Remove(U64 n) {
		len -= n;
	}

	void RemoveUnordered(U64 i) {
		Assert(i < len);
		len--;
		data[i] = data[len];
	}

	T* Extend(U64 n) {
		Assert(len + n <= cap);
		T* res = data + len;
		memset(data + len, 0, n * sizeof(T));
		len += n;
		return res;
	}

	T* Resize(U64 newLen) {
		Assert(newLen <= cap);
		T* res = data + len;
		memset(data + len, 0, newLen * sizeof(T));
		len = newLen;
		return res;
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC