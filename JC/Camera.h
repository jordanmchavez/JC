#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct OrthoCamera {
	Vec3 pos        = {};
	F32  halfWidth  = 0.0f;
	F32  halfHeight = 0.0f;

	void Set(F32 fov, F32 aspect, F32 z);
	Mat4 GetProjView();
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC