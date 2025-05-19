#pragma once

#include "JC/Hash.h"

#if defined(_MSC_VER)
	#include <intrin.h>
	#if defined(_M_X64) && !defined(_M_ARM64EC)
		#pragma intrinsic(_umul128)
	#endif
#endif

namespace JC {

//-------------------------------------------------------------------------------------------------

/*
* rapidhash: https://github.com/Nicoshev/rapidhash
* Copyright (C) 2024 Nicolas De Carli
*
* BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following disclaimer
*      in the documentation and/or other materials provided with the
*      distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#define Rapid_Likely(x)  __builtin_expect(x,1)
#define Rapid_Unlikely(x)  __builtin_expect(x,0)
#else
#define Rapid_Likely(x) (x)
#define Rapid_Unlikely(x) (x)
#endif


static void Rapid_Mum(U64* A, U64* B){
	#if defined(__SIZEOF_INT128__)
		__uint128_t r = *a;
		r *= *b; 
		*A ^= (U64)r;
		*B ^= (U64)(r >> 64);
	#elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
		#if defined(_M_X64)
			U64  a,  b;
			a = _umul128(*A, *B, &b);
			*A ^= a;
			*B ^= b;
		#else
			U64 a, b;
			b = __umulh(*A, *B);
			a = *A * *B;
			*A ^= a;
			*B ^= b;
		#endif
	#else
		U64 ha = *A >> 32;
		U64 hb = *B >> 32;
		U64 la = (U32)*A;
		U64 lb = (U32)*B
		U64 rh = ha * hb;
		U64 rm0 = ha * lb;
		U64 rm1 = hb * la;
		U64 rl = la * lb;
		U64 t = rl + (rm0 << 32);
		U64 c = t < rl;
		U64 lo = t + (rm1 << 32);
		c += lo < t;
		U64 hi = rh + (rm0 >> 32) + (rm1 > >32) + c;
		*A ^= lo;
		*B ^= hi;
	#endif
}

static U64 Rapid_Mix(U64 a, U64 b) {
	Rapid_Mum(&a, &b);
	return a ^ b;
}

static U64 Rapid_Read64(const U8* p) {
	U64 v;
	memcpy(&v, p, 8);
	return v;
}

static U64 Rapid_Read32(const U8* p) {
	U32 v;
	memcpy(&v, p, 4);
	return v;
}

static U64 Rapid_ReadSmall(const U8* p, U64 k) {
	return (((U64)p[0]) << 56) | (((U64)p[k >> 1]) << 32) | p[k - 1];
}

static constexpr U64 Rapid_Secret[3] = { 0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull, 0x4b33a62ed433d4a3ull };

U64 HashCombine(U64 seed, const void* data, U64 len) {
	const U8* p = (const U8*)data;
	seed ^= Rapid_Mix(seed ^ Rapid_Secret[0], Rapid_Secret[1]) ^ len;
	U64 a, b;
	if (Rapid_Likely(len <= 16)) {
		if (Rapid_Likely(len >= 4)) {
			const U8* plast = p + len - 4;
			a = (Rapid_Read32(p) << 32) | Rapid_Read32(plast);
			const U64 delta = ((len & 24) >> (len >> 3));
			b = ((Rapid_Read32(p + delta) << 32) | Rapid_Read32(plast - delta));
		} else if (Rapid_Likely(len > 0)) {
			a = Rapid_ReadSmall(p, len);
			b = 0;
		} else {
			a = 0;
			b = 0;
		}
	} else {
		U64 i = len; 
		if (Rapid_Unlikely(i > 48)) {
			U64 see1 = seed;
			U64 see2 = seed;
			while (Rapid_Likely(i >= 96)) {
				seed = Rapid_Mix(Rapid_Read64(p     ) ^ Rapid_Secret[0], Rapid_Read64(p +  8) ^ seed);
				see1 = Rapid_Mix(Rapid_Read64(p + 16) ^ Rapid_Secret[1], Rapid_Read64(p + 24) ^ see1);
				see2 = Rapid_Mix(Rapid_Read64(p + 32) ^ Rapid_Secret[2], Rapid_Read64(p + 40) ^ see2);
				seed = Rapid_Mix(Rapid_Read64(p + 48) ^ Rapid_Secret[0], Rapid_Read64(p + 56) ^ seed);
				see1 = Rapid_Mix(Rapid_Read64(p + 64) ^ Rapid_Secret[1], Rapid_Read64(p + 72) ^ see1);
				see2 = Rapid_Mix(Rapid_Read64(p + 80) ^ Rapid_Secret[2], Rapid_Read64(p + 88) ^ see2);
				p += 96;
				i -= 96;
			}
			if (Rapid_Unlikely(i >= 48)) {
				seed = Rapid_Mix(Rapid_Read64(p     ) ^ Rapid_Secret[0], Rapid_Read64(p +  8) ^ seed);
				see1 = Rapid_Mix(Rapid_Read64(p + 16) ^ Rapid_Secret[1], Rapid_Read64(p + 24) ^ see1);
				see2 = Rapid_Mix(Rapid_Read64(p + 32) ^ Rapid_Secret[2], Rapid_Read64(p + 40) ^ see2);
				p += 48;
				i -= 48;
			}
			seed ^= see1 ^ see2;
		}
		if (i > 16) {
			seed = Rapid_Mix(Rapid_Read64(p) ^ Rapid_Secret[2], Rapid_Read64(p + 8) ^ seed ^ Rapid_Secret[1]);
			if (i > 32) {
				seed = Rapid_Mix(Rapid_Read64(p + 16) ^ Rapid_Secret[2], Rapid_Read64(p + 24) ^ seed);
			}
		}
		a = Rapid_Read64(p + i - 16);
		b = Rapid_Read64(p + i - 8);
	}
	a ^= Rapid_Secret[1];
	b ^= seed;
	Rapid_Mum(&a, &b);
	return Rapid_Mix(a ^ Rapid_Secret[0] ^ len, b ^ Rapid_Secret[1]);
}

//-------------------------------------------------------------------------------------------------

}	// namespace JC