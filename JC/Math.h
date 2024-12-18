#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr f32 DegToRad(f32 deg) {
	constexpr f32 PI = 3.14159265358979323846f;
	return deg * PI / 180.0f;
}

//--------------------------------------------------------------------------------------------------

struct Vec2 {
	f32 x = 0.0f;
	f32 y = 0.0f;
};

struct Vec3 {
	f32 x = 0.0f;
	f32 y = 0.0f;
	f32 z = 0.0f;

	static Vec3  Add(Vec3 u, Vec3 v);
	static Vec3  AddScaled(Vec3 u, Vec3 v, f32 s);
	static Vec3  Sub(Vec3 u, Vec3 v);
	static Vec3  Scale(Vec3 v, f32 s);
	static f32   Dot(Vec3 u, Vec3 v);
	static Vec3  Cross(Vec3 u, Vec3 v);
	static Vec3  Normalize(Vec3 v);
};

struct Vec4 {
	f32 x = 0.0f;
	f32 y = 0.0f;
	f32 z = 0.0f;
	f32 w = 0.0f;
};

struct Mat3 {
	f32 m[3][3];

	static Mat3 Identity();
	static Mat3 Mul(Mat3 m1, Mat3 m2);
	static Vec3 Mul(Mat3 m, Vec3 v);
	static Mat3 RotateX(f32 a);
	static Mat3 RotateY(f32 a);
	static Mat3 RotateZ(f32 a);
	static Mat3 AxisAngle(Vec3 v, f32 a);
};

struct Mat4 {
	f32 m[4][4];

	static Mat4 Identity();
	static Mat4 Mul(Mat4 m1, Mat4 m2);
	static Vec4 Mul(Mat4 m, Vec4 v);
	static Mat4 AngleAxis(f32 a, Vec3 v);
	static Mat4 Look(Vec3 pos, Vec3 x, Vec3 y, Vec3 z);
	static Mat4 Perspective(f32 fovy, f32 aspect, f32 zn, f32 zf);
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC