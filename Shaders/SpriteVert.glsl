#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "SpriteCommon.glsl"

layout (location = 0)      out vec2 uvOut;
layout (location = 1)      out vec4 colorOut;
layout (location = 2) flat out uint textureIdxOut;

vec2 spriteOffsets[6] = vec2[6](
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
	DrawCmd drawCmd = pushConstants.sceneBuffer.drawCmdBuffer.drawCmds[gl_InstanceIndex];
	uint idx = gl_VertexIndex % 6;

	vec2 worldPos = drawCmd.pos + (spriteOffsets[idx] * drawCmd.size);
	gl_Position = pushConstants.sceneBuffer.projView * vec4(worldPos, 0.0f, 1.0f);
	
	vec2 t = spriteUVs[idx];
	uvOut = vec2(
		drawCmd.uv1.x * (1.0f - t.x) + (drawCmd.uv2.x * t.x),
		drawCmd.uv1.y * (1.0f - t.y) + (drawCmd.uv2.y * t.y)
	);

	textureIdxOut = drawCmd.textureIdx;
	colorOut = drawCmd.color;
}