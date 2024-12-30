#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "sprite.common.glsl"

layout (location = 0) out vec2 uvOut;
layout (location = 1) out vec3 worldPosOut;
layout (location = 2) flat out vec3 lightPosOut;
layout (location = 3) flat out uint diffuseIdxOut;
layout (location = 4) flat out uint normalIdxOut;

vec2 spriteXYs[6] = vec2[6](
	vec2(-0.5f,  0.5f),
	vec2( 0.5f,  0.5f),
	vec2( 0.5f, -0.5f),
	vec2( 0.5f, -0.5f),
	vec2(-0.5f, -0.5f),
	vec2(-0.5f,  0.5f)
);

vec2 spriteUVs[6] = vec2[6](
	vec2(0.0f, 0.0f),
	vec2(1.0f, 0.0f),
	vec2(1.0f, 1.0f),
	vec2(1.0f, 1.0f),
	vec2(0.0f, 1.0f),
	vec2(0.0f, 0.0f)
);

void main() {
	Sprite sprite = pushConstants.sceneBuffer.spriteBuffer.sprites[gl_InstanceIndex];
	uint idx = gl_VertexIndex % 6;

	vec4 worldPos = sprite.model * vec4(spriteXYs[idx], 0.0f, 1.0f);
	gl_Position = pushConstants.sceneBuffer.projView * worldPos;
	
	vec2 t = spriteUVs[idx];
	uvOut = vec2(
		sprite.uv1.x * (1.0f - t.x) + sprite.uv2.x * t.x,
		sprite.uv1.y * (1.0f - t.y) + sprite.uv2.y * t.y
	);

	mat3 model3 = mat3(sprite.model);

	vec3 tangent   = normalize(model3 * vec3(1.0f, 0.0f, 0.0f));
	vec3 bitangent = normalize(model3 * vec3(0.0f, 1.0f, 0.0f));
	vec3 normal    = normalize(model3 * vec3(0.0f, 0.0f, 1.0f));
	mat3 tbn       = transpose(mat3(tangent, bitangent, normal));

	worldPosOut   = worldPos.rgb;
	//lightPosOut   = pushConstants.sceneBuffer.lightPos;
	lightPosOut   = tbn * pushConstants.sceneBuffer.lightPos;
	diffuseIdxOut = sprite.diffuseIdx;
	normalIdxOut  = sprite.normalIdx;
}