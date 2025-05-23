#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "UiCommon.glsl"

layout (location = 0)      in  vec2  sdfPosIn;
layout (location = 1) flat in  vec2  halfRectIn;
layout (location = 2) flat in  vec4  fillColorIn;
layout (location = 3) flat in  vec4  borderColorIn;
layout (location = 4) flat in  float borderIn;
layout (location = 5) flat in  float cornerRadiusIn;
layout (location = 0)      out vec4  colorOut;

float RectSdf(vec2 p, vec2 b, float r) {
	return length(max(abs(p) - b + r, 0.0)) - r;
}

float lerpstep(float lo, float hi, float t) {
	return clamp((t - lo) / (hi - lo), 0, 1);
}

void main() {
	float dist = RectSdf(sdfPosIn, halfRectIn, cornerRadiusIn);
	float a = fwidth(dist);
	
	if (dist > -borderIn) {
		colorOut = borderColor;
		colorOut.a *= 1 - lerpstep(0, a, dist);
	} else {
		float t = lerpstep(-borderIn - a, -borderIn, dist);
		colorOut = mix(solidColor, borderColor, t);
	}
}