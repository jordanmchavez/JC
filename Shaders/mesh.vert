#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : enable

// vertex buffers
struct Vertex {
	vec4 pos;
	vec4 uv;
};
layout (buffer_reference, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

// material buffer
struct Material {
	vec4 color;
	uint textureIdx;
	uint pad[3];
};
layout (buffer_reference, std430) readonly buffer MaterialBuffer {
	Material materials[];
};

// scene data buffer: don't need to bind
layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4 view;
	mat4 proj;
};

layout (push_constant, scalar) uniform constants {
	mat4           model;
	SceneBuffer    scene;
	MaterialBuffer materials;
	VertexBuffer   vertexBuffer;
	uint           materialIdx;
} pcs;

// bindless samplers
layout (set = 0, binding = 0) uniform sampler samplers[];

layout (location = 0) out vec2 uvOut;

void main() {
	Vertex v = pcs.vertexBuffer.vertices[gl_VertexIndex];
	vec4 startv = v.pos;
	vec4 modelv = pcs.model * startv;
	vec4 viewv = pcs.scene.view * modelv;
	vec4 projv = pcs.scene.proj * viewv;
	gl_Position = projv;
	uvOut = vec2(v.uv.x, v.uv.y);
}