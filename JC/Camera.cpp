#include "JC/Camera.h"
#include "JC/Math.h"
#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

void OrthoCamera::Set(F32 fov, F32 aspect, F32 z) {
	pos.z      = z;
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

}	// namespace JC