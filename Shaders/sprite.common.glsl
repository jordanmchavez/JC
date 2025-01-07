#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_nonuniform_qualifier : enable

struct SpriteDrawCmd {
	mat4 model;
	vec2 uv1;
	vec2 uv2;
	uint diffuseIdx;
	uint normalIdx;
	uint pad[2];
};

layout (buffer_reference, scalar) readonly buffer SpriteDrawCmdBuffer {
	SpriteDrawCmd spriteDrawCmds[];
};

struct Light {
	vec3  position;
	float radius;
	vec3  color;
	float pad;
};
layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4                projView;
	Light               lights[4];
	SpriteDrawCmdBuffer spriteDrawCmdBuffer;

};

layout (set = 0, binding = 0) uniform texture2D bindlessTextures[];
layout (set = 0, binding = 1) uniform sampler bindlessSamplers[];

#define SamplerId_Nearest 0
#define SamplerId_Linear 1

layout (push_constant) uniform PushConstants {
	SceneBuffer sceneBuffer;
} pushConstants;