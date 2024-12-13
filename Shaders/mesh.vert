#version 450
#extension GL_EXT_buffer_reference : require

struct Vertex {
	vec4 pos;
	vec4 uv;
};

layout (buffer_reference, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout (push_constant) uniform PushConstants {
	VertexBuffer vertexBuffer;
	uint         textureIndex;
} pushConstants;

layout (location = 0) out vec2 outUv;

void main() {
	vec4 pos = pushConstants.vertexBuffer.vertices[gl_VertexIndex].pos;
	vec4 uv  = pushConstants.vertexBuffer.vertices[gl_VertexIndex].uv;
	gl_Position = pos;
	outUv = vec2(uv.x, uv.y);
}