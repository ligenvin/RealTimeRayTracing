/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "MultiViewShadowMap.h"
#include "Utils/Sampling/SampleGenerator.h"
#include "ShadowMapConstant.slangh"

namespace
{
    const char kDesc[] = "Simple Shadow map pass";

    // input
    const std::string kDepth = "Depth";

    // internal parameters
    const std::string kMapSize = "mapSize";
    const std::string kDepthFormat = "depthFormat";
    const std::string kShadowMapSet = "ShadowMap";

    // output
    const std::string kVisibility = "Visibility";
    const std::string kDebug = "Debug";
   
    // shader file path
    const std::string kPointGenerationFile = "RenderPasses/SpatioTemporalSM/PointGenerationPass.slang";
    const std::string kShadowPassfile = "RenderPasses/SpatioTemporalSM/ShadowPass.cs.slang";
    const std::string kVisibilityPassfile = "RenderPasses/SpatioTemporalSM/VisibilityPass.ps.slang";
}

static CPUSampleGenerator::SharedPtr createSamplePattern(STSM_MultiViewShadowMap::SamplePattern type, uint32_t sampleCount)
{
    switch (type)
    {
    case STSM_MultiViewShadowMap::SamplePattern::Center: return nullptr;
    case STSM_MultiViewShadowMap::SamplePattern::DirectX: return DxSamplePattern::create(sampleCount);
    case STSM_MultiViewShadowMap::SamplePattern::Halton: return HaltonSamplePattern::create(sampleCount);
    case STSM_MultiViewShadowMap::SamplePattern::Stratitied: return StratifiedSamplePattern::create(sampleCount);
    default:
        should_not_get_here();
        return nullptr;
    }
}

STSM_MultiViewShadowMap::STSM_MultiViewShadowMap()
{
    createPointGenerationPassResource();
    createShadowPassResource();
    createVisibilityPassResource();

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point).setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border).setBorderColor(float4(1.0f));
    samplerDesc.setLodParams(0.f, 0.f, 0.f);
    samplerDesc.setComparisonMode(Sampler::ComparisonMode::LessEqual);
    mVisibilityPass.pPointCmpSampler = Sampler::create(samplerDesc);

    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    mVisibilityPass.pLinearCmpSampler = Sampler::create(samplerDesc);
}

STSM_MultiViewShadowMap::SharedPtr STSM_MultiViewShadowMap::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    return SharedPtr(new STSM_MultiViewShadowMap());
}

std::string STSM_MultiViewShadowMap::getDesc() { return kDesc; }

Dictionary STSM_MultiViewShadowMap::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection STSM_MultiViewShadowMap::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kDepth, "Depth");
    reflector.addInternal(kShadowMapSet, "ShadowMapSet").bindFlags(ResourceBindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource).format(mShadowMapPass.DepthFormat).texture2D(mShadowMapPass.MapSize.x, mShadowMapPass.MapSize.y, 0, 1, mNumShadowMapPerFrame);
    reflector.addOutput(kVisibility, "Visibility").bindFlags(ResourceBindFlags::RenderTarget | Resource::BindFlags::ShaderResource).format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addOutput(kDebug, "Debug").bindFlags(ResourceBindFlags::RenderTarget | Resource::BindFlags::ShaderResource).format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    return reflector;
}

void STSM_MultiViewShadowMap::compile(RenderContext* pContext, const CompileData& compileData)
{
}

static bool checkOffset(size_t cbOffset, size_t cppOffset, const char* field)
{
    if (cbOffset != cppOffset)
    {
        logError("CsmData::" + std::string(field) + " CB offset mismatch. CB offset is " + std::to_string(cbOffset) + ", C++ data offset is " + std::to_string(cppOffset));
        return false;
    }
    return true;
}

#if _LOG_ENABLED
#define check_offset(_a) {static bool b = true; if(b) {assert(checkOffset(pCB["gSTsmData"][#_a].getByteOffset(), offsetof(StsmData, _a), #_a));} b = false;}
#else
#define check_offset(_a)
#endif

void STSM_MultiViewShadowMap::execute(RenderContext* vRenderContext, const RenderData& vRenderData)
{
    if (!mpScene || !mpLight) return;

    __executePointGenerationPass(vRenderContext, vRenderData);
    __executeShadowMapPass(vRenderContext, vRenderData);
    __executeVisibilityPass(vRenderContext, vRenderData);
    mShadowMapPass.Time++;
}

void STSM_MultiViewShadowMap::renderUI(Gui::Widgets& widget)
{
    if (!mRectLightList.empty())
        widget.dropdown("Current Light", mRectLightList, mCurrentRectLightIndex);
    else
        widget.text("No Light in Scene");
    widget.checkbox("Jitter Area Light Camera", mVContronls.jitterAreaLightCamera);
    widget.var("Depth Bias", mSMData.depthBias, 0.000f, 0.1f, 0.0005f);
    widget.var("Sample Count", mJitterPattern.mSampleCount, 0u, 1000u, 1u);
    widget.var("PCF Radius", mPcfRadius, 0, 10, 1);
    widget.separator();
    widget.checkbox("Randomly Select Shadow Map", mVContronls.randomSelection);
    if (mVContronls.randomSelection)
    {
        widget.indent(20.0f);
        widget.var("Select Number", mVContronls.selectNum, 1u, mNumShadowMapPerFrame, 1u);
        widget.indent(-20.0f);
    }
}

void STSM_MultiViewShadowMap::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    _ASSERTE(mpScene);
    // find all rect area light
    mpLight = nullptr;
    uint32_t LightNum = mpScene->getLightCount();
    for (uint32_t i = 0; i < LightNum; ++i)
    {
        auto pLight = mpScene->getLight(i);
        if (pLight->getType() == LightType::Rect)
            mRectLightList.emplace_back(Gui::DropdownValue{ i, pLight->getName() });
    }
    _ASSERTE(mRectLightList.size() > 0);
    mCurrentRectLightIndex = mRectLightList[0].value;
    mpLight = mpScene->getLight(mCurrentRectLightIndex);

    // set light camera
    const auto& CameraSet = mpScene->getCameras();
    for (auto pCamera : CameraSet)
    {
        if (pCamera->getName() == "LightCamera")
        {
            mpLightCamera = pCamera;
            break;
        }
    }
    _ASSERTE(mpLightCamera);
    mpLightCamera->setUpVector(float3(1.0, 0.0, 0.0));
    mpLightCamera->setAspectRatio((float)mShadowMapPass.MapSize.x / (float)mShadowMapPass.MapSize.y);

    updateSamplePattern(); //default as halton
    updatePointGenerationPass();
}

void STSM_MultiViewShadowMap::createPointGenerationPassResource()
{
    Program::Desc ShaderDesc;
    ShaderDesc.addShaderLibrary(kPointGenerationFile).vsEntry("vsMain").psEntry("psMain");

    GraphicsProgram::SharedPtr pProgram = GraphicsProgram::create(ShaderDesc);

    RasterizerState::Desc RasterDesc;
    RasterDesc.setCullMode(mPointGenerationPass.CullMode);
    RasterizerState::SharedPtr pRasterState = RasterizerState::create(RasterDesc);

    DepthStencilState::Desc DepthStateDesc;
    DepthStateDesc.setDepthEnabled(false);
    DepthStateDesc.setDepthWriteMask(true);
    DepthStencilState::SharedPtr pDepthState = DepthStencilState::create(DepthStateDesc);

    mPointGenerationPass.pState = GraphicsState::create();
    mPointGenerationPass.pState->setProgram(pProgram);
    mPointGenerationPass.pState->setRasterizerState(pRasterState);
    mPointGenerationPass.pState->setDepthStencilState(pDepthState);
    mPointGenerationPass.pState->setBlendState(nullptr);
}

void STSM_MultiViewShadowMap::createShadowPassResource()
{
    ComputeProgram::SharedPtr pProgram = ComputeProgram::createFromFile(kShadowPassfile, "main");
    mShadowMapPass.pVars = ComputeVars::create(pProgram->getReflector());
    mShadowMapPass.pState = ComputeState::create();
    mShadowMapPass.pState->setProgram(pProgram);
}

