#pragma once

#include <math.h>

#include "JC/Common.h"

inline float Math_Radians(float deg) {
	constexpr float PI = 3.14159265358979323846f;
	return deg * PI / 180.0f;
}

inline Vec3 Vec3_Make(float x, float y, float z) {
	return {
		.x = x,
		.y = y,
		.z = z,
	};
}

inline Vec3 Vec3_Scale(Vec3 v, float s) {
	return {
		.x = v.x * s,
		.y = v.y * s,
		.z = v.z * s,
	};
}

inline Vec3 Vec3_Add(Vec3 u, Vec3 v) {
	return {
		.x = u.x + v.x,
		.y = u.y + v.y,
		.z = u.z + v.z,
	};
}

inline Vec3 Vec3_Sub(Vec3 u, Vec3 v) {
	return {
		.x = u.x - v.x,
		.y = u.y - v.y,
		.z = u.z - v.z,
	};
}

inline float Vec3_Dot(Vec3 u, Vec3 v) {
	return (u.x * v.x) + (u.y * v.y) + (u.z * v.z);
}

inline Vec3 Vec3_Cross(Vec3 u, Vec3 v) {
	return {
		.x = u.y * v.z - u.z * v.y,
		.y = u.z * v.x - u.x * v.z,
		.z = u.x * v.y - u.y * v.x,
	};
}

inline Vec3 Vec3_Normalize(Vec3 v) {
	const float s = 1.0f / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	return {
		.x = v.x * s,
		.y = v.y * s,
		.z = v.z * s,
	};
}

inline Mat4 Mat4_AngleAxis(float a, Vec3 v) {
	Vec3 n = Vec3_Normalize(v);
	float c = cosf(a);
	Vec3 vc = Vec3_Scale(n, 1.0f - c);
	Vec3 vs = Vec3_Scale(n, sinf(a));

	Mat4 m;
	m.m[0][0] = vc.x * n.x + c;    m.m[1][0] = vc.y * n.x - vs.z; m.m[2][0] = vc.z * n.x + vs.x; m.m[3][0] = 0.0f;
	m.m[0][1] = vc.x * n.y + vs.z; m.m[1][1] = vc.y * n.y + c;    m.m[2][1] = vc.z * n.y - vs.x; m.m[3][1] = 0.0f;
	m.m[0][2] = vc.x * n.z - vs.y; m.m[1][2] = vc.y * n.z + vs.x; m.m[2][2] = vc.z * n.z + c;    m.m[3][2] = 0.0f;
	m.m[0][3] =              0.0f; m.m[1][3] =              0.0f; m.m[2][3] =              0.0f; m.m[3][3] = 1.0f;
	return m;
}

inline Mat4 Mat4_LookAt(Vec3 eye, Vec3 at, Vec3 up) {
	Vec3 z = Vec3_Normalize(Vec3_Sub(at, eye));
	Vec3 x = Vec3_Normalize(Vec3_Cross(z, up));
	Vec3 y = Vec3_Cross(x, z);
	Mat4 m;
	m.m[0][0] =  x.x; m.m[1][0] =  x.y; m.m[2][0] =  x.z; m.m[3][0] = -Vec3_Dot(y, eye);
	m.m[0][1] =  y.x; m.m[1][1] =  y.y; m.m[2][1] =  y.z; m.m[3][1] = -Vec3_Dot(x, eye);
	m.m[0][2] = -z.x; m.m[1][2] = -z.y; m.m[2][2] = -z.z; m.m[3][2] =  Vec3_Dot(z, eye);
	m.m[0][3] = 0.0f; m.m[1][3] = 0.0f; m.m[2][3] = 0.0f; m.m[3][3] =  1.0f;
	return m;
}

inline Mat4 Mat4_Perspective(float fovy, float aspect, float zn, float zf) {
	const float ht = tanf(fovy / 2.0f);
	Mat4 m;
	m.m[0][0] = 1.0f / (aspect * ht); m.m[1][0] = 0.0f;       m.m[2][0] = 0.0f;           m.m[3][0] = 0.0f;
	m.m[0][1] = 0.0f;                 m.m[1][1] = -1.0f / ht; m.m[2][1] = 0.0f;           m.m[3][1] = 0.0f;
	m.m[0][2] = 0.0f;                 m.m[1][2] = 0.0f;       m.m[2][2] = zf / (zn - zf); m.m[3][2] = -(zf * zn) / (zf - zn);
	m.m[0][3] = 0.0f;                 m.m[1][3] = 0.0f;       m.m[2][3] = -1.0f;          m.m[3][3] = 0.0f;
	return m;
}