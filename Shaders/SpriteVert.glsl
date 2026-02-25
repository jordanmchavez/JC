#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "SpriteCommon.glsl"

layout (location = 0)      out vec2  uvOut;
layout (location = 1) flat out vec4  colorOut;
layout (location = 2) flat out uint  textureIdxOut;
layout (location = 3) flat out vec4  outlineColorOut;
layout (location = 4) flat out float outlineWidthOut;
layout (location = 5) flat out vec2  uv1Out;
layout (location = 6) flat out vec2  uv2Out;

vec2 offsets[6] = vec2[6](
	vec2(-0.5, -0.5),
	vec2(-0.5,  0.5),
	vec2( 0.5,  0.5),
	vec2( 0.5,  0.5),
	vec2( 0.5, -0.5),
	vec2(-0.5, -0.5)
);

void main() {
	DrawCmd drawCmd = pushConstants.drawCmdBuffer.drawCmds[gl_InstanceIndex + pushConstants.drawCmdStart];
	vec2 offset = offsets[gl_VertexIndex];	// no need to % 6 since we're always called with 6 vertices
	float s = sin(drawCmd.rotation);
	float c = cos(drawCmd.rotation);
	vec2 local = drawCmd.size * offset;
	vec3 worldPos = drawCmd.pos + vec3(
		local.x * c - local.y * s,
		local.x * s + local.y * c,
		0.0
	);
	gl_Position = pushConstants.sceneBuffer.projViews[pushConstants.sceneBufferIdx] * vec4(worldPos, 1.0);

	offset += 0.5;
	uvOut = vec2(
		drawCmd.uv1.x * (1.0 - offset.x) + (drawCmd.uv2.x * offset.x),
		drawCmd.uv1.y * (1.0 - offset.y) + (drawCmd.uv2.y * offset.y)
	);
	colorOut         = drawCmd.color;
	textureIdxOut    = drawCmd.textureIdx;
	outlineColorOut  = drawCmd.outlineColor;
	outlineWidthOut  = drawCmd.outlineWidth;
	uv1Out           = drawCmd.uv1;
	uv2Out           = drawCmd.uv2;
}