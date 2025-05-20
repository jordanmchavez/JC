#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct OrthoCamera {
	Vec2 pos        = {};
	F32  halfWidth  = 0.0f;
	F32  halfHeight = 0.0f;

	void SetFovHeight(F32 fov, F32 aspect, F32 z);
	void SetBounds(float l, float r, float b, float t);
	Mat4 GetProjView();
};

//--------------------------------------------------------------------------------------------------

struct PerspectiveCamera {
	Vec3 pos    = {};
	Vec3 x      = { -1.0f, 0.0f,  0.0f };
	Vec3 y      = {  0.0f, 1.0f,  0.0f };
	Vec3 z      = {  0.0f, 0.0f, -1.0f };
	F32  yaw    = 0.0f;
	F32  pitch  = 0.0f;
	F32  fov    = 0.0f;
	F32  aspect = 0.0f;

	void Forward(F32 delta);
	void Left(F32 delta);
	void Up(F32 delta);
	void Yaw(F32 delta);
	void Pitch(F32 delta);
	Mat4 GetProjView();
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC