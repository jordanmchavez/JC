#include "JC/Math.h"

#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

Vec3 Vec3::Add(Vec3 a, Vec3 b) {
	return {
		.x = a.x + b.x,
		.y = a.y + b.y,
		.z = a.z + b.z,
	};
}

Vec3 Vec3::AddScaled(Vec3 a, Vec3 b, f32 s) {
	return {
		.x = a.x + b.x * s,
		.y = a.y + b.y * s,
		.z = a.z + b.z * s,
	};
}

Vec3 Vec3::Sub(Vec3 a, Vec3 b) {
	return {
		.x = a.x - b.x,
		.y = a.y - b.y,
		.z = a.z - b.z,
	};
}

Vec3 Vec3::Scale(Vec3 b, f32 s) {
	return {
		.x = b.x * s,
		.y = b.y * s,
		.z = b.z * s,
	};
}

f32 Vec3::Dot(Vec3 a, Vec3 b) {
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

Vec3 Vec3::Cross(Vec3 a, Vec3 b) {
	return {
		.x = a.y * b.z - a.z * b.y,
		.y = a.z * b.x - a.x * b.z,
		.z = a.x * b.y - a.y * b.x,
	};
}

Vec3 Vec3::Normalize(Vec3 b) {
	const f32 s = 1.0f / sqrtf(b.x * b.x + b.y * b.y + b.z * b.z);
	return {
		.x = b.x * s,
		.y = b.y * s,
		.z = b.z * s,
	};
}

//--------------------------------------------------------------------------------------------------

Mat2 Mat2::Identity() {
	return Mat2 {
		1.0f, 0.0f,
		0.0f, 1.0f,
	};
}

Mat2 Mat2::Mul(Mat2 a, Mat2 b) {
	return Mat2 {
		(a.m[0][0] * b.m[0][0]) + (a.m[0][1] * b.m[1][0]),
		(a.m[0][0] * b.m[0][1]) + (a.m[0][1] * b.m[1][1]),

		(a.m[1][0] * b.m[0][0]) + (a.m[1][1] * b.m[1][0]),
		(a.m[1][0] * b.m[0][1]) + (a.m[1][1] * b.m[1][1]),
	};
}

//--------------------------------------------------------------------------------------------------

Mat3 Mat3::Identity() {
	return Mat3 {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
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

Vec3 Mat3::Mul(Mat3 a, Vec3 b) {
	return Vec3 {
		.x = a.m[0][0] * b.x + a.m[0][1] * b.y + a.m[0][2] * b.z,
		.y = a.m[1][0] * b.x + a.m[1][1] * b.y + a.m[1][2] * b.z,
		.z = a.m[2][0] * b.x + a.m[2][1] * b.y + a.m[2][2] * b.z,
	};
}

Mat3 Mat3::RotateX(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat3 {
		1.0f, 0.0f, 0.0f,
		0.0f,    c,   -s,
		0.0f,    s,    c,
	};
}

Mat3 Mat3::RotateY(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat3 {
		   c, 0.0f,    s,
		0.0f, 1.0f, 0.0f,
		  -s, 0.0f,    c,
	};
}

Mat3 Mat3::RotateZ(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat3 {
		   c,   -s, 0.0f,
		   s,    c, 0.0f,
		0.0f, 0.0f, 1.0f,
	};
}

Mat3 Mat3::AxisAngle(Vec3 v, f32 a) {
	Vec3 n = Vec3::Normalize(v);
	f32 c = cosf(a);
	Vec3 vc = Vec3::Scale(n, 1.0f - c);
	Vec3 vs = Vec3::Scale(n, sinf(a));

	Mat3 m;
	m.m[0][0] = vc.x * n.x + c;    m.m[1][0] = vc.y * n.x - vs.z; m.m[2][0] = vc.z * n.x + vs.x;
	m.m[0][1] = vc.x * n.y + vs.z; m.m[1][1] = vc.y * n.y + c;    m.m[2][1] = vc.z * n.y - vs.x;
	m.m[0][2] = vc.x * n.z - vs.y; m.m[1][2] = vc.y * n.z + vs.x; m.m[2][2] = vc.z * n.z + c;
	return m;
}

//--------------------------------------------------------------------------------------------------

Mat4 Mat4::Identity() {
	return Mat4 {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 Mat4::Mul(Mat4 a, Mat4 b) {
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

Vec4 Mat4::Mul(Mat4 m, Vec4 b) {
	return Vec4 {
		.x = m.m[0][0] * b.x + m.m[0][1] * b.y + m.m[0][2] * b.z + m.m[0][3] * b.w,
		.y = m.m[1][0] * b.x + m.m[1][1] * b.y + m.m[1][2] * b.z + m.m[1][3] * b.w,
		.z = m.m[2][0] * b.x + m.m[2][1] * b.y + m.m[2][2] * b.z + m.m[2][3] * b.w,
		.w = m.m[3][0] * b.x + m.m[3][1] * b.y + m.m[3][2] * b.z + m.m[3][3] * b.w,
	};
}

Mat4 Mat4::Translate(Vec3 b) {
	//b.x = b.y = b.z = 1.0f;
	return Mat4 {
		//{ 1.0f, 0.0f, 0.0f,  b.x },
		//{ 0.0f, 1.0f, 0.0f,  b.y },
		//{ 0.0f, 0.0f, 1.0f,  b.z },
		//{ 0.0f, 0.0f, 0.0f, 1.0f },

		1.0f, 0.0f, 0.0f,  0.f,
		0.0f, 1.0f, 0.0f,  0.f,
		0.0f, 0.0f, 1.0f,  0.f,
		b.x, b.y, b.z   , 1.0f,
	};
}

Mat4 Mat4::RotateX(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat4 {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f,    c,   -s, 0.0f,
		0.0f,    s,    c, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 Mat4::RotateY(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat4 {
		   c, 0.0f,    s, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		  -s, 0.0f,    c, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 Mat4::RotateZ(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat4 {
		   c,   -s, 0.0f, 0.0f,
		   s,    c, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 Mat4::AxisAngle(Vec3 b, f32 a) {
	Vec3 n = Vec3::Normalize(b);
	f32 c = cosf(a);
	Vec3 vc = Vec3::Scale(n, 1.0f - c);
	Vec3 vs = Vec3::Scale(n, sinf(a));

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

Mat4 Mat4::Look(Vec3 pos, Vec3 x, Vec3 y, Vec3 z) {
	Mat4 m;
	m.m[0][0] =  x.x; m.m[1][0] =  x.y; m.m[2][0] =  x.z; m.m[3][0] = -Vec3::Dot(x, pos);
	m.m[0][1] =  y.x; m.m[1][1] =  y.y; m.m[2][1] =  y.z; m.m[3][1] = -Vec3::Dot(y, pos);
	m.m[0][2] =  z.x; m.m[1][2] =  z.y; m.m[2][2] =  z.z; m.m[3][2] = -Vec3::Dot(z, pos);
	m.m[0][3] = 0.0f; m.m[1][3] = 0.0f; m.m[2][3] = 0.0f; m.m[3][3] = 1.0f;
	return m;
}

/*
Mat4 LookAt(Vec3 eye, Vec3 at, Vec3 up) {
	Vec3 z = Vec3::Normalize(Vec3::Sub(eye, at));
	Vec3 x = Vec3::Normalize(Vec3::Cross(up, z));
	Vec3 y = Vec3::Cross(z, x);
	Mat4 m;
	m.m[0][0] =  x.x; m.m[1][0] =  x.y; m.m[2][0] =  x.z; m.m[3][0] = -Vec3::Dot(x, eye);
	m.m[0][1] =  y.x; m.m[1][1] =  y.y; m.m[2][1] =  y.z; m.m[3][1] = -Vec3::Dot(y, eye);
	m.m[0][2] =  z.x; m.m[1][2] =  z.y; m.m[2][2] =  z.z; m.m[3][2] = -Vec3::Dot(z, eye);
	m.m[0][3] = 0.0f; m.m[1][3] = 0.0f; m.m[2][3] = 0.0f; m.m[3][3] =  1.0f;
	return m;
}
*/

Mat4 Mat4::Perspective(f32 fovy, f32 aspect, f32 zn, f32 zf) {
	const f32 ht = tanf(fovy / 2.0f);
	Mat4 m;
	m.m[0][0] = 1.0f / (aspect * ht); m.m[1][0] = 0.0f;       m.m[2][0] = 0.0f;           m.m[3][0] = 0.0f;
	m.m[0][1] = 0.0f;                 m.m[1][1] = -1.0f / ht; m.m[2][1] = 0.0f;           m.m[3][1] = 0.0f;
	m.m[0][2] = 0.0f;                 m.m[1][2] = 0.0f;       m.m[2][2] = zf / (zn - zf); m.m[3][2] = -(zf * zn) / (zf - zn);
	m.m[0][3] = 0.0f;                 m.m[1][3] = 0.0f;       m.m[2][3] = -1.0f;          m.m[3][3] = 0.0f;
	return m;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC