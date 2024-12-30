#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "sprite.common.glsl"

layout (location = 0) in vec2 uvIn;
layout (location = 1) in vec3 worldPosIn;
layout (location = 2) flat in vec3 lightPosIn;
layout (location = 3) flat in uint diffuseIdxIn;
layout (location = 4) flat in uint normalIdxIn;

layout (location = 0) out vec4 rgbaOut;

void main() {
	vec3 texNormal  = texture(
		nonuniformEXT(sampler2D(bindlessTextures[normalIdxIn], bindlessSamplers[SamplerId_Nearest])),
		uvIn
	).rgb;
	vec3 texNormalC = texNormal * 2.0f - 1.0f;
	vec3 texNormalN = normalize(texNormalC);
    vec4 texColor = texture(
		nonuniformEXT(sampler2D(bindlessTextures[diffuseIdxIn], bindlessSamplers[SamplerId_Nearest])),
		uvIn
	);
	vec3 lightVec = lightPosIn - worldPosIn;
	float lightDist = length(lightVec);
	vec3 lightDir = normalize(lightVec);
	float d = min(1.0f, dot(lightDir, texNormalN) / (0.001 * lightDist * lightDist));

	float diffuse = max(d, 0.0f);
	rgbaOut = vec4(0.01f, 0.01f, 0.01f, 0.0f) + diffuse * texColor;
}