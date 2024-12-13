#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr float DegToRad(float deg) {
	constexpr float PI = 3.14159265358979323846f;
	return deg * PI / 180.0f;
}

//--------------------------------------------------------------------------------------------------

struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;
};

struct Vec3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	static Vec3  Add(Vec3 u, Vec3 v);
	static Vec3  AddScaled(Vec3 u, Vec3 v, float s);
	static Vec3  Sub(Vec3 u, Vec3 v);
	static Vec3  Scale(Vec3 v, float s);
	static float Dot(Vec3 u, Vec3 v);
	static Vec3  Cross(Vec3 u, Vec3 v);
	static Vec3  Normalize(Vec3 v);
};

struct Vec4 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;
};

struct Mat4 {
	float m[4][4];

	static Mat4 Identity();
	static Mat4 Mul(Mat4 m1, Mat4 m2);
	static Vec4 Mul(Mat4 m, Vec4 v);
	static Vec3 Mul(Mat4 m, Vec3 v);
	static Mat4 RotateY(float a);
	static Mat4 AngleAxis(float a, Vec3 v);
	static Mat4 LookAt(Vec3 eye, Vec3 at, Vec3 up);
	static Mat4 Perspective(float fovy, float aspect, float zn, float zf);
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC