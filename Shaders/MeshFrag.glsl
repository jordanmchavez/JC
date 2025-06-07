#version 450

//#extension GL_ARB_shading_language_include: require

//#include "MeshCommon.glsl"

layout (location = 0) in  vec4 colorIn;
layout (location = 0) out vec4 colorOut;

void main() {
	colorOut = colorIn;
}