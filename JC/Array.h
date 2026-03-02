#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct Array {
	Mem  mem;
	T*   data = 0;
	U64  len = 0;
	U64  cap = 0;

	Array() = default;

	Array(Mem memIn, U64 capIn, SrcLoc sl = SrcLoc::Here()) {
		Init(memIn, capIn, sl);
	}

	Array(Array const&) = delete;
	Array& operator=(Array const&) = delete;

	void Init(Mem memIn, U64 capIn, SrcLoc sl = SrcLoc::Here()) {
		mem = memIn;
		data = Mem::AllocT<T>(mem, capIn, sl);
		len  = 0;
		cap  = capIn;
	}

	constexpr       T& operator[](U64 i)       { Assert(i < len); return data[i]; }
	constexpr const T& operator[](U64 i) const { Assert(i < len); return data[i]; }

	operator Span<T>()             { return Span<T>      (data, len); }
	operator Span<T const>() const { return Span<T const>(data, len); }

	T* Add(SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > cap) {
			GrowTo(len + 1, sl);
		}
		memset(&data[len], 0, sizeof(T));
		return &data[len++];
	}

	T* Add(T val, SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > cap) {
			GrowTo(len + 1, sl);
		}
		data[len] = val;
		return &data[len++];
	}

	T* Add(const T* vals, U64 valsLen, SrcLoc sl = SrcLoc::Here()) {
		Assert(!valsLen || vals);
		if (len + valsLen > cap) {
			GrowTo(len + valsLen, sl);
		}
		memcpy(data + len, vals, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(const T* begin, const T* end, SrcLoc sl = SrcLoc::Here()) {
		Assert((!begin && !end) || (begin && end && begin <= end));
		U64 const valsLen = (U64)(end - begin);
		if (len + valsLen > cap) {
			GrowTo(len + valsLen, sl);
		}
		memcpy(data + len, begin, valsLen * sizeof(T));
		T* const result = &data[len];
		len += valsLen;
		return result;
	}

	T* Add(Span<T> vals, SrcLoc sl = SrcLoc::Here()) {
		if (len + vals.len > cap) {
			GrowTo(len + vals.len, sl);
		}
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		T* const result = &data[len];
		len += vals.len;
		return result;
	}

	T* Add(Span<T const> vals, SrcLoc sl = SrcLoc::Here()) {
		if (len + vals.len > cap) {
			GrowTo(len + vals.len, sl);
		}
		memcpy(data + len, vals.data, vals.len * sizeof(T));
		T* const result = &data[len];
		len += vals.len;
		return result;
	}

	T* AddN(T val, U64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > cap) {
			GrowTo(len + n, sl);
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

	T* Insert(U64 i, T val, SrcLoc sl = SrcLoc::Here()) {
		Assert(i <= len);
		if (len + 1 > cap) {
			GrowTo(len + 1, sl);
		}
		memmove(data + i + 1, data + i, (len - i) * sizeof(T));
		data[i] = val;
		len++;
		return &data[i];
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

	T* Extend(U64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > cap) {
			GrowTo(len + n, sl);
		}
		T* res = data + len;
		memset(data + len, 0, n * sizeof(T));
		len += n;
		return res;
	}

	void GrowTo(U64 newLen, SrcLoc sl) {
		U64 newCap = Max((cap * 2), Max(newLen, (U64)16));
		data = Mem::ReallocT<T>(mem, data, cap, newCap, sl);
		cap = newCap;
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC