#include "LTCData.slangh"
#include "LTCMath.slang"
#include "EnvLight.slang"

//import LTCData;
//import LTCMath;

Texture2D gPosW;
Texture2D gNormalW;
Texture2D gTangent;
Texture2D gRoughness;
Texture2D gDiffuseOpacity;
Texture2D gSpecRough;
Texture2D gVisibility;
Texture2D gLTCMatrix;
Texture2D gLTCMagnitue;
Texture2D gSkyBox;
Texture2DArray gLightColor;


SamplerState gSampler;

cbuffer PerFrameCB
{
    SPassData g;
    EnvMapLighting gEnvMapLighting;
};

#define PI 3.14159265

float3 fecthFilteredLightTexture(float4 L[4], float3 vLookupVec)
{
    float3 V1 = L[1].xyz - L[0].xyz;
    float3 V2 = L[3].xyz - L[0].xyz;
    float3 PlaneOrtho = cross(V1, V2);
    float PlaneAreaSquared = dot(PlaneOrtho, PlaneOrtho);
	
    SRay Ray;
    Ray.m_Origin = float3(0);
    Ray.m_Dir = vLookupVec;
    float4 Plane = float4(PlaneOrtho, -dot(PlaneOrtho, L[0].xyz));
    float Distance2Plane;
    rayPlaneIntersect(Ray, Plane, Distance2Plane);
	
    float3 P = Distance2Plane * Ray.m_Dir - L[0].xyz;

    float Dot_V1_V2 = dot(V1, V2);
    float Inv_Dot_V1_V1 = 1.0 / dot(V1, V1);
    float3 V2_ = V2 - V1 * Dot_V1_V2 * Inv_Dot_V1_V1;
    float2 UV;
    UV.y = dot(V2_, P) / dot(V2_, V2_);
    UV.x = dot(V1, P) * Inv_Dot_V1_V1 - Dot_V1_V2 * Inv_Dot_V1_V1 * UV.y;
    UV = float2(UV.x, UV.y);
	
    float Distance = abs(Distance2Plane) / pow(PlaneAreaSquared, 0.25);
	
    float Lod = log(2048.0 * Distance) / log(3.0);

    uint3 texDim;
    uint levels;
    gLightColor.GetDimensions(0, texDim.x, texDim.y, texDim.z, levels);

    float LodA = clamp(floor(Lod),0,7);
    float LodB = clamp(ceil(Lod), 0, 7);
    
    float t = Lod - LodA;
    //float3 ColorA = gLightColor.Sample(gSampler, float3(UV, LodA / float(texDim.z))).rgb;
    //float3 ColorB = gLightColor.Sample(gSampler, float3(UV, LodB / float(texDim.z))).rgb
    float3 ColorA = gLightColor.Load(int4(UV * texDim.xy, LodA, 0)).xyz;
    float3 ColorB = gLightColor.Load(int4(UV * texDim.xy, LodB, 0)).xyz;
    return lerp(ColorA, ColorB, t);
}

float3 IntegrateEdgeVec(float3 v1, float3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * pow(sqrt(max(1.0 - x * x, 1e-7)),-0.5) - v;

    return cross(v1, v2) * theta_sintheta;
}

float2 LTC_Coords(float vCosTheta, float vRoughness)
{
    uint2 texDim;
    uint levels;
    gLTCMatrix.GetDimensions(0, texDim.x, texDim.y, levels);

    float Theta = acos(vCosTheta);
    float2 Coords = float2(vRoughness, Theta / (0.5 * PI));
    const float LUT_SIZE = texDim.x;
    // scale and bias coordinates, for correct filtered lookup
    Coords = Coords * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;

    return Coords;
}

