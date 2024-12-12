#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outColor;

layout (push_constant, std430) uniform PushConstants {
	int bufferIndex;
	int textureIndex;
} pushConstants;

layout (binding = 1) uniform sampler2D textures[];

void main() {
	outColor = vec4(texture(textures[pushConstants.textureIndex], inTexCoord).rgb, 1);
}