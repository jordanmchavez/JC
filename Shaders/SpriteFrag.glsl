#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "SpriteCommon.glsl"

layout (location = 0)      in  vec2 uvIn;
layout (location = 1) flat in  uint textureIdxIn;
layout (location = 0)      out vec4 rgbaOut;

void main() {
    rgbaOut = texture(
		nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[SamplerId_Nearest])),
		uvIn
	);
}