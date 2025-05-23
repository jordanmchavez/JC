#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "UiCommon.glsl"

layout (location = 0)      out vec2  sdfPosOut;
layout (location = 1) flat out vec2  halfRectOut;
layout (location = 2) flat out vec4  fillColorOut;
layout (location = 3) flat out vec4  borderColorOut;
layout (location = 4) flat out float borderOut;
layout (location = 5) flat out float cornerRadiusOut;

vec2 spriteOffsets[6] = vec2[6](
	vec2(-0.5f,  0.5f),
	vec2( 0.5f,  0.5f),
	vec2( 0.5f, -0.5f),
	vec2( 0.5f, -0.5f),
	vec2(-0.5f, -0.5f),
	vec2(-0.5f,  0.5f)
);

void main() {
	DrawCmd drawCmd = pushConstants.sceneBuffer.drawCmdBuffer.drawCmds[gl_InstanceIndex];
	uint idx = gl_VertexIndex % 6;
	vec2 offset = spriteOffsets[idx];
	vec2 worldPos = drawCmd.pos + (offset * drawCmd.size);

	gl_Position = pushConstants.sceneBuffer.projView * vec4(worldPos, 0.0f, 1.0f);

	sdfPosOut       = offset * drawCmd.size;
	halfRectOut     = drawCmd.size / 2.0f;
	fillColorOut    = drawCmd.fillColor;
	borderColorOut  = drawCmd.borderColor;
	borderOut       = drawCmd.border;
	cornerRadiusOut = drawCmd.cornerRadius;
}