cbuffer SceneConstantBuffer : register(b0) {
	float4x4 wvp;
};

struct PSIn {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSIn VSMain(float4 pos : POSITION, float2 uv: TEXCOORD) {
	PSIn res;
	res.pos = pos;//mul(pos, wvp);
	res.uv = uv;
	return res;
}

float4 PSMain(PSIn psIn) : SV_TARGET {
	return g_texture.Sample(g_sampler, psIn.uv);
}