#pragma once

#include <string.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct Span {
	T* data;
	u32 len;
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

}	// namespace JC