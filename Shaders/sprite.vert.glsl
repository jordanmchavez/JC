#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "sprite.common.glsl"

layout (location = 0) out vec2 uvOut;
layout (location = 1) flat out uint textureIdxOut;

vec2 spriteXYs[6] = vec2[6](
	vec2(-0.5f,  0.5f),
	vec2( 0.5f,  0.5f),
	vec2( 0.5f, -0.5f),
	vec2( 0.5f, -0.5f),
	vec2(-0.5f, -0.5f),
	vec2(-0.5f,  0.5f)
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

	Sprite sprite = pushConstants.spriteBuffer.sprites[gl_InstanceIndex];
	uint idx = gl_VertexIndex % 6;

	vec4 pos = sprite.model * vec4(spriteXYs[idx], 0.0f, 1.0f);
	gl_Position = pushConstants.projView * pos;
	
	vec2 t = spriteUVs[idx];
	uvOut = vec2(
		sprite.uv1.x * (1.0f - t.x) + sprite.uv2.x * t.x,
		sprite.uv1.y * (1.0f - t.y) + sprite.uv2.y * t.y
	);

	textureIdxOut = sprite.textureIdx;
}