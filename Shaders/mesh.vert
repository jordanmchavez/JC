#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 outTexCoord;

layout (push_constant) uniform PushConstants {
	int bufferIndex;
	int textureIndex;
} pushConstants;

layout (binding = 0) uniform Uniforms {
	vec2 offset;
	float rotation;
	float scale;
	float aspect;
} uniforms[];

vec3 rotate(vec3 v, float a) {
	float s = sin(a);
	float c = cos(a);
	float x = v.x * c - v.y * s;
	float y = v.x * s + v.y * c;
	return vec3(x, y, v.z);
}

void main() {
	vec3  offset   = vec3(uniforms[pushConstants.bufferIndex].offset, 0.0f);
	float rotation = uniforms[pushConstants.bufferIndex].rotation;
	float scale    = uniforms[pushConstants.bufferIndex].scale;
	vec3  aspect   = vec3(1.0f, uniforms[pushConstants.bufferIndex].aspect, 1.0f);

	gl_Position = vec4((offset + rotate(inPosition, rotation) * scale) * aspect, 1.0f);
	outTexCoord = inTexCoord;
}