#version 450
#extension GL_ARB_shading_language_include: require

#include "mesh.glsl"

layout (location = 0) in vec2 uvIn;
layout (location = 1) in vec4 colorIn;
layout (location = 0) out vec4 colorOut;

void main() {
/*	vec4 textureColor = texture(
		nonuniformEXT(
			sampler2D(
				bindlessTextures[pushConstants.bindlessTextureIdx],
				bindlessSamplers[LINEAR_SAMPLER_IDX]
			)
		),
		uvIn
	);*/
    //colorOut = vec4(1.0f, 1.0f, 1.0f, 1.0f);//textureColor * pushConstants.color;
    colorOut = colorIn;
}