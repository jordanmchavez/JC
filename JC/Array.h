#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct Array {
	Arena* arena  = 0;
	T*     data   = 0;
	u64    len    = 0;
	u64    cap    = 0;

	Array() = default;
	Array(Arena* arenaIn) { arena = arenaIn; }

	void Init(Arena* arenaIn) { arena = arenaIn; }

	void Init(Arena* arenaIn, u64 initLen) {
		arena = arenaIn;
		Resize(initLen);
	}

	constexpr       T& operator[](u64 i)       { return data[i]; }
	constexpr const T& operator[](u64 i) const { return data[i]; }

	operator Span<T>() const { return Span(data, len); }

	constexpr const T* Begin() const { return data; }
	constexpr const T* End()   const { return data + len; }

	T* Add(SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, sl);
		}
		MemSet(&data[len], 0, sizeof(data[len]));
		len++;
		return &data[len - 1];
	}

	T* Add(T val, SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, sl);
		}
		data[len++] = val;
		return &data[len - 1];
	}

	void Add(const T* vals, u64 valsLen, SrcLoc sl = SrcLoc::Here()) {
		if (len + valsLen > cap) {
			Grow(len + valsLen, sl);
		}
		MemCpy(data + len, vals, valsLen * sizeof(T));
		len += valsLen;
	}

	void Add(const T* begin, const T* end, SrcLoc sl = SrcLoc::Here()) {
		const u64 valsLen = (u64)(end - begin);
		if (len + valsLen > cap) {
			Grow(len + valsLen, sl);
		}
		MemCpy(data + len, begin, valsLen * sizeof(T));
		len += valsLen;
	}

	void Add(Span<T> vals, SrcLoc sl = SrcLoc::Here()) {
		if (len + vals.len > cap) {
			Grow(len + vals.len, sl);
		}
		MemCpy(data + len, vals,data, vals.len * sizeof(T));
		len += vals.len;
	}

	void Fill(T val, u64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > cap) {
			Grow(len + n, sl);
		}
		if constexpr (sizeof(T) == 1) {
			MemSet(data + len, (int)val, n);
		} else {
			T* const end = data + n;
			for (T* iter = data + len; iter <= end; ++iter) {
				*iter = val;
			}
		}
		len += n;
	}

	void Insert(u32 i, T val, SrcLoc sl = SrcLoc::Here()) {
		if (len + 1 > cap) {
			Grow(len + 1, sl);
		}
		MemMove(data + i + 1, data + i, (len - i) * sizeof(T));
		data[i] = val;
		++len;
	}

	void Remove() {
		--len;
	}

	void Remove(u64 n) {
		--len -= n;
	}

	bool RemoveUnordered(T val) {
		for (u64 i = 0; i < len; i++) {
			if (data[i] == val) {
				len--;
				data[i] = data[len];
				return true;
			}
		}
		return false;
	}

	T* Extend(u64 n, SrcLoc sl = SrcLoc::Here()) {
		if (len + n > cap) {
			Grow(len + n, sl);
		}
		T* res = data + len;
		len += n;
		return res;
	}

	T* Resize(u64 newLen, SrcLoc sl = SrcLoc::Here()) {
		if (newLen > cap) {
			Grow(newLen, sl);
		}
		T* res = data + len;
		len = newLen;
		return res;
	}

	T* Reserve(u64 newCap, SrcLoc sl = SrcLoc::Here()) {
		if (newCap > cap) {
			Grow(newCap, sl);
		}
		return data + len;
	}

	void Grow(u64 newCap, SrcLoc sl = SrcLoc::Here()) {
		Assert(newCap > cap);
		newCap = Max(Max((u64)16, newCap), cap * 2);
		if (!arena->Extend(data, cap * sizeof(T), newCap * sizeof(T), sl)) {
			T* newData = (T*)arena->Alloc(newCap * sizeof(T), sl);
			MemCpy(newData, data, len * sizeof(T));
			data = newData;
		}
		cap = newCap;
	}

	void Clear() {
		len = 0;
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC