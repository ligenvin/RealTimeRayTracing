#include "ShadowMapConstant.slangh"

StructuredBuffer<float4> gPointAndIdList;
RWTexture2DArray<uint> gOutputInfo;
// interlock/atomic operation only support int/uint, so depth need to store as int/uint
ByteAddressBuffer gNumPointBuffer;

cbuffer PerFrameCB : register(b0)
{
    SShadowMapData gShadowMapData;
    uint3 gDispatchDim;
}

inline uint pack(float vDepth, uint vId)
{
    uint High = (uint(round((1.0f - vDepth) * float(0xffffffu))) << 8);
    uint Low = (vId & 0x000000ffu);
    return High | Low;
}

inline int getPointNum()
{
    return (int)((uint)(gNumPointBuffer.Load(0)));
}

inline void fillMapWithPoint(float vPointIndex, int vMapIndex, uint2 vMapSize)
{
    float4 PointAndId = gPointAndIdList[vPointIndex];
    float3 Point = PointAndId.xyz;
    uint Id = asuint(PointAndId.w);
    float4x4 VP = gShadowMapData.allGlobalMat[vMapIndex];
    float4 PosH = mul(float4(Point, 1.0f), VP);
#ifdef FALCOR_D3D
    PosH.y = -PosH.y;
#endif
    float3 UVW = (PosH.xyz / PosH.w) * 0.5 + 0.5;
    uint3 TargetIndex = uint3(UVW.x * vMapSize.x, UVW.y * vMapSize.y, vMapIndex);
    uint PackedInfo = pack(UVW.z, Id);
    
    InterlockedMax(gOutputInfo[TargetIndex], PackedInfo);
    //gOutputShadowMap[TargetIndex] = PackedDepth;
}

inline int getStartPointIndex(int3 vGroupID, int3 vThreadID)
{
    int GroupSize = _SHADOW_MAP_SHADER_THREAD_NUM_X * _SHADOW_MAP_SHADER_THREAD_NUM_Y;
    int StartIndex = vGroupID.x * gDispatchDim.y + vGroupID.y;
    int ThreadStartIndex = vThreadID.x * _SHADOW_MAP_SHADER_THREAD_NUM_Y + vThreadID.y;
    StartIndex = StartIndex * GroupSize + ThreadStartIndex;
    return StartIndex * _SHADOW_MAP_SHADER_POINT_PER_THREAD;
}

[numthreads(_SHADOW_MAP_SHADER_THREAD_NUM_X, _SHADOW_MAP_SHADER_THREAD_NUM_Y, _SHADOW_MAP_SHADER_THREAD_NUM_Z)]
void main(int3 groupID : SV_GroupID, int3 threadID : SV_GroupThreadID)
{
    int PointNum = getPointNum();
    int StartPointIndex = getStartPointIndex(groupID, threadID);
    int EndPointIndex = min(StartPointIndex + _SHADOW_MAP_SHADER_POINT_PER_THREAD, PointNum);
    int N = EndPointIndex - StartPointIndex;

    uint3 Dimension;
    gOutputInfo.GetDimensions(Dimension.x, Dimension.y, Dimension.z);
    int MapNum = (int)Dimension.z;
    int StartMapIndex = groupID.z * _SHADOW_MAP_SHADER_THREAD_NUM_Z + threadID.z;
    
    //[unroll(_SHADOW_MAP_SHADER_POINT_PER_THREAD)]
    for (int i = 0; i < N; ++i)
    {
        fillMapWithPoint(StartPointIndex + i, StartMapIndex, Dimension.xy);
    }
}
