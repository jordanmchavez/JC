#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "sprite.common.glsl"

layout (location = 0)      in vec2 uvIn;
layout (location = 1)      in vec3 worldPosIn;
layout (location = 2) flat in uint diffuseIdxIn;
layout (location = 3) flat in uint normalIdxIn;

layout (location = 0) out vec4 rgbaOut;

void main() {
	// vec3 texNormal = texture(
	// 	nonuniformEXT(sampler2D(bindlessTextures[normalIdxIn], bindlessSamplers[SamplerId_Nearest])),
	// 	uvIn
	// ).rgb;
	// vec3 texNormalC = texNormal * 2.0 - 1.0;
	// vec3 texNormalN = normalize(texNormalC);

    vec4 texColor = texture(
		nonuniformEXT(sampler2D(bindlessTextures[diffuseIdxIn], bindlessSamplers[SamplerId_Nearest])),
		uvIn
	);

	vec3 lightAccum = vec3(0.0, 0.0, 0.0);
	for (int i = 0; i < 4; i++) {
		Light light = pushConstants.sceneBuffer.lights[i];

		vec3  posToLight = light.position - worldPosIn;
		//float distSqr    = dot(posToLight, posToLight);

		float dist = length(posToLight);
		float atten = max(0.0f, light.radius - dist) / light.radius;

		// float radiusInv  = 1.0 / light.radius;
		// float factor     = distSqr * radiusInv * radiusInv;
		// float smoother   = max(1.0 - factor * factor, 0.0);
		// float atten      = (smoother * smoother) / max(distSqr, 1e-4);

		lightAccum += light.color * atten;
	}

	float r = min(1.0f, lightAccum.r);
	float g = min(1.0f, lightAccum.g);
	float b = min(1.0f, lightAccum.b);
	//rgbaOut = vec4(r, g, b, 1.0) * texColor;
	rgbaOut = texColor;
}