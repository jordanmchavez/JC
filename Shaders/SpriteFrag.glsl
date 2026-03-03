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

	if (textureIdxIn != 0) {
		colorOut = texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), uvIn);
	} else {
		colorOut = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	if (outlineWidthIn > 0.0 && colorOut.a < 0.5) {
		vec2 texelStep = 1.0 / vec2(textureSize(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId]), 0));
		vec2 halfTexelStep = texelStep * 0.5;

		vec2 tl = max(uvIn - texelStep, uv1In + halfTexelStep);
		vec2 br = min(uvIn + texelStep, uv2In - halfTexelStep);

		float maxNeighborAlpha = 0.0;
		maxNeighborAlpha = max(maxNeighborAlpha, texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), vec2(tl.x,   uvIn.y)).a);
		maxNeighborAlpha = max(maxNeighborAlpha, texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), vec2(uvIn.x, tl.y  )).a);
		maxNeighborAlpha = max(maxNeighborAlpha, texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), vec2(br.x,   uvIn.y)).a);
		maxNeighborAlpha = max(maxNeighborAlpha, texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), vec2(uvIn.x, br.y  )).a);

		if (maxNeighborAlpha > 0.0) {
			colorOut = outlineColorIn;
		}
	} else {
		colorOut *= colorIn;
	}

	if (colorOut.a < 0.01) discard;
}