#version 450
#extension GL_ARB_shading_language_include: require

#include "mesh.glsl"

layout (location = 0) in vec2 uvIn;
layout (location = 0) out vec4 colorOut;

void main() {
	Material material = pushConstants.materialsPtr.materials[pushConstants.materialIdx];
	vec4 textureColor = texture(
		nonuniformEXT(
			sampler2D(
				bindlessTextures[material.bindlessTextureIdx],
				bindlessSamplers[LINEAR_SAMPLER_IDX]
			)
		),
		uvIn
	);
    colorOut = textureColor * material.color;
}