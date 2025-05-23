#extension GL_EXT_buffer_reference     : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : enable

struct DrawCmd {
	vec2  pos;
	vec2  size;
	vec4  fillColor;
	vec4  borderColor;
	float border;
	float cornerRadius;
	uint  pad[2];
};

layout (buffer_reference, scalar) readonly buffer DrawCmdBuffer {
	DrawCmd drawCmds[];
};

layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4          projView;
	DrawCmdBuffer drawCmdBuffer;
};

layout (set = 0, binding = 0) uniform texture2D bindlessTextures[];
layout (set = 0, binding = 1) uniform sampler   bindlessSamplers[];

#define SamplerId_Nearest 0
#define SamplerId_Linear 1

layout (push_constant) uniform PushConstants {
	SceneBuffer sceneBuffer;
} pushConstants;