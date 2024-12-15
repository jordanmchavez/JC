#version 450
#extension GL_ARB_shading_language_include: require

#include "mesh.glsl"

layout (location = 0) out vec2 uvOut;

void main() {
	Vertex v = pushConstants.vertexBufferPtr.vertices[gl_VertexIndex];
	vec4 modelv = pushConstants.model * vec4(v.pos, 1.0f);
	vec4 viewv = pushConstants.scene.view * modelv;
	gl_Position = pushConstants.scene.proj * viewv;
	uvOut = vec2(v.u, v.v);
}