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
#include "ForwardLightingPass.h"

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("ForwardLightingPass", "Computes direct and indirect illumination and applies shadows for the current scene", ForwardLightingPass::create);
}

const char* ForwardLightingPass::kDesc = "The pass computes the lighting results for the current scene. It will compute direct-illumination, indirect illumination from the light-probe and apply shadows (if a visibility map is provided).\n"
"The pass can output the world-space normals and screen-space motion vectors, both are optional";

namespace
{
    const std::string kDepth = "depth";
    const std::string kColor = "color";
    const std::string kMotionVecs = "motionVecs";
    const std::string kNormals = "normals";
    const std::string kVisBuffer = "visibilityBuffer";

    const std::string kSampleCount = "sampleCount";
    const std::string kSuperSampling = "enableSuperSampling";
}

static CPUSampleGenerator::SharedPtr createSamplePattern(ForwardLightingPass::SamplePattern type, uint32_t sampleCount)
{
    switch (type)
    {
    case ForwardLightingPass::SamplePattern::Center:
        return nullptr;
        break;
    case ForwardLightingPass::SamplePattern::DirectX:
        return DxSamplePattern::create(sampleCount);
        break;
    case ForwardLightingPass::SamplePattern::Halton:
        return HaltonSamplePattern::create(sampleCount);
        break;
    case ForwardLightingPass::SamplePattern::Stratitied:
        return StratifiedSamplePattern::create(sampleCount);
        break;
    default:
        should_not_get_here();
        return nullptr;
    }
}

ForwardLightingPass::SharedPtr ForwardLightingPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    auto pThis = SharedPtr(new ForwardLightingPass());
    pThis->setColorFormat(ResourceFormat::RGBA32Float).setMotionVecFormat(ResourceFormat::RG16Float).setNormalMapFormat(ResourceFormat::RGBA8Unorm).setSampleCount(1).usePreGeneratedDepthBuffer(true);

    for (const auto& [key, value] : dict)
    {
        if (key == kSampleCount) pThis->setSampleCount(value);
        else if (key == kSuperSampling) pThis->setSuperSampling(value);
        else logWarning("Unknown field '" + key + "' in a ForwardLightingPass dictionary");
    }

    return pThis;
}

Dictionary ForwardLightingPass::getScriptingDictionary()
{
    Dictionary d;
    d[kSampleCount] = mSampleCount;
    d[kSuperSampling] = mEnableSuperSampling;
    return d;
}

ForwardLightingPass::ForwardLightingPass()
{
    GraphicsProgram::SharedPtr pProgram = GraphicsProgram::createFromFile("RenderPasses/ForwardLightingPass/ForwardLightingPass.slang", "", "ps");
    mpState = GraphicsState::create();
    mpState->setProgram(pProgram);

    mpFbo = Fbo::create();

    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
    mpDsNoDepthWrite = DepthStencilState::create(dsDesc);

    //lg
    updateSamplePattern();
    createDebugDrawderResource();
    createPointDrawderResource();
}

RenderPassReflection ForwardLightingPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addInput(kVisBuffer, "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addInputOutput(kColor, "Color texture").format(mColorFormat).texture2D(0, 0, mSampleCount);

    auto& depthField = mUsePreGenDepth ? reflector.addInputOutput(kDepth, "Pre-initialized depth-buffer") : reflector.addOutput(kDepth, "Depth buffer");
    depthField.bindFlags(Resource::BindFlags::DepthStencil).texture2D(0, 0, mSampleCount);

    if (mNormalMapFormat != ResourceFormat::Unknown)
    {
        reflector.addOutput(kNormals, "World-space shading normal, [0,1] range. Don't forget to transform it to [-1, 1] range").format(mNormalMapFormat).texture2D(0, 0, mSampleCount);
    }

    if (mMotionVecFormat != ResourceFormat::Unknown)
    {
        reflector.addOutput(kMotionVecs, "Screen-space motion vectors").format(mMotionVecFormat).texture2D(0, 0, mSampleCount);
    }

    return reflector;
}

void ForwardLightingPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;

    if (mpScene) mpState->getProgram()->addDefines(mpScene->getSceneDefines());

    mpVars = GraphicsVars::create(mpState->getProgram()->getReflector());

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    setSampler(Sampler::create(samplerDesc));

    //lg
    updateSamplePattern();
    mAreaLight = mpScene && mpScene->getLightCount() >= 1 ? mpScene->getLight(1) : nullptr; //fixme:only calculate first light's shadow map,0:point light,1:area light
    updateLightCamera();
    updateDebugDrawerResource();

}

void ForwardLightingPass::initDepth(const RenderData& renderData)
{
    const auto& pTexture = renderData[kDepth]->asTexture();

    if (pTexture)
    {
        mpState->setDepthStencilState(mpDsNoDepthWrite);
        mpFbo->attachDepthStencilTarget(pTexture);
    }
    else
    {
        mpState->setDepthStencilState(nullptr);
        if (mpFbo->getDepthStencilTexture() == nullptr)
        {
            auto pDepth = Texture::create2D(mpFbo->getWidth(), mpFbo->getHeight(), ResourceFormat::D32Float, 1, 1, nullptr, Resource::BindFlags::DepthStencil);
            mpFbo->attachDepthStencilTarget(pDepth);
        }
    }
}

void ForwardLightingPass::initFbo(RenderContext* pContext, const RenderData& renderData)
{
    mpFbo->attachColorTarget(renderData[kColor]->asTexture(), 0);
    mpFbo->attachColorTarget(renderData[kNormals]->asTexture(), 1);
    mpFbo->attachColorTarget(renderData[kMotionVecs]->asTexture(), 2);

    for (uint32_t i = 1; i < 3; i++)
    {
        const auto& pRtv = mpFbo->getRenderTargetView(i).get();
        if (pRtv->getResource() != nullptr) pContext->clearRtv(pRtv, float4(0));
    }

    // TODO Matt (not really matt, just need to fix that since if depth is not bound the pass crashes
    if (mUsePreGenDepth == false) pContext->clearDsv(renderData[kDepth]->asTexture()->getDSV().get(), 1, 0);
}

void ForwardLightingPass::updateLightCamera()
{
    auto getLightCamera = [this]() {//get Light Camera,if not exist ,return default camera
        const auto& Cameras = mpScene->getCameras();
        for (const auto& Camera : Cameras) if (Camera->getName() == "LightCamera") return Camera;
        return Cameras[0];
    };

    mpLightCamera = getLightCamera();
}

void ForwardLightingPass::createDebugDrawderResource()
{
    DebugDrawerData.mpProgram = GraphicsProgram::createFromFile("RenderPasses/ForwardLightingPass/DebugShow.slang", "vsMain", "psMain");
    DebugDrawerData.mpGraphicsState = GraphicsState::create();
    DebugDrawerData.mpGraphicsState->setProgram(DebugDrawerData.mpProgram);

    DebugDrawerData.mpProgramVars = GraphicsVars::create(DebugDrawerData.mpProgram->getReflector());


    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthEnabled(true).setDepthFunc(ComparisonFunc::Less);
    DepthStencilState::SharedPtr pDepthTestDS = DepthStencilState::create(dsDesc);
    DebugDrawerData.mpGraphicsState->setDepthStencilState(pDepthTestDS);

    mpDebugDrawer = TriangleDebugDrawer::create();
    mpDebugDrawer->clear();
    mpDebugDrawer->setColor(float3(0, 1, 0));

    DebugDrawer::Quad quad;
    quad[0] = float3(-1, -1, 0); //LL
    quad[1] = float3(-1, 1, 0);  //LU
    quad[2] = float3(1, 1, 0);   //RU
    quad[3] = float3(1, -1, 0);  //RL

    mpDebugDrawer->addPoint(quad[3], float2(1, 0));
    mpDebugDrawer->addPoint(quad[1], float2(0, 1));
    mpDebugDrawer->addPoint(quad[0], float2(0, 0));
    mpDebugDrawer->addPoint(quad[2] + float3(0, 0.2, 0), float2(1, 1));
    mpDebugDrawer->addPoint(quad[1] + float3(0, 0.2, 0), float2(0, 1));
    mpDebugDrawer->addPoint(quad[3] + float3(0, 0.2, 0), float2(1, 0));
    //mpDebugDrawer->addPoint(quad[3]);
    //mpDebugDrawer->addQuad(quad);
}

void ForwardLightingPass::updateDebugDrawerResource()
{

    DebugDrawer::Quad quad;
    quad[0] = float3(-1, -1, 0);
    quad[1] = float3(-1, 1, 0);
    quad[2] = float3(1, 1, 0);
    quad[3] = float3(1, -1, 0);

    float fovy = mpLightCamera->getFovY();//todo get fovy
    float halfFovy = fovy / 2.0f;
    float maxAreaL = 2.0f;//Local Space

    float dLightCenter2EyePos = maxAreaL / (2.0f * std::tan(halfFovy));

    float3 LightCenter = float3(0, 0, 0);//Local Space
    float3 LightNormal = float3(0, 0, 1);//Local Space
    float3 EyePos = LightCenter - (dLightCenter2EyePos * LightNormal);

    /*mpDebugDrawer->addPoint(quad[0]);
    mpDebugDrawer->addPoint(quad[1]);
    mpDebugDrawer->addPoint(quad[3]);*/
    //mpDebugDrawer->addPoint(quad[3]);
    /*mpDebugDrawer->addLine(EyePos, quad[0]);
    mpDebugDrawer->addLine(EyePos, quad[1]);
    mpDebugDrawer->addLine(EyePos, quad[2]);
    mpDebugDrawer->addLine(EyePos, quad[3]);*/
}

void ForwardLightingPass::updateSamplePattern()
{
    mJitterPattern.mpSampleGenerator = createSamplePattern(mJitterPattern.mSamplePattern, mJitterPattern.mSampleCount);
    if (mJitterPattern.mpSampleGenerator) mJitterPattern.mSampleCount = mJitterPattern.mpSampleGenerator->getSampleCount();
}

Falcor::float2 ForwardLightingPass::getJitteredSample(bool isScale /*= true*/)
{
    float2 jitter = float2(0.f, 0.f);
    if (mJitterPattern.mpSampleGenerator)
    {
        jitter = mJitterPattern.mpSampleGenerator->next();

        if (isScale) jitter *= mJitterPattern.scale;
    }
    return jitter;
}

void ForwardLightingPass::createPointDrawderResource()
{
    PointDrawerData.mpProgram = GraphicsProgram::createFromFile("RenderPasses/ForwardLightingPass/DebugShow.slang", "vsMain", "psMain");
    PointDrawerData.mpGraphicsState = GraphicsState::create();
    PointDrawerData.mpGraphicsState->setProgram(PointDrawerData.mpProgram);
    PointDrawerData.mpProgramVars = GraphicsVars::create(PointDrawerData.mpProgram->getReflector());

    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthEnabled(true).setDepthFunc(ComparisonFunc::Less);
    DepthStencilState::SharedPtr pDepthTestDS = DepthStencilState::create(dsDesc);
    PointDrawerData.mpGraphicsState->setDepthStencilState(pDepthTestDS);

    mPointDrawer = PointDrawer::create();
    mPointDrawer->clear();
    mPointDrawer->setColor(float3(1, 0, 0));

    std::vector<float2> Samples;

    for (uint32_t i = 0; i < mJitterPattern.mSampleCount; ++i)
    {
        Samples.push_back(getJitteredSample());
    }

   for (uint32_t k = 0; k < Samples.size(); ++k)
   {
       mPointDrawer->addPoint(float3(Samples[k], 0));
   }
}

