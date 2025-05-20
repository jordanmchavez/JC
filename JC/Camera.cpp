#include "JC/Camera.h"
#include "JC/Math.h"
#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

void OrthoCamera::SetFovHeight(F32 fov, F32 aspect, F32 z) {
	halfWidth  = z * tanf(fov / 2.0f);
	halfHeight = halfWidth / aspect;
}

Mat4 OrthoCamera::GetProjView() {
	return Math::Ortho(
		pos.x - halfWidth,
		pos.x + halfWidth,
		pos.y - halfHeight,
		pos.y + halfHeight,
		10.0f,
		-10.0f
	);
}

//--------------------------------------------------------------------------------------------------

void PerspectiveCamera::Forward(F32 delta) { pos = Math::AddScaled(pos, z, delta); }
void PerspectiveCamera::Left   (F32 delta) { pos = Math::AddScaled(pos, x, delta); }
void PerspectiveCamera::Up     (F32 delta) { pos = Math::AddScaled(pos, y, delta); }

void PerspectiveCamera::Yaw(F32 delta) {
	yaw += delta;
	z = Math::Mul(Math::RotationYMat3(yaw), Vec3 { 0.0f, 0.0f, -1.0f });
	x = Math::Cross(Vec3 { 0.0f, 1.0f, 0.0f }, z);
}

void PerspectiveCamera::Pitch(F32 delta) {
	pitch += delta;
	z = Math::Mul(Math::AxisAngleMat3(x, pitch), z);
	y = Math::Cross(z, x);
}

Mat4 PerspectiveCamera::GetProjView() {
	return Math::Mul(
		Math::Look(pos, x, y, z),
		Math::Perspective(Math::DegToRad(fov), aspect, 0.01f, 100000000.0f)
	);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC