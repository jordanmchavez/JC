#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "SpriteCommon.glsl"

layout (location = 0)      out vec2 uvOut;
layout (location = 1)      out vec4 colorOut;
layout (location = 2) flat out uint textureIdxOut;

vec2 spriteXYs[6] = vec2[6](
	// vec2(0.0f, 1.0f),
	// vec2(1.0f, 1.0f),
	// vec2(1.0f, 0.0f),
	// vec2(1.0f, 0.0f),
	// vec2(0.0f, 0.0f),
	// vec2(0.0f, 1.0f)

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
	SpriteDrawCmd spriteDrawCmd = pushConstants.sceneBuffer.spriteDrawCmdBuffer.spriteDrawCmds[gl_InstanceIndex];
	uint idx = gl_VertexIndex % 6;

	vec2 worldPos = spriteDrawCmd.pos + (spriteXYs[idx] * spriteDrawCmd.size);
	gl_Position = pushConstants.sceneBuffer.projView * vec4(worldPos, 0.0f, 1.0f);
	
	vec2 t = spriteUVs[idx];
	uvOut = vec2(
		spriteDrawCmd.uv1.x * (1.0f - t.x) + (spriteDrawCmd.uv2.x * t.x),
		spriteDrawCmd.uv1.y * (1.0f - t.y) + (spriteDrawCmd.uv2.y * t.y)
	);

	textureIdxOut = spriteDrawCmd.textureIdx;
	colorOut = spriteDrawCmd.color;
}