void ForwardLightingPass::execute(RenderContext* pContext, const RenderData& renderData)
{
    initDepth(renderData);
    initFbo(pContext, renderData);

    if (!mpScene) return;

    // Update env map lighting
    const auto& pEnvMap = mpScene->getEnvMap();
    if (pEnvMap && (!mpEnvMapLighting || mpEnvMapLighting->getEnvMap() != pEnvMap))
    {
        mpEnvMapLighting = EnvMapLighting::create(pContext, pEnvMap);
        mpEnvMapLighting->setShaderData(mpVars["gEnvMapLighting"]);
        mpState->getProgram()->addDefine("_USE_ENV_MAP");
    }
    else if (!pEnvMap)
    {
        mpEnvMapLighting = nullptr;
        mpState->getProgram()->removeDefine("_USE_ENV_MAP");
    }

    mpVars["PerFrameCB"]["gRenderTargetDim"] = float2(mpFbo->getWidth(), mpFbo->getHeight());
    mpVars->setTexture(kVisBuffer, renderData[kVisBuffer]->asTexture());

    mpState->setFbo(mpFbo);
    mpScene->rasterize(pContext, mpState.get(), mpVars.get());

    //***************************************************************
    //lg render area light
    DebugDrawerData.mpGraphicsState->setFbo(mpFbo);
    const auto pCamera = mpScene->getCamera().get();
    DebugDrawerData.mpProgramVars["PerFrameCB"]["ViewProj"] = pCamera->getViewProjMatrix();
    DebugDrawerData.mpProgramVars["PerFrameCB"]["LightWorldMat"] = mAreaLight->getData().transMat;
    mpDebugDrawer->render(pContext, DebugDrawerData.mpGraphicsState.get(), DebugDrawerData.mpProgramVars.get(), pCamera);

    //render points
    /*PointDrawerData.mpGraphicsState->setFbo(mpFbo);
    PointDrawerData.mpProgramVars["PerFrameCB"]["ViewProj"] = pCamera->getViewProjMatrix();
    PointDrawerData.mpProgramVars["PerFrameCB"]["LightWorldMat"] = mAreaLight->getData().transMat;
    mPointDrawer->render(pContext, PointDrawerData.mpGraphicsState.get(), PointDrawerData.mpProgramVars.get(), pCamera);*/
}

void ForwardLightingPass::renderUI(Gui::Widgets& widget)
{
    static const Gui::DropdownList kSampleCountList =
    {
        { 1, "1" },
        { 2, "2" },
        { 4, "4" },
        { 8, "8" },
    };

    if (widget.dropdown("Sample Count", kSampleCountList, mSampleCount))              setSampleCount(mSampleCount);
    if (mSampleCount > 1 && widget.checkbox("Super Sampling", mEnableSuperSampling))  setSuperSampling(mEnableSuperSampling);
}

ForwardLightingPass& ForwardLightingPass::setColorFormat(ResourceFormat format)
{
    mColorFormat = format;
    mPassChangedCB();
    return *this;
}

ForwardLightingPass& ForwardLightingPass::setNormalMapFormat(ResourceFormat format)
{
    mNormalMapFormat = format;
    mPassChangedCB();
    return *this;
}

ForwardLightingPass& ForwardLightingPass::setMotionVecFormat(ResourceFormat format)
{
    mMotionVecFormat = format;
    if (mMotionVecFormat != ResourceFormat::Unknown)
    {
        mpState->getProgram()->addDefine("_OUTPUT_MOTION_VECTORS");
    }
    else
    {
        mpState->getProgram()->removeDefine("_OUTPUT_MOTION_VECTORS");
    }
    mPassChangedCB();
    return *this;
}

ForwardLightingPass& ForwardLightingPass::setSampleCount(uint32_t samples)
{
    mSampleCount = samples;
    mPassChangedCB();
    return *this;
}

ForwardLightingPass& ForwardLightingPass::setSuperSampling(bool enable)
{
    mEnableSuperSampling = enable;
    if (mEnableSuperSampling)
    {
        mpState->getProgram()->addDefine("INTERPOLATION_MODE", "sample");
    }
    else
    {
        mpState->getProgram()->removeDefine("INTERPOLATION_MODE");
    }

    return *this;
}

ForwardLightingPass& ForwardLightingPass::usePreGeneratedDepthBuffer(bool enable)
{
    mUsePreGenDepth = enable;
    mPassChangedCB();
    mpState->setDepthStencilState(mUsePreGenDepth ? mpDsNoDepthWrite : nullptr);

    return *this;
}

ForwardLightingPass& ForwardLightingPass::setSampler(const Sampler::SharedPtr& pSampler)
{
    mpVars->setSampler("gSampler", pSampler);
    return *this;
}
