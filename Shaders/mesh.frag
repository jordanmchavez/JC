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
	SceneBuffer    scenePtr;
	MaterialBuffer materialsPtr;
	VertexBuffer   vertexBufferPtr;
	uint           materialIdx;
} pcs;

// bindless samplers
layout (set = 0, binding = 0) uniform sampler2D samplers[];

layout (location = 0) in vec2 uvIn;

layout (location = 0) out vec4 colorOut;

void main() {
	Material material = pcs.materialsPtr.materials[pcs.materialIdx];
	vec4 texColor = texture(nonuniformEXT(samplers[material.textureIdx]), uvIn);
    colorOut = texColor * material.color;
}