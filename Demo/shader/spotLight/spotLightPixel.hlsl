Texture2D<float4> normalTex : register(t0);
Texture2D<uint4> materialTex : register(t1);
Texture2D<float> depthTex : register(t2);
Texture2D<uint4> stencilTex : register(t3);

Texture2D shadowMap : register(t4);
SamplerComparisonState shadowMapSamplerComparisonState : register(s0);

cbuffer ShadowMatrixBuffer : register (b0)
{
	matrix shadowMatrix;
}

cbuffer IVPMatrixBuffer : register (b1)
{
	matrix IVPMatrix;
}

cbuffer ScreenBuffer : register (b2)
{
	uint2 screen;
}

cbuffer CameraBuffer : register (b3)
{
	float3 camPosition;
}

cbuffer LightInfoBuffer : register (b4)
{
	float3 lightColor;
	float3 lightPosition;
	float3 lightDirection;
	float lightSpotCutOff;
	bool withShadowMapping;
}

struct PS_INPUT
{
	float4 position : SV_POSITION;
};

struct PS_OUTPUT
{
	float4 light : SV_TARGET;
};

struct light
{
	float3 diff;
	float3 spec;
};

uint4 pack(int4 a, int4 b, int4 c, int4 d)
{
	uint4 res =
		(0xFF000000 & (a << 24)) |
		(0x00FF0000 & (b << 16)) |
		(0x0000FF00 & (c << 8)) |
		(0x000000FF & d);

	return res;
}

int4 unpack(uint4 a, int v)
{
	return (0x000000FF & (a >> (v * 8)));
}

float3 getPosition(float2 pixelCoord)
{
	float depth = depthTex[pixelCoord];
	float4 tmp1 = float4(pixelCoord.x / screen.x * 2.0 - 1.0, (1.0 - (pixelCoord.y / screen.y)) * 2.0 - 1.0, depth, 1.0);
	float4 tmp2 = mul(IVPMatrix, tmp1);
	return tmp2.xyz / tmp2.w;
}

float lookUp(float4 coord, float2 offSet, int2 texSize)
{
	coord.x = 0.5f + (coord.x / coord.w * 0.5f);
	coord.y = 0.5f - (coord.y / coord.w * 0.5f);
	coord.z = (coord.z - 0.0025) / coord.w;

	coord.x += offSet.x * (1.0 / texSize.x);
	coord.y += offSet.y * (1.0 / texSize.y);

	if (coord.x > 1.0 || coord.x < 0.0 || coord.y > 1.0 || coord.y < 0.0)
		return 1.0;
	return shadowMap.SampleCmpLevelZero(shadowMapSamplerComparisonState, coord.xy, coord.z);
}

float calcShadow(float4 coord, float pcf)
{
	float a, x, y, shadow = 0.0;
	uint width, height;
	shadowMap.GetDimensions(width, height);

	a = (pcf - 1.0) / 2.0;
	for (x = -a; x <= a; x += 1.0)
		for (y = -a; y <= a; y += 1.0)
			shadow += lookUp(coord, float2(x, y), int2(width, height));
	shadow /= (pcf*pcf);

	return shadow;
}

light calcSpotLight(float3 N, float3 eyeVec, float3 position, float shininess, float shadow) // N need to be normalize
{
	float3 L, V, R, D;
	float cos_cur_angle, cos_inner_cone_angle, cos_outer_cone_angle, cos_inner_minus_outer_angle;
	float cosTheta, spot, specular;
	light res;

	res.diff = float3(0.0, 0.0, 0.0);
	res.spec = float3(0.0, 0.0, 0.0);

	if (length(N) == 0)
		return res;

	L = normalize(lightPosition - position);
	V = normalize(eyeVec);
	R = reflect(-L, N);
	D = normalize(lightDirection);

	cos_cur_angle = dot(-L, D);
	cos_outer_cone_angle = cos(radians(lightSpotCutOff));
	cos_inner_cone_angle = cos_outer_cone_angle + 0.01;
	cos_inner_minus_outer_angle = cos_inner_cone_angle - cos_outer_cone_angle;
	spot = clamp((cos_cur_angle - cos_outer_cone_angle) / cos_inner_minus_outer_angle, 0.0, 1.0);

	res.diff = max(dot(N, L), 0.0) * lightColor * spot * shadow;
	res.spec = pow(max(dot(R, V), 0.0), shininess) * lightColor * spot * shadow;

	return res;
}

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT output = (PS_OUTPUT)0;

	if (stencilTex[input.position.xy].y != 1)
		return output;

	float3 position = getPosition(input.position.xy);
	float4 normal = normalTex[input.position.xy];
	uint4 material = materialTex[input.position.xy];

	float4 matDiffuse = float4(unpack(material, 1)) / 255;
	float4 matSpecular = float4(unpack(material, 0)) / 255;

	float s = 1.0;
	if (withShadowMapping)
		s = calcShadow(mul(shadowMatrix, float4(position, 1.0)), 1.0);
	light l = calcSpotLight(normal.xyz, camPosition - position, position, normal.w, s);

	output.light = (matDiffuse * float4(l.diff, 1.0)) + (matSpecular * float4(l.spec, 1.0));

	return output;
}