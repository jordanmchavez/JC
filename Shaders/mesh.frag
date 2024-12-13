#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : require

layout (location = 0) in vec2 inUv;
layout (location = 0) out vec4 outColor;

struct Vertex {
	vec4 pos;
	vec4 uv;
};

layout (buffer_reference, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout (push_constant) uniform PushConstants {
	VertexBuffer vertexBuffer;
	uint         samplerIndex;
} pushConstants;

layout (set = 0, binding = 0) uniform sampler2D samplers[];

void main() {
    outColor = texture(nonuniformEXT(samplers[pushConstants.samplerIndex]), inUv);
}