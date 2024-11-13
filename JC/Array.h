#pragma once

#include "JC/Common.h"

namespace JC {

struct Allocator;

//--------------------------------------------------------------------------------------------------

template <class T>
struct Array {
	Allocator* alloc = nullptr;
	T*         data  = nullptr;
	u64        len   = 0;
	u64        cap   = 0;

	constexpr       T& operator[](u64 i)       { return data[i]; }
	constexpr const T& operator[](u64 i) const { return data[i]; }

	operator Span<T>() const { return Span(data, len); }

	constexpr const T* Begin() const { return data; }
	constexpr const T* End()   const { return data + len; }

	void Init(Allocator* inAlloc) {
		alloc = inAlloc;
		data  = 0;
		len   = 0;
		cap   = 0;
	}

	void Shutdown() {
		if (alloc) {
			alloc->Free(data, len * sizeof(T));
		}
		alloc = 0;
		data  = 0;
		len   = 0;
		cap   = 0;
	}

	T* Add(SrcLoc srcLoc = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, srcLoc);
		}
		JC_MEMSET(&data[len], 0, sizeof(data[len]));
		len++;
		return &data[len - 1];
	}

	T* Add(T val, SrcLoc srcLoc = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, srcLoc);
		}
		data[len++] = val;
		return &data[len - 1];
	}

	void Add(const T* vals, u64 valsLen, SrcLoc srcLoc = SrcLoc::Here()) {
		if (len + valsLen > cap) {
			Grow(len + valsLen, srcLoc);
		}
		JC_MEMCPY(data + len, vals, valsLen * sizeof(T));
		len += valsLen;
	}

	void Add(const T* begin, const T* end, SrcLoc srcLoc = SrcLoc::Here()) {
		const u64 valsLen = (u64)(end - begin);
		if (len + valsLen > cap) {
			Grow(len + valsLen, srcLoc);
		}
		JC_MEMCPY(data + len, begin, valsLen * sizeof(T));
		len += valsLen;
	}

	void Fill(T val, u64 n, SrcLoc srcLoc = SrcLoc::Here()) {
		if (len + n > cap) {
			Grow(len + n, srcLoc);
		}
		if constexpr (sizeof(T) == 1) {
			JC_MEMSET(data + len, (int)val, n);
		} else {
			T* const end = data + n;
			for (T* iter = data + len; iter <= end; ++iter) {
				*iter = val;
			}
		}
		len += n;
	}

	void Insert(u32 i, T val, SrcLoc srcLoc = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, srcLoc);
		}
		JC_MEMMOVE(data + i + 1, data + i, (len - i) * sizeof(T));
		data[i] = val;
		++len;
	}

	void Remove() {
		--len;
	}

	void Remove(u64 n) {
		--len -= n;
	}

	T* Extend(u64 n, SrcLoc srcLoc = SrcLoc::Here()) {
		if (len + n > cap) {
			Grow(len + n, srcLoc);
		}
		T* res = data + len;
		len += n;
		return res;
	}

	T* Resize(u64 newLen, SrcLoc srcLoc = SrcLoc::Here()) {
		if (newLen > cap) {
			Grow(newLen, srcLoc);
		}
		T* res = data + len;
		len = newLen;
		return res;
	}

	T* Reserve(u64 newCap, SrcLoc srcLoc = SrcLoc::Here()) {
		if (newCap > cap) {
			Grow(newCap, srcLoc);
		}
		return data + len;
	}

	void Grow(u64 newCap, SrcLoc srcLoc = SrcLoc::Here()) {
		if (newCap < cap * 2) {
			newCap = cap * 2;
		}
		newCap = Max(Max((u64)16, newCap), cap * 2u);
		data = (T*)alloc->Realloc(data, cap * sizeof(T), newCap * sizeof(T), srcLoc);
		cap = newCap;
	}

	void Clear() {
		len = 0;
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC