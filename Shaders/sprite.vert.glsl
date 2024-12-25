#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "sprite.common.glsl"

layout (location = 0) out vec2 uvOut;
layout (location = 1) flat out uint textureIdOut;

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
	uint idx = gl_InstanceIndex % 6;

	vec2 pos = spriteXYs[idx];
	pos = sprite.model * pos;
	gl_Position = pushConstants.viewProj * vec4(pos, 0.0f, 1.0f);
	
	vec2 t = spriteUVs[idx];
	uvOut = vec2(
		sprite.uv1.x * (1.0f - t.x) + sprite.uv2.x * t.x,
		sprite.uv1.y * (1.0f - t.y) + sprite.uv2.y * t.y
	);
	textureIdOut = sprite.textureId;
}