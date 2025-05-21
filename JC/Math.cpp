#include "JC/Math.h"

#include <math.h>

namespace JC::Math {

//--------------------------------------------------------------------------------------------------

Vec2 Add(Vec2 a, Vec2 b) {
	return {
		.x = a.x + b.x,
		.y = a.y + b.y,
	};
}

Vec3 Add(Vec3 a, Vec3 b) {
	return {
		.x = a.x + b.x,
		.y = a.y + b.y,
		.z = a.z + b.z,
	};
}

//--------------------------------------------------------------------------------------------------

Vec2 AddScaled(Vec2 a, Vec2 b, F32 s) {
	return {
		.x = a.x + b.x * s,
		.y = a.y + b.y * s,
	};
}

Vec3 AddScaled(Vec3 a, Vec3 b, F32 s) {
	return {
		.x = a.x + b.x * s,
		.y = a.y + b.y * s,
		.z = a.z + b.z * s,
	};
}

//--------------------------------------------------------------------------------------------------

Mat3 AxisAngleMat3(Vec3 v, F32 a) {
	Vec3 n = Normalize(v);
	F32 c = cosf(a);
	Vec3 vc = Mul(n, 1.0f - c);
	Vec3 vs = Mul(n, sinf(a));

	return {
		vc.x * n.x + c,    vc.y * n.x - vs.z, vc.z * n.x + vs.x,
		vc.x * n.y + vs.z, vc.y * n.y + c,    vc.z * n.y - vs.x,
		vc.x * n.z - vs.y, vc.y * n.z + vs.x, vc.z * n.z + c,
	};
}

Mat4 AxisAngleMat4(Vec3 b, F32 a) {
	Vec3 n = Normalize(b);
	F32 c = cosf(a);
	Vec3 vc = Mul(n, 1.0f - c);
	Vec3 vs = Mul(n, sinf(a));

	return Mat4 {
		vc.x * n.x + c,    vc.x * n.y + vs.z,  vc.x * n.z - vs.y, 0.0f,
		vc.y * n.x - vs.z, vc.y * n.y + c,     vc.y * n.z + vs.x, 0.0f,
		vc.z * n.x + vs.x, vc.z * n.y - vs.x,  vc.z * n.z + c,    0.0f,
		0.0f,              0.0f,               0.0f,              1.0f,

		//vc.x * n.x + c,    vc.y * n.x - vs.z, vc.z * n.x + vs.x, 0.0f,
		//vc.x * n.y + vs.z, vc.y * n.y + c,    vc.z * n.y - vs.x, 0.0f,
		//vc.x * n.z - vs.y, vc.y * n.z + vs.x, vc.z * n.z + c,    0.0f,
		//0.0f,              0.0f,              0.0f,              1.0f,
	};
}

//--------------------------------------------------------------------------------------------------

F32 Dot(Vec3 a, Vec3 b) {
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

//--------------------------------------------------------------------------------------------------

Vec3 Cross(Vec3 a, Vec3 b) {
	return {
		.x = a.y * b.z - a.z * b.y,
		.y = a.z * b.x - a.x * b.z,
		.z = a.x * b.y - a.y * b.x,
	};
}

//--------------------------------------------------------------------------------------------------

Mat2 IdentityMat2() {
	return Mat2 {
		1.0f, 0.0f,
		0.0f, 1.0f,
	};
}

Mat3 IdentityMat3() {
	return Mat3 {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	};
}

Mat4 IdentityMat4() {
	return Mat4 {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}
//--------------------------------------------------------------------------------------------------

F32 Lerp(F32 a, F32 b, F32 t) {
	return a + (t * (b - a));
}

Vec4 Lerp(Vec4 a, Vec4 b, F32 t) {
	return {
		.x = a.x + (t * (b.x - a.x)),
		.y = a.y + (t * (b.y - a.y)),
		.z = a.z + (t * (b.z - a.z)),
		.w = a.w + (t * (b.w - a.w)),
	};
}

//--------------------------------------------------------------------------------------------------

Mat4 Look(Vec3 pos, Vec3 x, Vec3 y, Vec3 z) {
	return Mat4 {
		 x.x,  x.y,  x.z, -Dot(x, pos),
		 y.x,  y.y,  y.z, -Dot(y, pos),
		 z.x,  z.y,  z.z, -Dot(z, pos),
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

/*
Mat4 LookAt(Vec3 eye, Vec3 at, Vec3 up) {
	Vec3 z = Normalize(Sub(eye, at));
	Vec3 x = Normalize(Cross(up, z));
	Vec3 y = Cross(z, x);
	Mat4 m;
	m.m[0][0] =  x.x; m.m[1][0] =  x.y; m.m[2][0] =  x.z; m.m[3][0] = -Dot(x, eye);
	m.m[0][1] =  y.x; m.m[1][1] =  y.y; m.m[2][1] =  y.z; m.m[3][1] = -Dot(y, eye);
	m.m[0][2] =  z.x; m.m[1][2] =  z.y; m.m[2][2] =  z.z; m.m[3][2] = -Dot(z, eye);
	m.m[0][3] = 0.0f; m.m[1][3] = 0.0f; m.m[2][3] = 0.0f; m.m[3][3] =  1.0f;
	return m;
}
*/

//--------------------------------------------------------------------------------------------------

Vec2 Mul(Vec2 b, F32 s) {
	return {
		.x = b.x * s,
		.y = b.y * s,
	};
}

Vec2 Mul(Vec2 a, Vec2 b) {
	return {
		.x = a.x * b.x,
		.y = a.y * b.y,
	};
}

Vec3 Mul(Vec3 b, F32 s) {
	return {
		.x = b.x * s,
		.y = b.y * s,
		.z = b.z * s,
	};
}

Vec3 Mul(Mat3 a, Vec3 b) {
	return Vec3 {
		.x = a.m[0][0] * b.x + a.m[0][1] * b.y + a.m[0][2] * b.z,
		.y = a.m[1][0] * b.x + a.m[1][1] * b.y + a.m[1][2] * b.z,
		.z = a.m[2][0] * b.x + a.m[2][1] * b.y + a.m[2][2] * b.z,
	};
}

Vec4 Mul(Mat4 m, Vec4 b) {
	return Vec4 {
		.x = m.m[0][0] * b.x + m.m[0][1] * b.y + m.m[0][2] * b.z + m.m[0][3] * b.w,
		.y = m.m[1][0] * b.x + m.m[1][1] * b.y + m.m[1][2] * b.z + m.m[1][3] * b.w,
		.z = m.m[2][0] * b.x + m.m[2][1] * b.y + m.m[2][2] * b.z + m.m[2][3] * b.w,
		.w = m.m[3][0] * b.x + m.m[3][1] * b.y + m.m[3][2] * b.z + m.m[3][3] * b.w,
	};
}

Mat2 Mul(Mat2 a, Mat2 b) {
	return Mat2 {
		(a.m[0][0] * b.m[0][0]) + (a.m[0][1] * b.m[1][0]),
		(a.m[0][0] * b.m[0][1]) + (a.m[0][1] * b.m[1][1]),

		(a.m[1][0] * b.m[0][0]) + (a.m[1][1] * b.m[1][0]),
		(a.m[1][0] * b.m[0][1]) + (a.m[1][1] * b.m[1][1]),
	};
}

Mat3 Mul(Mat3 a, Mat3 b) {
	return Mat3 {
		(a.m[0][0] * b.m[0][0]) + (a.m[0][1] * b.m[1][0]) + (a.m[0][2] * b.m[2][0]),
		(a.m[0][0] * b.m[0][1]) + (a.m[0][1] * b.m[1][1]) + (a.m[0][2] * b.m[2][1]),
		(a.m[0][0] * b.m[0][2]) + (a.m[0][1] * b.m[1][2]) + (a.m[0][2] * b.m[2][2]),

		(a.m[1][0] * b.m[0][0]) + (a.m[1][1] * b.m[1][0]) + (a.m[1][2] * b.m[2][0]),
		(a.m[1][0] * b.m[0][1]) + (a.m[1][1] * b.m[1][1]) + (a.m[1][2] * b.m[2][1]),
		(a.m[1][0] * b.m[0][2]) + (a.m[1][1] * b.m[1][2]) + (a.m[1][2] * b.m[2][2]),

		(a.m[2][0] * b.m[0][0]) + (a.m[2][1] * b.m[1][0]) + (a.m[2][2] * b.m[2][0]),
		(a.m[2][0] * b.m[0][1]) + (a.m[2][1] * b.m[1][1]) + (a.m[2][2] * b.m[2][1]),
		(a.m[2][0] * b.m[0][2]) + (a.m[2][1] * b.m[1][2]) + (a.m[2][2] * b.m[2][2]),
	};
};

Mat4 Mul(Mat4 a, Mat4 b) {
	return Mat4 { 
		(a.m[0][0] * b.m[0][0]) + (a.m[0][1] * b.m[1][0]) + (a.m[0][2] * b.m[2][0]) + (a.m[0][3] * b.m[3][0]),
		(a.m[0][0] * b.m[0][1]) + (a.m[0][1] * b.m[1][1]) + (a.m[0][2] * b.m[2][1]) + (a.m[0][3] * b.m[3][1]),
		(a.m[0][0] * b.m[0][2]) + (a.m[0][1] * b.m[1][2]) + (a.m[0][2] * b.m[2][2]) + (a.m[0][3] * b.m[3][2]),
		(a.m[0][0] * b.m[0][3]) + (a.m[0][1] * b.m[1][3]) + (a.m[0][2] * b.m[2][3]) + (a.m[0][3] * b.m[3][3]),

		(a.m[1][0] * b.m[0][0]) + (a.m[1][1] * b.m[1][0]) + (a.m[1][2] * b.m[2][0]) + (a.m[1][3] * b.m[3][0]),
		(a.m[1][0] * b.m[0][1]) + (a.m[1][1] * b.m[1][1]) + (a.m[1][2] * b.m[2][1]) + (a.m[1][3] * b.m[3][1]),
		(a.m[1][0] * b.m[0][2]) + (a.m[1][1] * b.m[1][2]) + (a.m[1][2] * b.m[2][2]) + (a.m[1][3] * b.m[3][2]),
		(a.m[1][0] * b.m[0][3]) + (a.m[1][1] * b.m[1][3]) + (a.m[1][2] * b.m[2][3]) + (a.m[1][3] * b.m[3][3]),

		(a.m[2][0] * b.m[0][0]) + (a.m[2][1] * b.m[1][0]) + (a.m[2][2] * b.m[2][0]) + (a.m[2][3] * b.m[3][0]),
		(a.m[2][0] * b.m[0][1]) + (a.m[2][1] * b.m[1][1]) + (a.m[2][2] * b.m[2][1]) + (a.m[2][3] * b.m[3][1]),
		(a.m[2][0] * b.m[0][2]) + (a.m[2][1] * b.m[1][2]) + (a.m[2][2] * b.m[2][2]) + (a.m[2][3] * b.m[3][2]),
		(a.m[2][0] * b.m[0][3]) + (a.m[2][1] * b.m[1][3]) + (a.m[2][2] * b.m[2][3]) + (a.m[2][3] * b.m[3][3]),

		(a.m[3][0] * b.m[0][0]) + (a.m[3][1] * b.m[1][0]) + (a.m[3][2] * b.m[2][0]) + (a.m[3][3] * b.m[3][0]),
		(a.m[3][0] * b.m[0][1]) + (a.m[3][1] * b.m[1][1]) + (a.m[3][2] * b.m[2][1]) + (a.m[3][3] * b.m[3][1]),
		(a.m[3][0] * b.m[0][2]) + (a.m[3][1] * b.m[1][2]) + (a.m[3][2] * b.m[2][2]) + (a.m[3][3] * b.m[3][2]),
		(a.m[3][0] * b.m[0][3]) + (a.m[3][1] * b.m[1][3]) + (a.m[3][2] * b.m[2][3]) + (a.m[3][3] * b.m[3][3]),
	};
}

//--------------------------------------------------------------------------------------------------

Vec3 Normalize(Vec3 b) {
	const F32 s = 1.0f / sqrtf(b.x * b.x + b.y * b.y + b.z * b.z);
	return {
		.x = b.x * s,
		.y = b.y * s,
		.z = b.z * s,
	};
}

//--------------------------------------------------------------------------------------------------

Mat4 Ortho(F32 l, F32 r, F32 b, F32 t, F32 n, F32 f) {
	return Mat4 {
		2.0f / (r - l),     0.0f,               0.0f,           0.0f,
		0.0f,               2.0f / (b - t),     0.0f,           0.0f,
		0.0f,               0.0f,               1.0f / (n - f), 0.0f,
		-(r + l) / (r - l), -(b + t) / (b - t), n / (n - f),    1.0f,
	};
}

//--------------------------------------------------------------------------------------------------

Mat4 Perspective(F32 fovy, F32 aspect, F32 zn, F32 zf) {
	const F32 ht = tanf(fovy / 2.0f);
	return Mat4 {
		1.0f / (aspect * ht), 0.0f,       0.0f,           0.0f,
		0.0f,                 -1.0f / ht, 0.0f,           0.0f,
		0.0f,                 0.0f,       zf / (zn - zf), -(zf * zn) / (zf - zn),
		0.0f,                 0.0f,       -1.0f,          0.0f,
	};
}

//--------------------------------------------------------------------------------------------------

Mat3 RotationXMat3(F32 a) {
	const F32 s = sinf(a);
	const F32 c = cosf(a);
	return Mat3 {
		1.0f, 0.0f, 0.0f,
		0.0f,    c,   -s,
		0.0f,    s,    c,
	};
}

Mat3 RotationYMat3(F32 a) {
	const F32 s = sinf(a);
	const F32 c = cosf(a);
	return Mat3 {
		   c, 0.0f,    s,
		0.0f, 1.0f, 0.0f,
		  -s, 0.0f,    c,
	};
}

Mat3 RotationZMat3(F32 a) {
	const F32 s = sinf(a);
	const F32 c = cosf(a);
	return Mat3 {
		   c,   -s, 0.0f,
		   s,    c, 0.0f,
		0.0f, 0.0f, 1.0f,
	};
}

Mat4 RotationXMat4(F32 a) {
	const F32 s = sinf(a);
	const F32 c = cosf(a);
	return Mat4 {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f,    c,   -s, 0.0f,
		0.0f,    s,    c, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 RotationYMat4(F32 a) {
	const F32 s = sinf(a);
	const F32 c = cosf(a);
	return Mat4 {
		   c, 0.0f,    s, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		  -s, 0.0f,    c, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 RotationZMat4(F32 a) {
	const F32 s = sinf(a);
	const F32 c = cosf(a);
	return Mat4 {
		   c,   -s, 0.0f, 0.0f,
		   s,    c, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}


//--------------------------------------------------------------------------------------------------

Vec3 Sub(Vec3 a, Vec3 b) {
	return {
		.x = a.x - b.x,
		.y = a.y - b.y,
		.z = a.z - b.z,
	};
}

//--------------------------------------------------------------------------------------------------

Mat4 TranslationMat4(Vec3 b) {
	return Mat4 {
		1.0f, 0.0f, 0.0f,  0.f,
		0.0f, 1.0f, 0.0f,  0.f,
		0.0f, 0.0f, 1.0f,  0.f,
		 b.x,  b.y,  b.z, 1.0f,
	};
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Math