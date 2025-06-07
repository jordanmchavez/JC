#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "Common.glsl"

layout (location = 0)      in  vec2  uvIn;
layout (location = 1)      in  vec2  sdfPosIn;
layout (location = 2) flat in  vec2  halfRectIn;
layout (location = 3) flat in  vec4  colorIn;
layout (location = 4) flat in  vec4  borderColorIn;
layout (location = 5) flat in  float borderIn;
layout (location = 6) flat in  float cornerRadiusIn;
layout (location = 7) flat in  uint  textureIdxIn;
layout (location = 0)      out vec4  colorOut;

float RectSdf(vec2 p, vec2 b, float r) {
	return length(max(abs(p) - b + r, 0.0)) - r;
}

float LerpStep(float lo, float hi, float t) {
	return clamp((t - lo) / (hi - lo), 0, 1);
}

void main() {
	if (textureIdxIn != 0) {
		colorOut = texture(nonuniformEXT(sampler2D(bindlessTextures[textureIdxIn], bindlessSamplers[SamplerId_Nearest])), uvIn) * colorIn;
	} else {
		colorOut = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	if (borderIn > 0.0f || cornerRadiusIn > 0.0f) {	
		float dist = RectSdf(sdfPosIn, halfRectIn, cornerRadiusIn);
		float a = fwidth(dist);
	
		if (dist > -borderIn) {
			colorOut *= borderColorIn;
			colorOut.a *= 1 - LerpStep(0, a, dist);
		} else {
			float t = LerpStep(-borderIn - a, -borderIn, dist);
			colorOut *= mix(colorIn, borderColorIn, t);
		}
	} else {
		colorOut *= colorIn;
	}
}