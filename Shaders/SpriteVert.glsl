#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "SpriteCommon.glsl"

layout (location = 0)      out vec2  uvOut;
layout (location = 1)      out vec2  sdfPosOut;
layout (location = 2) flat out vec2  halfRectOut;
layout (location = 3) flat out vec4  colorOut;
layout (location = 4) flat out vec4  borderColorOut;
layout (location = 5) flat out float borderOut;
layout (location = 6) flat out float cornerRadiusOut;
layout (location = 7) flat out uint  textureIdxOut;

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
	uint idx = gl_VertexIndex % 6;
	vec2 offset = offsets[idx];
	vec2 worldPos = drawCmd.pos + (offset * drawCmd.size);
	gl_Position = pushConstants.sceneBuffer.projViews[pushConstants.sceneBufferIdx] * vec4(worldPos, 0.0f, 1.0f);
	uvOut = vec2(
		drawCmd.uv1.x * (1.0f - offset.x) + (drawCmd.uv2.x * offset.x),
		drawCmd.uv1.y * (1.0f - offset.y) + (drawCmd.uv2.y * offset.y)
	);
	sdfPosOut       = offset * drawCmd.size;
	halfRectOut     = drawCmd.size / 2.0f;
	colorOut        = drawCmd.color;
	borderColorOut  = drawCmd.borderColor;
	borderOut       = drawCmd.border;
	cornerRadiusOut = drawCmd.cornerRadius;
	textureIdxOut   = drawCmd.textureIdx;
}