void STSM_MultiViewShadowMap::updatePointGenerationPass()
{
    if (!mpScene || !mpLight || !mpLightCamera) return;

    // create camera behind light and calculate resolution
    const uint2 ShadowMapSize = mShadowMapPass.MapSize;
    float3 AreaLightCenter = getAreaLightCenterPos();
    float2 AreaLightSize = getAreaLightSize();
    float LightSize = std::max(AreaLightSize.x, AreaLightSize.y); // TODO: calculate actual light size
    float FovY = mpLightCamera->getFovY();
    float FovX = 2 * atan(tan(FovY * 0.5f) * ShadowMapSize.y / ShadowMapSize.x);
    float Fov = std::max(FovX, FovY);
    float Distance = LightSize * 0.5f / tan(Fov * 0.5f); // Point Generation Camera Distance

    float LightCameraNear = mpLightCamera->getNearPlane();
    float Scaling = ((LightCameraNear + Distance) / LightCameraNear); // Scaling of rasterization resolution
    // FIXME: the calculated scale seems too large, so I put a limitation
    Scaling = std::min(8.0f, Scaling);
    mPointGenerationPass.CoverMapSize = uint2(ShadowMapSize.x * Scaling, ShadowMapSize.y * Scaling);

    Camera::SharedPtr pTempCamera = Camera::create("TempCamera");
    pTempCamera->setPosition(AreaLightCenter - getAreaLightDir() * Distance);
    pTempCamera->setTarget(AreaLightCenter);
    pTempCamera->setUpVector(mpLightCamera->getUpVector());
    pTempCamera->setAspectRatio(1.0f); 
    pTempCamera->setFrameHeight(mpLightCamera->getFrameHeight());
    float FocalLength = fovYToFocalLength(Fov, mpLightCamera->getFrameHeight());
    pTempCamera->setFocalLength(FocalLength);
    mPointGenerationPass.CoverLightViewProjectMat = pTempCamera->getViewProjMatrix();

    Fbo::Desc FboDesc;
    FboDesc.setDepthStencilTarget(ResourceFormat::D32Float);
    Fbo::SharedPtr pFbo = Fbo::create2D(mPointGenerationPass.CoverMapSize.x, mPointGenerationPass.CoverMapSize.y, FboDesc);

    mPointGenerationPass.pState->setFbo(pFbo);

    auto pProgram = mPointGenerationPass.pState->getProgram();
    pProgram->addDefines(mpScene->getSceneDefines());
    mPointGenerationPass.pVars = GraphicsVars::create(pProgram->getReflector());

    // create append buffer to storage point list
    mPointGenerationPass.pPointAppendBuffer = Buffer::createStructured(pProgram.get(), "PointList", mPointGenerationPass.MaxPointNum);
}

void STSM_MultiViewShadowMap::createVisibilityPassResource()
{
    _ASSERTE(mNumShadowMapPerFrame <= _MAX_SHADOW_MAP_NUM);

    Program::DefineList Defines;
    Defines.add("SAMPLE_GENERATOR_TYPE", std::to_string(SAMPLE_GENERATOR_UNIFORM));

    mVisibilityPass.pFbo = Fbo::create();
    mVisibilityPass.pPass = FullScreenPass::create(kVisibilityPassfile, Defines);
    mVisibilityPass.mPassDataOffset = mVisibilityPass.pPass->getVars()->getParameterBlock("PerFrameCB")->getVariableOffset("gPass");
}

void STSM_MultiViewShadowMap::updateVisibilityVars()
{
    auto VisibilityVars = mVisibilityPass.pPass->getVars().getRootVar();//update vars

    VisibilityVars["gSTsmCompareSampler"] = mVisibilityPass.pLinearCmpSampler;
    VisibilityVars["PerFrameCB"]["gSTsmData"].setBlob(mSMData);
    VisibilityVars["PerFrameCB"]["gShadowMapData"].setBlob(mShadowMapPass.ShadowMapData);
    VisibilityVars["PerFrameCB"]["gTime"] = mShadowMapPass.Time;
    VisibilityVars["PerFrameCB"]["gRandomSelection"] = mVContronls.randomSelection;
    VisibilityVars["PerFrameCB"]["gSelectNum"] = mVContronls.selectNum;
    VisibilityVars["PerFrameCB"]["gTest"] = mpScene->getCamera()->getViewProjMatrix();
}

float3 STSM_MultiViewShadowMap::getAreaLightDir()
{
    assert(mpLight != nullptr);

    float3 AreaLightDir = float3(0, 0, 1);//use normal as direction
    // normal is axis-aligned, so no need to construct normal transform matrix
    float3 AreaLightDirW = normalize(mpLight->getData().transMat * float4(AreaLightDir, 0.0f)).xyz;

    return AreaLightDirW;
}

float3 STSM_MultiViewShadowMap::getAreaLightCenterPos()
{
    assert(mpLight != nullptr);

    float3 AreaLightCenter = float3(0, 0, 0);
    float4 AreaLightCenterPosH = mpLight->getData().transMat * float4(AreaLightCenter, 1.0f);
    float3 AreaLightCenterPosW = AreaLightCenterPosH.xyz * (1.0f / AreaLightCenterPosH.w);

    return AreaLightCenterPosW;
}

