#include "JC/Math.h"

#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

Vec3 Vec3::Add(Vec3 u, Vec3 v) {
	return {
		.x = u.x + v.x,
		.y = u.y + v.y,
		.z = u.z + v.z,
	};
}

Vec3 Vec3::AddScaled(Vec3 u, Vec3 v, f32 s) {
	return {
		.x = u.x + v.x * s,
		.y = u.y + v.y * s,
		.z = u.z + v.z * s,
	};
}

Vec3 Vec3::Sub(Vec3 u, Vec3 v) {
	return {
		.x = u.x - v.x,
		.y = u.y - v.y,
		.z = u.z - v.z,
	};
}

Vec3 Vec3::Scale(Vec3 v, f32 s) {
	return {
		.x = v.x * s,
		.y = v.y * s,
		.z = v.z * s,
	};
}

f32 Vec3::Dot(Vec3 u, Vec3 v) {
	return (u.x * v.x) + (u.y * v.y) + (u.z * v.z);
}

Vec3 Vec3::Cross(Vec3 u, Vec3 v) {
	return {
		.x = u.y * v.z - u.z * v.y,
		.y = u.z * v.x - u.x * v.z,
		.z = u.x * v.y - u.y * v.x,
	};
}

Vec3 Vec3::Normalize(Vec3 v) {
	const f32 s = 1.0f / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	return {
		.x = v.x * s,
		.y = v.y * s,
		.z = v.z * s,
	};
}

//--------------------------------------------------------------------------------------------------

Mat3 Mat3::Identity() {
	return Mat3 {
		.m = {
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
		},
	};
}

Mat3 Mul(Mat3 m1, Mat3 m2) {
	Mat3 r;
	r.m[0][0] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0];
	r.m[0][1] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0];
	r.m[0][2] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0];
	r.m[0][3] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0];
	r.m[1][0] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1];
	r.m[1][1] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1];
	r.m[1][2] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1];
	r.m[1][3] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1];
	r.m[2][0] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2];
	r.m[2][1] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2];
	r.m[2][2] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2];
	r.m[2][3] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2];
	r.m[3][0] = m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][0];
	return r;
};

Vec3 Mat3::Mul(Mat3 m, Vec3 v) {
	return Vec3 {
		.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z,
		.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z,
		.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z,
	};
}

Mat3 Mat3::RotateX(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat3 { .m = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f,    c,   -s },
		{ 0.0f,    s,    c },
	} };
}

Mat3 Mat3::RotateY(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat3 { .m = {
		{    c, 0.0f,    s },
		{ 0.0f, 1.0f, 0.0f },
		{   -s, 0.0f,    c },
	} };
}

Mat3 Mat3::RotateZ(f32 a) {
	const f32 s = sinf(a);
	const f32 c = cosf(a);
	return Mat3 { .m = {
		{    c,   -s, 0.0f },
		{    s,    c, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
	} };
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
		.m = {
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
		},
	};
}

Mat4 Mul(Mat4 m1, Mat4 m2) {
	Mat4 r;
	r.m[0][0] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0] + m1.m[0][3] * m2.m[3][0];
	r.m[0][1] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0] + m1.m[0][3] * m2.m[3][0];
	r.m[0][2] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0] + m1.m[0][3] * m2.m[3][0];
	r.m[0][3] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0] + m1.m[0][3] * m2.m[3][0];
	r.m[1][0] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1] + m1.m[1][3] * m2.m[3][1];
	r.m[1][1] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1] + m1.m[1][3] * m2.m[3][1];
	r.m[1][2] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1] + m1.m[1][3] * m2.m[3][1];
	r.m[1][3] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1] + m1.m[1][3] * m2.m[3][1];
	r.m[2][0] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2] + m1.m[2][3] * m2.m[3][2];
	r.m[2][1] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2] + m1.m[2][3] * m2.m[3][2];
	r.m[2][2] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2] + m1.m[2][3] * m2.m[3][2];
	r.m[2][3] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2] + m1.m[2][3] * m2.m[3][2];
	r.m[3][0] = m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][0] + m1.m[3][3] * m2.m[3][3];
	r.m[3][1] = m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][0] + m1.m[3][3] * m2.m[3][3];
	r.m[3][2] = m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][0] + m1.m[3][3] * m2.m[3][3];
	r.m[3][3] = m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][0] + m1.m[3][3] * m2.m[3][3];
	return r;
};

Vec4 Mat4::Mul(Mat4 m, Vec4 v) {
	return Vec4 {
		.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3] * v.w,
		.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3] * v.w,
		.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3] * v.w,
		.w = m.m[3][0] * v.x + m.m[3][1] * v.y + m.m[3][2] * v.z + m.m[3][3] * v.w,
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