#include "BilateralFilterCommon.slang"

struct SOut
{
    float4 Result : SV_Target0;
    float4 Debug : SV_Target1;
};

SOut main(float2 texC : TEXCOORD, float4 posH : SV_POSITION) : SV_Target
{
    uint3 Dim;
    gTexColor.GetDimensions(0, Dim.x, Dim.y, Dim.z);
    int2 TexCoordInt = int2(texC * Dim.xy);

    SOut Res;
    Res.Result = float4(separateJBFPassKernel(TexCoordInt, Dim.xy), 1.0);
    // print kernel size ratio
    float KernelSizeVis = (getKernelSize(TexCoordInt) - gKernelSize) / float(gAdaptiveRange);
    Res.Debug = float4(float3(KernelSizeVis), 1.0);
    return Res;
}