float2 STSM_MultiViewShadowMap::getAreaLightSize()
{
    assert(mpLight != nullptr);

    float3 XMin = float3(-1, 0, 0);
    float3 XMax = float3(1, 0, 0);
    float3 YMin = float3(0, -1, 0);
    float3 YMax = float3(0, 1, 0);

    float4 XMinH = mpLight->getData().transMat * float4(XMin, 1.0f);
    float4 XMaxH = mpLight->getData().transMat * float4(XMax, 1.0f);
    float4 YMinH = mpLight->getData().transMat * float4(YMin, 1.0f);
    float4 YMaxH = mpLight->getData().transMat * float4(YMax, 1.0f);

    float3 XMinW = XMinH.xyz * (1.0f / XMinH.w);
    float3 XMaxW = XMaxH.xyz * (1.0f / XMaxH.w);
    float3 YMinW = YMinH.xyz * (1.0f / YMinH.w);
    float3 YMaxW = YMaxH.xyz * (1.0f / YMaxH.w);

    float Width = distance(XMinH, XMaxH);
    float Height = distance(YMinW, YMaxW);

    return float2(Width, Height);
}

void STSM_MultiViewShadowMap::sampleLight()
{
    if (!mVContronls.jitterAreaLightCamera) sampleAreaPosW();
    else sampleWithDirectionFixed();
}

void STSM_MultiViewShadowMap::sampleWithDirectionFixed()
{
    _ASSERTE(mpLightCamera);

    float3 LightDir = getAreaLightDir();//normalized dir

    for (uint i = 0; i < mNumShadowMapPerFrame; ++i)
    {
        float2 jitteredPos = getJitteredSample();
        float4 SamplePos = float4(jitteredPos, 0.f, 1.f);//Local space
        SamplePos = mpLight->getData().transMat * SamplePos;
        float3 LookAtPos = SamplePos.xyz + LightDir;//todo:maybe have error

        mpLightCamera->setPosition(SamplePos.xyz);
        mpLightCamera->setTarget(LookAtPos);

        glm::mat4 VP = mpLightCamera->getViewProjMatrix();
        mShadowMapPass.ShadowMapData.allGlobalMat[i] = VP;
    }
}

void STSM_MultiViewShadowMap::sampleAreaPosW()
{
    _ASSERTE(mpLightCamera);

    float3 EyePosBehindAreaLight = calacEyePosition();
    float4 SamplePosition = float4(EyePosBehindAreaLight, 1);
    SamplePosition = mpLight->getData().transMat * SamplePosition;

    float3 LightDir = getAreaLightDir();//normalized dir
    float3 LookAtPos = SamplePosition.xyz + LightDir;//todo:maybe have error

    mpLightCamera->setPosition(SamplePosition.xyz);//todo : if need to change look at target
    mpLightCamera->setTarget(LookAtPos);

    for (uint i = 0; i < mNumShadowMapPerFrame; ++i)
    {
        mShadowMapPass.ShadowMapData.allGlobalMat[i] = mpLightCamera->getViewProjMatrix();
    }
}

void STSM_MultiViewShadowMap::updateSamplePattern()
{
    mJitterPattern.mpSampleGenerator = createSamplePattern(mJitterPattern.mSamplePattern, mJitterPattern.mSampleCount);
    if (mJitterPattern.mpSampleGenerator) mJitterPattern.mSampleCount = mJitterPattern.mpSampleGenerator->getSampleCount();
}

float2 STSM_MultiViewShadowMap::getJitteredSample(bool isScale)
{
    float2 jitter = float2(0.f, 0.f);
    if (mJitterPattern.mpSampleGenerator)
    {
        jitter = mJitterPattern.mpSampleGenerator->next();

        if (isScale) jitter *= mJitterPattern.scale;
    }
    return jitter;
}

float3 STSM_MultiViewShadowMap::calacEyePosition()
{
    float fovy = mpLightCamera->getFovY();//todo get fovy
    float halfFovy = fovy / 2.0f;

    float maxAreaL = 2.0f;//Local Space

    float dLightCenter2EyePos = maxAreaL / (2.0f * std::tan(halfFovy));

    float3 LightCenter = float3(0, 0, 0);//Local Space
    float3 LightNormal = float3(0, 0, 1);//Local Space
    float3 EyePos = LightCenter - (dLightCenter2EyePos * LightNormal);

    return EyePos;
}

void STSM_MultiViewShadowMap::__executePointGenerationPass(RenderContext* vRenderContext, const RenderData& vRenderData)
{
    if (!mpScene) return;
    if (mPointGenerationPass.Regenerate)
    {
        //mPointGenerationPass.Regenerate = false;

        auto pCounterBuffer = mPointGenerationPass.pPointAppendBuffer->getUAVCounter();
        pCounterBuffer->setElement(0, (uint32_t)0);
        auto pFbo = mPointGenerationPass.pState->getFbo();
        auto pDebug = Texture::create2D(mPointGenerationPass.CoverMapSize.x, mPointGenerationPass.CoverMapSize.y, ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceBindFlags::RenderTarget);
        auto x = pDebug->getWidth();
        auto y = pDebug->getHeight();
        pFbo->attachColorTarget(pDebug, 0);
        vRenderContext->clearFbo(pFbo.get(), float4(0.0, 0.0, 0.0, 0.0), 0.0, 0);

        mPointGenerationPass.pVars["PointList"] = mPointGenerationPass.pPointAppendBuffer;
        mPointGenerationPass.pVars["PerFrameCB"]["gViewProjectMat"] = mPointGenerationPass.CoverLightViewProjectMat;

        mpScene->rasterize(vRenderContext, mPointGenerationPass.pState.get(), mPointGenerationPass.pVars.get(), RasterizerState::CullMode::None);

        // FIXME: delete this point list size logging
        uint32_t* pNum = (uint32_t*)pCounterBuffer->map(Buffer::MapType::Read);
        std::cout << "Point List Size: " << *pNum << "\n";
    }
}

void STSM_MultiViewShadowMap::__executeShadowMapPass(RenderContext* vRenderContext, const RenderData& vRenderData)
{
    // clear
    const auto& pShadowMapSet = vRenderData[kShadowMapSet]->asTexture();
    vRenderContext->clearUAV(pShadowMapSet->getUAV().get(), uint4(0, 0, 0, 0));

    // update
    sampleLight();
    // FIXME: UAV counter has no shader resources flag so can not be bound. How to fix?
    //mShadowMapPass.pVars->setBuffer("gNumPoint", mPointGenerationPass.pPointAppendBuffer->getUAVCounter());
    mShadowMapPass.pVars["PerFrameCB"]["gShadowMapData"].setBlob(mShadowMapPass.ShadowMapData);
    auto pCounterBuffer = mPointGenerationPass.pPointAppendBuffer->getUAVCounter();
    uint32_t* pNum = (uint32_t*)pCounterBuffer->map(Buffer::MapType::Read);
    mShadowMapPass.pVars["PerFrameCB"]["gNumPoint"] = *pNum;
    mShadowMapPass.pVars->setBuffer("gPointList", mPointGenerationPass.pPointAppendBuffer);
    mShadowMapPass.pVars->setTexture("gOutputShadowMap", pShadowMapSet);

    // execute
    uint32_t NumGroupX = div_round_up((int)mPointGenerationPass.MaxPointNum, _SHADOW_MAP_SHADER_THREAD_NUM_X * _SHADOW_MAP_SHADER_POINT_PER_THREAD);
    uint32_t NumGroupY = div_round_up((int)mNumShadowMapPerFrame, _SHADOW_MAP_SHADER_THREAD_NUM_Y * _SHADOW_MAP_SHADER_MAP_PER_THREAD);
    vRenderContext->dispatch(mShadowMapPass.pState.get(), mShadowMapPass.pVars.get(), { NumGroupX, NumGroupY, 1 });
}

void STSM_MultiViewShadowMap::__executeVisibilityPass(RenderContext* vRenderContext, const RenderData& vRenderData)
{
    const auto pCamera = mpScene->getCamera().get();
    const auto& pVisibility = vRenderData[kVisibility]->asTexture();
    const auto& pShadowMapSet = vRenderData[kShadowMapSet]->asTexture();
    const auto& pDepth = vRenderData[kDepth]->asTexture();
    const auto& pDebug = vRenderData[kDebug]->asTexture();

    mVisibilityPass.pFbo->attachColorTarget(pVisibility, 0);
    mVisibilityPass.pFbo->attachColorTarget(pDebug, 1);
    vRenderContext->clearFbo(mVisibilityPass.pFbo.get(), float4(1, 0, 0, 0), 1, 0, FboAttachmentType::All);//clear visibility buffer
    updateVisibilityVars();
    mVisibilityPassData.camInvViewProj = pCamera->getInvViewProjMatrix();
    mVisibilityPassData.screenDim = uint2(mVisibilityPass.pFbo->getWidth(), mVisibilityPass.pFbo->getHeight());
    mVisibilityPass.pPass["PerFrameCB"][mVisibilityPass.mPassDataOffset].setBlob(mVisibilityPassData);
    mVisibilityPass.pPass["gShadowMapSet"] = pShadowMapSet;
    mVisibilityPass.pPass["gDepth"] = pDepth;
    mVisibilityPass.pPass["PerFrameCB"]["PcfRadius"] = mPcfRadius;
    mVisibilityPass.pPass->execute(vRenderContext, mVisibilityPass.pFbo); // Render visibility buffer
}