float3 LTC_Evaluate(
    float3 N, float3 V, float3 P, float2 iPos, float3x3 Minv, float3 points[4], bool twoSided)
{
    float3 T1 = normalize(gTangent[iPos].xyz);
    T1 = normalize(V - N * dot(V, N));
    float3 T2 = normalize(cross(N, T1));

    // rotate area light in (T1, T2, N) basis
    float3x3 MatToTan = transpose(float3x3(T1, T2, N));
    //Minv = mul(transpose(float3x3(T1, T2, N)), Minv);

    // polygon (allocate 5 vertices for clipping)
    float3 L[5];
    L[0] = mul(points[0] - P, MatToTan);
    L[1] = mul(points[1] - P, MatToTan);
    L[2] = mul(points[2] - P, MatToTan);
    L[3] = mul(points[3] - P, MatToTan);
    L[0] = mul(L[0], Minv);
    L[1] = mul(L[1], Minv);
    L[2] = mul(L[2], Minv);
    L[3] = mul(L[3], Minv);

    // integrate
    float sum = 0.0;
    
    float3 dir = points[0].xyz - P;
    float3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    float3 vsum = float3(0.0);

    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);
    float z = vsum.z / len;

    if (behind)
        z = -z;

    float2 uv = float2(z * 0.5 + 0.5, len);
    uint2 uvtexDim;
    uint uvlevels;
    gLTCMagnitue.GetDimensions(0, uvtexDim.x, uvtexDim.y, uvlevels);

    const float LUT_SIZE = uvtexDim.x;
    const float LUT_SCALE = (LUT_SIZE - 1.0) / LUT_SIZE;
    const float LUT_BIAS = 0.5 / LUT_SIZE;
    uv = uv * LUT_SCALE + LUT_BIAS;
    //uv.x = 1. - uv.x;

    float scale = gLTCMagnitue.Sample(gSampler, uv).w;

    sum = len * scale;

    if (behind && !twoSided)
        sum = 0.0;
    

    float3 Lo_i = float3(sum, sum, sum);

    return Lo_i;
}

float3 integrateLTC(float3 vNormal, float3 vViewDir, float3 vFragPos, float2 iPos, float3x3 vLTCMatrix, float4 vPolygonalLightVertexPos[4], bool vTwoSided)
{
    float3 Tangent = normalize(gTangent[iPos].xyz);
    Tangent = normalize(vViewDir - vNormal * dot(vViewDir, vNormal));
    float3 Bitangent = cross(vNormal, Tangent);

    float3x3 MatW2T = transpose(float3x3(Tangent, Bitangent, vNormal));
    
    float3 L[4];
    float4 L_BC[4];
    L[0] = mul((vPolygonalLightVertexPos[0].xyz - vFragPos), MatW2T);
    L[1] = mul((vPolygonalLightVertexPos[1].xyz - vFragPos), MatW2T);
    L[2] = mul((vPolygonalLightVertexPos[2].xyz - vFragPos), MatW2T);
    L[3] = mul((vPolygonalLightVertexPos[3].xyz - vFragPos), MatW2T);
    L_BC[0].xyz = L[0] = mul(L[0], vLTCMatrix);
    L_BC[1].xyz = L[1] = mul(L[1], vLTCMatrix);
    L_BC[2].xyz = L[2] = mul(L[2], vLTCMatrix);
    L_BC[3].xyz = L[3] = mul(L[3], vLTCMatrix);
    	
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    float3 VSum = float3(0.0);
    VSum += integrateEdge(L[0], L[1]);
    VSum += integrateEdge(L[1], L[2]);
    VSum += integrateEdge(L[2], L[3]);
    VSum += integrateEdge(L[3], L[0]);

    float3 dir = vFragPos - vPolygonalLightVertexPos[0].xyz;
    float3 lightNormal = -cross(vPolygonalLightVertexPos[1].xyz - vPolygonalLightVertexPos[0].xyz, vPolygonalLightVertexPos[3].xyz - vPolygonalLightVertexPos[0].xyz);
    bool behind = (dot(dir, lightNormal) < 0.0);

    float Sum = vTwoSided ? abs(VSum.z) : max(VSum.z, 0.0);
    if (behind && !vTwoSided)
        Sum = 0;

    float3 LightTint = g.LightTint.xyz;
    #ifdef USE_TEXTURE_LIGHT
    float3 LookupVector = normalize(VSum);
    LightTint *= fecthFilteredLightTexture(L_BC, LookupVector);
    #endif

    return float3(Sum) * LightTint;
}

float4 main(float2 texC : TEXCOORD) : SV_Target0
{
    float3 LightPolygonPoints[4] =
    {
        float3(0., 0., 0),
        float3(1., 0., 0),
        float3(1., 1., 0),
        float3(0., 1., 0),
    };

    uint2 texDim;
    uint levels;
    gPosW.GetDimensions(0, texDim.x, texDim.y, levels);

    float2 pixelPos = texC * texDim;
    int2 iPos = int2(pixelPos);

    #ifdef has_gSkyBox
    float3 SkyBoxColor = gSkyBox[iPos].rgb;
    if(length(SkyBoxColor)>0.)
    {
        return float4(SkyBoxColor,1.);
    }
    #endif

    float3 PosW = gPosW[iPos].xyz;
    float3 GroundNormal = normalize(gNormalW[iPos].xyz);
    float3 ViewDir = normalize(g.CamPosW.xyz - PosW);

    float Roughness;
    float3 SpecularColor, DiffuseColor;
    #ifdef has_gRoughness
        Roughness = gRoughness[iPos].r;
    #else
        Roughness = g.Roughness;
    #endif
    #ifdef has_gDiffuseOpacity
        DiffuseColor = gDiffuseOpacity[iPos].rgb;
    #else
        DiffuseColor = g.DiffuseColor.rgb;
    #endif
    #ifdef has_gSpecRough
        SpecularColor = gSpecRough[iPos].rgb;
    #else
        SpecularColor = g.SpecularColor.rgb;
    #endif

    float2 LTXCrd = LTC_Coords(dot(GroundNormal, ViewDir), Roughness);

    float4 LTCMatrixComponents = gLTCMatrix.Sample(gSampler, LTXCrd);
    float3x3 LTCMatrix = float3x3
	(
		float3(1, 0, LTCMatrixComponents.y),
		float3(0, LTCMatrixComponents.z, 0),
		float3(LTCMatrixComponents.w, 0, LTCMatrixComponents.x)
	);

    float3x3 MatE =
    {
        float3(1, 0, 0),
        float3(0, 1, 0),
        float3(0, 0, 1)
    };

    float3 Diffuse = integrateLTC(GroundNormal, ViewDir, PosW, iPos, MatE, g.LightPolygonPoints, g.TwoSide > 0.5);
    float3 Specular = integrateLTC(GroundNormal, ViewDir, PosW, iPos, LTCMatrix, g.LightPolygonPoints, g.TwoSide > 0.5);
    float4 LTCMag = gLTCMagnitue.Sample(gSampler, LTXCrd);
    Specular *= LTCMag.w;
    float2 Schlick = LTCMag.xy;
    Specular *= SpecularColor * Schlick.x + (1.0 - SpecularColor) * Schlick.y;

    float3 col = Diffuse * DiffuseColor + Specular/30;
    //float3 col = lerp(g.DiffuseColor.xyz * Diffuse, Specular / 10., 1. - g.Roughness);
    float3 ResultColor = g.Intensity * (col);
    ResultColor /= PI;

    #ifdef has_gVisibility
        ResultColor *= gVisibility[iPos].xyz;
    #endif

    #ifdef _USE_ENV_MAP
    float ggxAlpha = max(0.0064, Roughness * Roughness);
    float NdotV = dot(GroundNormal,ViewDir);
    ResultColor.rgb += gEnvMapLighting.evalDiffuse(GroundNormal,ViewDir,ggxAlpha,NdotV,DiffuseColor);
    ResultColor.rgb += gEnvMapLighting.evalSpecular(GroundNormal,ViewDir,ggxAlpha,NdotV,SpecularColor, reflect(-ViewDir, GroundNormal));
    #endif

    return float4(ResultColor, 1.0);
}
