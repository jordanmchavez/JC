#pragma once

#include "JC/Common_Std.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr U64 HashSeed = 0xbdd89aa982704029;

// Use PreHash if you're pre-hashing your data to avoid double hashing. Hash(PreHash) is a no-op
struct PreHash {
	U64 hash;

	constexpr PreHash() = default;
	constexpr PreHash(const PreHash&) = default;
	constexpr PreHash(U64 h) { hash = h; };	// Implicit
	constexpr PreHash& operator=(const PreHash& p) = default;
	constexpr PreHash& operator=(U64 h) { hash = h; return *this; }
};
inline bool operator==(PreHash p1, PreHash p2) { return p1.hash == p2.hash; }

U64 HashCombine(U64 h, void const* data, U64 len);

inline U64 HashCombine(U64 h, Str s)         { return HashCombine(h,        s.data, s.len); }
inline U64 HashCombine(U64 h, const void* p) { return HashCombine(h,        &p,     sizeof(p)); }
inline U64 HashCombine(U64 h, I64 i)         { return HashCombine(h,        &i,     sizeof(i)); }
inline U64 HashCombine(U64 h, U64 u)         { return HashCombine(h,        &u,     sizeof(u)); }

inline U64 Hash(void const* data, U64 len)   { return HashCombine(HashSeed, data,   len); }
inline U64 Hash(Str s)                       { return HashCombine(HashSeed, s.data, s.len); }
inline U64 Hash(const void* p)               { return HashCombine(HashSeed, &p,     sizeof(p)); }
inline U64 Hash(I32 i)                       { return HashCombine(HashSeed, &i,     sizeof(i)); }
inline U64 Hash(U32 u)                       { return HashCombine(HashSeed, &u,     sizeof(u)); }
inline U64 Hash(I64 i)                       { return HashCombine(HashSeed, &i,     sizeof(i)); }
inline U64 Hash(U64 u)                       { return HashCombine(HashSeed, &u,     sizeof(u)); }
inline U64 Hash(PreHash h)                   { return h.hash; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC