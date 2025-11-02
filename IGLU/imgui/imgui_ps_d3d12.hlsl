struct PSInput {
  float4 position : SV_Position;
  float4 color : COLOR;
  float2 uv : TEXCOORD0;
};

Texture2D tex : register(t0);
SamplerState uSampler : register(s0);

float4 main(PSInput input) : SV_Target {
  return input.color * tex.Sample(uSampler, input.uv);
}
