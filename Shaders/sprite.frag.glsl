#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "sprite.common.glsl"

layout (location = 0) in vec3 posIn;
layout (location = 0) in vec3 uvIn;
layout (location = 0) in vec3 uvIn;
layout (location = 0) in vec3 uvIn;
layout (location = 0) in vec2 uvIn;
layout (location = 1) flat in uint diffuseIdxIn;
layout (location = 2) flat in uint normalIdxIn;

layout (location = 0) out vec4 rgbaOut;

void main() {
	normal = texture(
		nonUniformEXT(sampler2D(bindlessTextures[normalIdxIn], bindlessSamplers[SamplerId_Nearest])),
		uvIn
	);
    rgbaOut = texture(
		nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[SamplerId_Nearest])),
		uvIn
	);
}