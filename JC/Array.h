#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

template <class T> struct Array {
	Mem* mem;
	T*   data;
	U64  len;
	U64  cap;

	Array() { Init(0); }

	explicit Array(Mem* mem_) {
		Init(mem_);
	}

	Array(Mem* mem_, U64 size) {
		Init(mem_);
		Resize(size);
	}

	void Init(Mem* mem_) {
		mem = mem_;
		data = 0;
		len  = 0;
		cap  = 0;
	}

	constexpr       T& operator[](U64 i)       { Assert(i < len); return data[i]; }
	constexpr const T& operator[](U64 i) const { Assert(i < len); return data[i]; }

	operator Span<T>() const { return Span(data, len); }

	constexpr const T* Begin() const { return data; }
	constexpr const T* End()   const { return data + len; }

	T* Add(SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, sl);
		}
		memset(&data[len], 0, sizeof(data[len]));
		return &data[len++];
	}

	T* Add(T val, SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, sl);
		}
		data[len] = val;
		return &data[len++];
	}

	void Add(const T* vals, U64 valsLen, SrcLoc sl = SrcLoc::Here()) {
		Assert(!valsLen || vals);
		if (len + valsLen > cap) {
			Grow(len + valsLen, sl);
		}
		memcpy(data + len, vals, valsLen * sizeof(T));
		len += valsLen;
	}

	void Add(const T* begin, const T* end, SrcLoc sl = SrcLoc::Here()) {
		Assert(begin <= end);
		const U64 valsLen = (U64)(end - begin);
		if (len + valsLen > cap) {
			Grow(len + valsLen, sl);
		}
		memcpy(data + len, begin, valsLen * sizeof(T));
		len += valsLen;
	}

	void Add(Span<T> vals, SrcLoc sl = SrcLoc::Here()) {
		if (len + vals.len > cap) {
			Grow(len + vals.len, sl);
		}
		memcpy(data + len, vals,data, vals.len * sizeof(T));
		len += vals.len;
	}

	void Fill(T val, U64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > cap) {
			Grow(len + n, sl);
		}
		if constexpr (sizeof(T) == 1) {
			memset(data + len, (int)val, n);
		} else {
			T* const end = data + n;
			for (T* iter = data + len; iter <= end; ++iter) {
				*iter = val;
			}
		}
		len += n;
	}

	void Insert(U64 i, T val, SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, sl);
		}
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

	T* Extend(U64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > cap) {
			Grow(len + n, sl);
		}
		T* res = data + len;
		memset(data + len, 0, n * sizeof(T));
		len += n;
		return res;
	}

	T* Resize(U64 newLen, SrcLoc sl = SrcLoc::Here()) {
		if (newLen > cap) {
			Grow(newLen, sl);
		}
		T* res = data + len;
		memset(data + len, 0, newLen * sizeof(T));
		len = newLen;
		return res;
	}

	T* Reserve(U64 newCap, SrcLoc sl = SrcLoc::Here()) {
		if (newCap > cap) {
			Grow(newCap, sl);
		}
		return data + len;
	}

	void Grow(U64 newCap, SrcLoc sl = SrcLoc::Here()) {
		Assert(newCap > cap);
		newCap = Max(Max((U64)16, newCap), cap * 2);
		if (!Mem_Extend(mem, data, newCap * sizeof(T), sl)) {
			T* newData = (T*)Mem_Alloc(mem, newCap * sizeof(T), sl);
			memcpy(data, newData, len * sizeof(T));
			data = newData;
		}
		cap = newCap;
	}
};