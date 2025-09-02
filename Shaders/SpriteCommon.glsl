#extension GL_EXT_buffer_reference     : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : enable

struct DrawCmd {
	vec2  pos;
	vec2  size;
	vec2  uv1;
	vec2  uv2;
	vec4  color;
	vec4  borderColor;
	float border;
	float cornerRadius;
	float rotation;
	uint  textureIdx;
};

layout (buffer_reference, scalar) readonly buffer DrawCmdBuffer {
	DrawCmd drawCmds[];
};

layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4 projViews[];
};

layout (set = 0, binding = 0) uniform texture2D bindlessTextures[];
layout (set = 0, binding = 1) uniform sampler   bindlessSamplers[];

#define SamplerId_Nearest 0
#define SamplerId_Linear 1

layout (push_constant) uniform PushConstants {
	SceneBuffer   sceneBuffer;
	DrawCmdBuffer drawCmdBuffer;
	uint          sceneBufferIdx;
	uint          drawCmdStart;
} pushConstants;