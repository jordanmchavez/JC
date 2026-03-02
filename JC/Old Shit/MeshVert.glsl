#version 450

#extension GL_ARB_shading_language_include: require

#include "MeshCommon.glsl"

layout (location = 0) out vec4 colorOut;

void main() {
	Vertex v = push.vertexBuffer.vertices[gl_VertexIndex];
	gl_Position = push.sceneBuffer.projView * push.transformBuffer.transforms[push.transformIdx] * vec4(v.pos, 1.0f);
	Material m = push.materialBuffer.materials[push.materialIdx];
	colorOut = m.color;
}