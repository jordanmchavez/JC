#pragma once

#include <string.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct Span {
	T* data;
	u32 len;
};

//--------------------------------------------------------------------------------------------------


struct NullOpt {};
inline NullOpt nullOpt;

template <class T> struct [[nodiscard]] Opt {
	T val;
	bool hasVal;

	constexpr Opt()        {          hasVal = false; }
	constexpr Opt(T v)     { val = v; hasVal = true;  }
	constexpr Opt(NullOpt) {          hasVal = false; }

	constexpr operator bool() const { return hasVal; }

	constexpr T Or (T def) const { return hasVal ? val : def; };
};

//--------------------------------------------------------------------------------------------------

struct Time { u64 ticks; };

struct Rect { u32 x, y, w, h; };

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

// Column-major
struct Mat2 { float m[2][2]; };
struct Mat3 { float m[3][3]; };
struct Mat4 { float m[4][4]; };

//--------------------------------------------------------------------------------------------------

template <class T> constexpr T Min(T t1, T t2) { return t1 < t2 ? t1 : t2; }
template <class T> constexpr T Max(T t1, T t2) { return t1 > t2 ? t1 : t2; }

template <class T> constexpr T Clamp(T lo, T t, T hi) { return t < lo ? lo : (t > hi ? hi : t); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC