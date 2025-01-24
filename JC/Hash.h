#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr u64 HashSeed = 0xbdd89aa982704029;

// Use PreHash if you're pre-hashing your data to avoid double hashing. Hash(PreHash) is a no-op
struct PreHash {
	u64 hash;

	PreHash() = default;
	PreHash(const PreHash&) = default;
	PreHash(u64 h) { hash = h; };	// Implicit
	PreHash& operator=(const PreHash& p) = default;
	PreHash& operator=(u64 h) { hash = h; return *this; }
};
inline bool operator==(PreHash p1, PreHash p2) { return p1.hash == p2.hash; }

u64 HashCombine(u64 h, void const* data, u64 len);

inline u64 HashCombine(u64 h, s8 s)          { return HashCombine(h,        s.data, s.len); }
inline u64 HashCombine(u64 h, const void* p) { return HashCombine(h,        &p,     sizeof(p)); }
inline u64 HashCombine(u64 h, i64 i)         { return HashCombine(h,        &i,     sizeof(i)); }
inline u64 HashCombine(u64 h, u64 u)         { return HashCombine(h,        &u,     sizeof(u)); }

inline u64 Hash(void const* data, u64 len)   { return HashCombine(HashSeed, data,   len); }
inline u64 Hash(s8 s)                        { return HashCombine(HashSeed, s.data, s.len); }
inline u64 Hash(const void* p)               { return HashCombine(HashSeed, &p,     sizeof(p)); }
inline u64 Hash(i32 i)                       { return HashCombine(HashSeed, &i,     sizeof(i)); }
inline u64 Hash(u32 u)                       { return HashCombine(HashSeed, &u,     sizeof(u)); }
inline u64 Hash(i64 i)                       { return HashCombine(HashSeed, &i,     sizeof(i)); }
inline u64 Hash(u64 u)                       { return HashCombine(HashSeed, &u,     sizeof(u)); }
inline u64 Hash(PreHash h)                   { return h.hash; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC