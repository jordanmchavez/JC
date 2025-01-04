#include "JC/Common.h"

#include "JC/Array.h"
#include "JC/Math.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct SpriteDrawCmd {
	Vec2 position;
	Vec2 uv1;

	Vec2 uv2;
	u32 diffuseIdx;
	u32 normalIdx;

	float scale;
	specular?
	emissive?
	roughness?
};

3 float4s = 12 dwords = 48 bytes

6 float4s = 24 dwords = 96 bytes

sprite pipeline
mesh pipeline
ui pipeline

make a ramp for each color hue section




//--------------------------------------------------------------------------------------------------

}	// namespace JC