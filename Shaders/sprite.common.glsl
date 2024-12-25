#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_nonuniform_qualifier : enable

struct Sprite {
	mat2 model;
	vec2 xyz;
	vec2 uv1;
	vec2 uv2;
	uint textureId;
	uint pad;
};

layout (buffer_reference, scalar) readonly buffer SpriteBuffer {
	Sprite sprites[];
};

layout (set = 0, binding = 0) uniform texture2D bindlessTextures[];
layout (set = 0, binding = 1) uniform sampler bindlessSamplers[];

#define SamplerId_Nearest 0
#define SamplerId_Linear 1

layout (push_constant) uniform PushConstants {
	mat4 viewProj;
	SpriteBuffer spriteBuffer;
} pushConstants;