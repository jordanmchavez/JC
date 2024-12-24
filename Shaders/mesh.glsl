#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : enable

struct Vertex {
	vec3  xyz;
	float pad;
	vec2  uv;
	float pad2[2];
	vec4  rgba;
};

layout (buffer_reference, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4 view;
	mat4 proj;
	vec4 ambient;
};

layout (push_constant, scalar) uniform constants {
	mat4           model;
	VertexBuffer   vertexBufferPtr;
	SceneBuffer    scene;
	vec4           color;
	uint           bindlessTextureIdx;
	uint           pad[3];
} pushConstants;

layout (set = 0, binding = 0) uniform texture2D bindlessTextures[];
layout (set = 0, binding = 1) uniform sampler  bindlessSamplers[];

#define NEAREST_SAMPLER_IDX 0
#define LINEAR_SAMPLER_IDX  1
