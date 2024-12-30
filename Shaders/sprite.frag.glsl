#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "sprite.common.glsl"

layout (location = 0) in vec2 uvIn;
layout (location = 1) in vec3 lightDirIn;
layout (location = 2) flat in uint diffuseIdxIn;
layout (location = 3) flat in uint normalIdxIn;

layout (location = 0) out vec4 rgbaOut;

void main() {
	normal = texture(
		nonUniformEXT(sampler2D(bindlessTextures[normalIdxIn], bindlessSamplers[SamplerId_Nearest])),
		uvIn
	);
    tcolor = texture(
		nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[SamplerId_Nearest])),
		uvIn
	);
	float diffuse = max(dot(lightDir, normal), 0.0f);
	rgbaOut = diffuse * tcolor;
}