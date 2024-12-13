#include "JC/Math.h"

#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

Vec3 Vec3::Add(Vec3 u, Vec3 v) {
	return {
		.x = u.x + v.x,
		.y = u.y + v.y,
		.z = u.y + v.z,
	};
}

Vec3 Vec3::Sub(Vec3 u, Vec3 v) {
	return {
		.x = u.x - v.x,
		.y = u.y - v.y,
		.z = u.y - v.z,
	};
}

Vec3 Vec3::Scale(Vec3 v, float s) {
	return {
		.x = v.x * s,
		.y = v.y * s,
		.z = v.z * s,
	};
}

float Vec3::Dot(Vec3 u, Vec3 v) {
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
	const float s = 1.0f / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	return {
		.x = v.x * s,
		.y = v.y * s,
		.z = v.z * s,
	};
}

Mat4 Mat4::AngleAxis(float a, Vec3 v) {
	Vec3 n = Vec3::Normalize(v);
	float c = cosf(a);
	Vec3 vc = Vec3::Scale(n, 1.0f - c);
	Vec3 vs = Vec3::Scale(n, sinf(a));

	Mat4 m;
	m.m[0][0] = vc.x * n.x + c;    m.m[1][0] = vc.y * n.x - vs.z; m.m[2][0] = vc.z * n.x + vs.x; m.m[3][0] = 0.0f;
	m.m[0][1] = vc.x * n.y + vs.z; m.m[1][1] = vc.y * n.y + c;    m.m[2][1] = vc.z * n.y - vs.x; m.m[3][1] = 0.0f;
	m.m[0][2] = vc.x * n.z - vs.y; m.m[1][2] = vc.y * n.z + vs.x; m.m[2][2] = vc.z * n.z + c;    m.m[3][2] = 0.0f;
	m.m[0][3] =              0.0f; m.m[1][3] =              0.0f; m.m[2][3] =              0.0f; m.m[3][3] = 1.0f;
	return m;
}

Mat4 Mat4::LookAt(Vec3 eye, Vec3 at, Vec3 up) {
	Vec3 z = Vec3::Normalize(Vec3::Sub(at, eye));
	Vec3 x = Vec3::Normalize(Vec3::Cross(z, up));
	Vec3 y = Vec3::Cross(x, z);
	Mat4 m;
	m.m[0][0] =  x.x; m.m[1][0] =  x.y; m.m[2][0] =  x.z; m.m[3][0] = -Vec3::Dot(y, eye);
	m.m[0][1] =  y.x; m.m[1][1] =  y.y; m.m[2][1] =  y.z; m.m[3][1] = -Vec3::Dot(x, eye);
	m.m[0][2] = -z.x; m.m[1][2] = -z.y; m.m[2][2] = -z.z; m.m[3][2] =  Vec3::Dot(z, eye);
	m.m[0][3] = 0.0f; m.m[1][3] = 0.0f; m.m[2][3] = 0.0f; m.m[3][3] =  1.0f;
	return m;
}

Mat4 _Perspective(float fovy, float aspect, float zn, float zf) {
	const float ht = tanf(fovy / 2.0f);
	Mat4 m;
	m.m[0][0] = 1.0f / (aspect * ht); m.m[1][0] = 0.0f;       m.m[2][0] = 0.0f;           m.m[3][0] = 0.0f;
	m.m[0][1] = 0.0f;                 m.m[1][1] = -1.0f / ht; m.m[2][1] = 0.0f;           m.m[3][1] = 0.0f;
	m.m[0][2] = 0.0f;                 m.m[1][2] = 0.0f;       m.m[2][2] = zf / (zn - zf); m.m[3][2] = -(zf * zn) / (zf - zn);
	m.m[0][3] = 0.0f;                 m.m[1][3] = 0.0f;       m.m[2][3] = -1.0f;          m.m[3][3] = 0.0f;
	return m;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC