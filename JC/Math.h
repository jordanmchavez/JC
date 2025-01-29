#pragma once

#include "JC/Core.h"

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

struct Mat2 {
	f32 m[2][2];

	static Mat2 Identity();
	static Mat2 Mul(Mat2 a, Mat2 b);
};

struct Mat3 {
	f32 m[3][3];

	static Mat3 Identity();
	static Mat3 Mul(Mat3 a, Mat3 b);
	static Vec3 Mul(Mat3 m, Vec3 v);
	static Mat3 RotateX(f32 a);
	static Mat3 RotateY(f32 a);
	static Mat3 RotateZ(f32 a);
	static Mat3 AxisAngle(Vec3 v, f32 a);
};

struct Mat4 {
	f32 m[4][4];

	static Mat4 Identity();
	static Mat4 Mul(Mat4 a, Mat4 b);
	static Vec4 Mul(Mat4 m, Vec4 v);
	static Mat4 Translate(Vec3 v);
	static Mat4 RotateX(f32 a);
	static Mat4 RotateY(f32 a);
	static Mat4 RotateZ(f32 a);
	static Mat4 AxisAngle(Vec3 v, f32 a);
	static Mat4 Look(Vec3 pos, Vec3 x, Vec3 y, Vec3 z);
	static Mat4 Perspective(f32 fovy, f32 aspect, f32 zn, f32 zf);
	static Mat4 Ortho(float l, float r, float b, float t, float n, float f);
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC