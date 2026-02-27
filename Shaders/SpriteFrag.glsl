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
	uint samplerId = SamplerId_Nearest;
	//uint samplerId = SamplerId_Linear;

	vec4 tempColorOut;
	if (textureIdxIn != 0) {
		tempColorOut = texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), uvIn);
	} else {
		tempColorOut = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	if (outlineWidthIn > 0.0 && tempColorOut.a < 0.5) {
		vec2 texelStep = 1.0 / vec2(textureSize(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId]), 0));

		float maxNeighborAlpha = 0.0;
		vec2 neighborUV;
		neighborUV = clamp(uvIn + texelStep * vec2(-1, 0), uv1In, uv2In);
		maxNeighborAlpha = max(maxNeighborAlpha, texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), neighborUV).a);
		neighborUV = clamp(uvIn + texelStep * vec2(1, 0), uv1In, uv2In);
		maxNeighborAlpha = max(maxNeighborAlpha, texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), neighborUV).a);
		neighborUV = clamp(uvIn + texelStep * vec2(0, -1), uv1In, uv2In);
		maxNeighborAlpha = max(maxNeighborAlpha, texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), neighborUV).a);
		neighborUV = clamp(uvIn + texelStep * vec2(0, 1), uv1In, uv2In);
		maxNeighborAlpha = max(maxNeighborAlpha, texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), neighborUV).a);

		if (maxNeighborAlpha > 0.0) {
			tempColorOut = outlineColorIn;
		}
	} else {
		tempColorOut *= colorIn;
	}

	if (tempColorOut.a < 0.01) discard;
	colorOut = tempColorOut;
}