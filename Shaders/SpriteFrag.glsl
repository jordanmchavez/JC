#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "SpriteCommon.glsl"

layout (location = 0)      in  vec2  uvIn;
layout (location = 1) flat in  vec4  colorIn;
layout (location = 2) flat in  uint  textureIdxIn;
layout (location = 3) flat in  vec4  outlineColorIn;
layout (location = 4) flat in  float outlineWidthIn;
layout (location = 5) flat in  vec2  uv1In;
layout (location = 6) flat in  vec2  uv2In;
layout (location = 0)      out vec4  colorOut;

void main() {
	vec4 tempColorOut;
	if (textureIdxIn != 0) {
		tempColorOut = texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[SamplerId_Nearest])), uvIn);
	} else {
		tempColorOut = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	tempColorOut *= colorIn;

	//if (outlineWidthIn > 0.0 && tempColorOut.a < 0.5) {
	//if (tempColorOut.a < 1.0) {
		vec2 uvDx = dFdx(uvIn);
		vec2 uvDy = dFdy(uvIn);
		float maxNeighborAlpha = 0.0;
		for (int nx = -1; nx <= 1; nx += 2) {
			for (int ny = -1; ny <= 1; ny += 2) {
				vec2 neighborUV = clamp(uvIn + uvDx * float(nx) + uvDy * float(ny), uv1In, uv2In);
				float a = texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[SamplerId_Nearest])), neighborUV).a;
				maxNeighborAlpha = max(maxNeighborAlpha, a);
			}
		}
		if (maxNeighborAlpha > 0.0) {
			tempColorOut = outlineColorIn;
		}
	//}

	colorOut = tempColorOut;
}