#pragma once

#include "JC/Core.h"

namespace JC::Math {

//--------------------------------------------------------------------------------------------------

constexpr F32 DegToRad(F32 deg) {
	constexpr F32 PI = 3.14159265358979323846f;
	return deg * PI / 180.0f;
}

//--------------------------------------------------------------------------------------------------

Vec2 Add(Vec2 u, Vec2 v);
Vec3 Add(Vec3 u, Vec3 v);
Vec2 AddScaled(Vec2 u, Vec2 v, F32 s);
Vec3 AddScaled(Vec3 u, Vec3 v, F32 s);
Mat3 AxisAngleMat3(Vec3 v, F32 a);
Mat4 AxisAngleMat4(Vec3 v, F32 a);
F32  Dot(Vec3 u, Vec3 v);
Vec3 Cross(Vec3 u, Vec3 v);
Mat2 IdentityMat2();
Mat3 IdentityMat3();
Mat4 IdentityMat4();
F32  Lerp(F32 a, F32 b, F32 t);
Vec4 Lerp(Vec4 a, Vec4 b, F32 t);
Mat4 Look(Vec3 pos, Vec3 x, Vec3 y, Vec3 z);
Vec2 Mul(Vec2 v, F32 s);
Vec2 Mul(Vec2 a, Vec2 b);
Vec3 Mul(Vec3 v, F32 s);
Mat2 Mul(Mat2 a, Mat2 b);
Vec3 Mul(Mat3 m, Vec3 v);
Mat3 Mul(Mat3 a, Mat3 b);
Vec4 Mul(Mat4 m, Vec4 v);
Mat4 Mul(Mat4 a, Mat4 b);
Vec3 Normalize(Vec3 v);
Mat4 Ortho(F32 l, F32 r, F32 b, F32 t, F32 n, F32 f);
Mat4 Perspective(F32 fovy, F32 aspect, F32 zn, F32 zf);
Mat3 RotationXMat3(F32 a);
Mat3 RotationYMat3(F32 a);
Mat3 RotationZMat3(F32 a);
Mat4 RotationXMat4(F32 a);
Mat4 RotationYMat4(F32 a);
Mat4 RotationZMat4(F32 a);
Vec3 Sub(Vec3 u, Vec3 v);
Mat4 TranslationMat4(Vec3 v);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Math