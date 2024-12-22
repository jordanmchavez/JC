struct Vertex {
	float2 position;
	float2 uv;
	float4 color;
};

struct RootConstants {
	float2 offset;
	uint   textureIndex;
};

struct PassConstants {
	uint vertexBufferIdx;
};

ConstantBuffer<RootConstant> root : register(b0);
ConstantBuffer<PassConstant> pass : register(b1);

sampler samplerNearts : register(s0);

void MainVs(
	in  uint   inVertexIdx : SV_VertexID,
	out float4 outPosition : SV_Position,
	out float2 outUv       : TEXCOORD,
	out float4 outColor    : COLOR
) {
	StructuredBuffer<Vertex> vertexBuffer = ResourceDescriptorHeap[pass.vertexBufferIdx];
	Vertex vertex = vertexBuffer.Load(inVertexIdx);
	outPosition = float4(vertex.position + root.offset, 0.0f, 1.0f);
	outUv       = vertex.uv;
	outColor    = vertex.color;
}

float4 MainPs(
	in float4 inPosition : SV_Position,
	in float2 inUv       : TEXCOORD
	in float4 inColor    : COLOR
) : SV_Target {
	Texture2D texture = ResourceDescriptorHeap[root.textureIndex];
	color = texture.SampleLevel(samplerNearest, inUv, 0);
	color *= inColor;
	return color;
}