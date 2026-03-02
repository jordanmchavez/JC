#extension GL_EXT_buffer_reference     : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : enable

struct Vertex {
	vec3 pos;
	uint pad;
};

struct Material {
	vec4 color;
};

layout (buffer_reference, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout (buffer_reference, std430) readonly buffer TransformBuffer {
	mat4 transforms[];
};

layout (buffer_reference, std430) readonly buffer MaterialBuffer {
	Material materials[];
};

layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4 view;
	mat4 proj;
	mat4 projView;
};

layout (push_constant, scalar) uniform constants {
	VertexBuffer    vertexBuffer;
	TransformBuffer transformBuffer;
	MaterialBuffer  materialBuffer;
	SceneBuffer     sceneBuffer;
	uint            materialIdx;
	uint            transformIdx;
	uint            pad[2];
} push;

layout (set = 0, binding = 0) uniform texture2D bindlessTextures[];
layout (set = 0, binding = 1) uniform sampler   bindlessSamplers[];

#define SamplerId_Nearest 0
#define SamplerId_Linear 1

