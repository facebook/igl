cbuffer Uniforms : register(b0) {
  float4x4 projectionMatrix;
};

struct VSInput {
  float2 position : POSITION;
  float2 uv : TEXCOORD0;
  float4 color : COLOR;
};

struct PSInput {
  float4 position : SV_Position;
  float4 color : COLOR;
  float2 uv : TEXCOORD0;
};

PSInput main(VSInput input) {
  PSInput output;
  output.position = mul(projectionMatrix, float4(input.position.xy, 0, 1));
  output.color = input.color;
  output.uv = input.uv;
  return output;
}
