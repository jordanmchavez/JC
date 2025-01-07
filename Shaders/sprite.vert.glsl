#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "sprite.common.glsl"

layout (location = 0)      out vec2 uvOut;
layout (location = 1)      out vec3 worldPosOut;
layout (location = 2) flat out uint diffuseIdxOut;
layout (location = 3) flat out uint normalIdxOut;

vec2 spriteXYs[6] = vec2[6](
	vec2( 0.0f, 16.0f),
	vec2( 16.0f, 16.0f),
	vec2( 16.0f, 0.0f),
	vec2( 16.0f, 0.0f),
	vec2( 0.0f, 0.0f),
	vec2( 0.0f, 16.0f)
);

vec2 spriteUVs[6] = vec2[6](
	vec2(0.0f, 0.0f),
	vec2(1.0f, 0.0f),
	vec2(1.0f, 1.0f),
	vec2(1.0f, 1.0f),
	vec2(0.0f, 1.0f),
	vec2(0.0f, 0.0f)
);

void main() {
	SpriteDrawCmd spriteDrawCmd = pushConstants.sceneBuffer.spriteDrawCmdBuffer.spriteDrawCmds[gl_InstanceIndex];
	uint idx = gl_VertexIndex % 6;

	vec4 worldPos = spriteDrawCmd.model * vec4(spriteXYs[idx], 0.0f, 1.0f);
	gl_Position = pushConstants.sceneBuffer.projView * worldPos;
	
	vec2 t = spriteUVs[idx];
	uvOut = vec2(
		spriteDrawCmd.uv1.x * (1.0f - t.x) + spriteDrawCmd.uv2.x * t.x,
		spriteDrawCmd.uv1.y * (1.0f - t.y) + spriteDrawCmd.uv2.y * t.y
	);

	mat3 model3 = mat3(spriteDrawCmd.model);

	vec3 tangent   = normalize(model3 * vec3(1.0f, 0.0f, 0.0f));
	vec3 bitangent = normalize(model3 * vec3(0.0f, 1.0f, 0.0f));
	vec3 normal    = normalize(model3 * vec3(0.0f, 0.0f, 1.0f));
	mat3 tbn       = transpose(mat3(tangent, bitangent, normal));

	worldPosOut   = worldPos.rgb;
	diffuseIdxOut = spriteDrawCmd.diffuseIdx;
	normalIdxOut  = spriteDrawCmd.normalIdx;
}