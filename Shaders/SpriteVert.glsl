#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "SpriteCommon.glsl"

layout (location = 0)      out vec2  uvOut;
layout (location = 1) flat out vec4  colorOut;
layout (location = 2) flat out uint  textureIdxOut;

vec2 offsets[6] = vec2[6](
	vec2(0.0f, 0.0f),
	vec2(0.0f, 1.0f),
	vec2(1.0f, 1.0f),
	vec2(1.0f, 1.0f),
	vec2(1.0f, 0.0f),
	vec2(0.0f, 0.0f)
);

void main() {
	DrawCmd drawCmd = pushConstants.drawCmdBuffer.drawCmds[gl_InstanceIndex + pushConstants.drawCmdStart];
	vec2 offset  = offsets[gl_VertexIndex];	// no need to % 6 since we're always called with 6 vertices
	float s = sin(drawCmd.rotation);
	float c = cos(drawCmd.rotation);
	vec2 local = (offset - 0.5) * drawCmd.size;
	vec2 worldPos = drawCmd.pos + vec2(
		local.x * c - local.y * s,
		local.x * s + local.y * c
	);
	gl_Position = pushConstants.sceneBuffer.projViews[pushConstants.sceneBufferIdx] * vec4(worldPos, 0.0f, 1.0f);

	uvOut = vec2(
		drawCmd.uv1.x * (1.0f - offset.x) + (drawCmd.uv2.x * offset.x),
		drawCmd.uv1.y * (1.0f - offset.y) + (drawCmd.uv2.y * offset.y)
	);
	colorOut        = drawCmd.color;
	textureIdxOut   = drawCmd.textureIdx;
}