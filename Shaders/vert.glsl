#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, binding = 0) uniform buffer {
	mat4 proj;
	mat4 view;
	mat4 model;
} Uniform;

layout (location = 0) in vec4 pos;

void main() {
	gl_Position = pos * (Uniform.model * Uniform.view * Uniform.proj);
}