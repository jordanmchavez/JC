#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr u64 HashSeed = 0xbdd89aa982704029;

u64 HashCombine(u64 h, void const* data, u64 len);

inline u64 HashCombine(u64 h, s8 s)          { return HashCombine(h,        s.data, s.len); }
inline u64 HashCombine(u64 h, const void* p) { return HashCombine(h,        &p,     sizeof(p)); }
inline u64 HashCombine(u64 h, i64 i)         { return HashCombine(h,        &i,     sizeof(i)); }
inline u64 HashCombine(u64 h, u64 u)         { return HashCombine(h,        &u,     sizeof(u)); }

inline u64 Hash(void const* data, u64 len)   { return HashCombine(HashSeed, data,   len); }
inline u64 Hash(s8 s)                        { return HashCombine(HashSeed, s.data, s.len); }
inline u64 Hash(const void* p)               { return HashCombine(HashSeed, &p,     sizeof(p)); }
inline u64 Hash(i64 i)                       { return (u64)i; }
inline u64 Hash(u64 u)                       { return u; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC