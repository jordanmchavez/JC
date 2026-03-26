#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "SpriteCommon.glsl"

layout (location = 0)      in  vec2  uvIn;
layout (location = 1) flat in  vec4  colorIn;
layout (location = 2) flat in  uint  textureIdxIn;
layout (location = 3) flat in  vec2  uv1In;
layout (location = 4) flat in  vec2  uv2In;
layout (location = 0)      out vec4  colorOut;

void main() {
	uint samplerId = SamplerId_Nearest;

	if (textureIdxIn != 0) {
		colorOut = texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[samplerId])), uvIn);
	} else {
		colorOut = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	colorOut *= colorIn;

	if (colorOut.a < 0.01) discard;
}