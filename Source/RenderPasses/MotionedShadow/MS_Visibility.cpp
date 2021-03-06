#include "MS_Visibility.h"

namespace
{
    const char kDesc[] = "Calculate Visibility, Shadow Motion Vector and Reliability.";

    const std::string kProgramFile = "RenderPasses/MotionedShadow/MS_Visibility.slang";
    const std::string shaderModel = "6_2";

    // Name = pass channel name; TexName = name in shader
    //const std::string kVisibilityName = "visibility";
    const ChannelList kInChannels =
    {
        { "SMs", "gShadowMapSet",  "Light Space Depth/Shadow Map Set", true /* optional */, ResourceFormat::R32Float},
        { "Ids", "gIDSet",  "Light Space Instance ID Set", true /* optional */, ResourceFormat::R32Uint},
        { "LOffs", "gLightOffset",  "Area Light Postion Offset", true /* optional */, ResourceFormat::RG32Float},
        { "PosW", "gPosW",  "World Position", true /* optional */, ResourceFormat::RGBA32Float},
    };
    const ChannelList kOutChannels =
    {
        { "vis", "gVisibility",  "Scene Visibility", true /* optional */, ResourceFormat::RGBA32Float},
        { "smv", "gShadowMotionVector", "Shadow Motion Vector", true/* optional */, ResourceFormat::RGBA32Float},  
        { "smvr", "gReliability", "Shadow Motion Vector Reliability", true/* optional */, ResourceFormat::RGBA32Float},  
        { "debug", "gDebug", "Data for Debug", true/* optional */, ResourceFormat::RGBA32Float},  
    };

    const std::string kPrePosW = "PPosW";
    const std::string kTexPrePosW = "gPrePosW";

    const std::string dkGridSize = "GridSize";
}

void MS_Visibility::registerScriptBindings(pybind11::module& m)
{
    pybind11::class_<MS_Visibility, RenderPass, MS_Visibility::SharedPtr> VisPass(m, "MS_Visibility");

    VisPass.def_property("UseSMV", &MS_Visibility::getUseSMV, &MS_Visibility::setUseSMV);
}

MS_Visibility::SharedPtr MS_Visibility::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    MS_Visibility::SharedPtr pMS_V = SharedPtr(new MS_Visibility);
    return pMS_V;
}

MS_Visibility::MS_Visibility()
{
    Program::Desc desc;
    desc.addShaderLibrary(kProgramFile).vsEntry("vsMain").psEntry("psMain");
    desc.setShaderModel(shaderModel);
    mpProgram = GraphicsProgram::create(desc);

    DepthStencilState::Desc DepthStateDesc;
    DepthStateDesc.setDepthEnabled(true);
    DepthStateDesc.setDepthWriteMask(true);
    DepthStencilState::SharedPtr pDepthState = DepthStencilState::create(DepthStateDesc);

    mpGraphicsState = GraphicsState::create();
    mpGraphicsState->setProgram(mpProgram);
    mpGraphicsState->setDepthStencilState(pDepthState);
}

std::string MS_Visibility::getDesc() { return kDesc; }

RenderPassReflection MS_Visibility::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector; 

    //addRenderPassInputs(reflector, kInChannels, Resource::BindFlags::ShaderResource);
    reflector.addInternal(kPrePosW, "previous world position").bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource).format(ResourceFormat::RGBA32Float).texture2D(0, 0);

    for (const auto& channel : kInChannels)
    {
        auto& buffer = reflector.addInput(channel.name,channel.desc);
        buffer.texture2D(0, 0, 0, 1, 0);
        buffer.flags(RenderPassReflection::Field::Flags::Optional);
    }
    addRenderPassOutputs(reflector, kOutChannels, Resource::BindFlags::RenderTarget);

    return reflector;
}

void MS_Visibility::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (mControls.UseSMV)
    {
        mpProgram->addDefine("USE_SMV");
    }
    else
    {
        mpProgram->removeDefine("USE_SMV");
    }

    const InternalDictionary& InterDict = renderData.getDictionary();

    // attach and clear output textures to fbo
    for (size_t i = 0; i < kOutChannels.size(); ++i)
    {
        Texture::SharedPtr pTex = renderData[kOutChannels[i].name]->asTexture();
        mpFbo->attachColorTarget(pTex, uint32_t(i));
    }
    pRenderContext->clearFbo(mpFbo.get(), float4(0), 1.f, 0, FboAttachmentType::All);

    if (mpScene == nullptr) return; //early return
    if (!mpVars) { mpVars = GraphicsVars::create(mpProgram.get()); }    //assure vars isn't empty

    // set input textures
    for (const auto& channel : kInChannels)
    {
        auto pTex = renderData[channel.name]->asTexture();
        if (pTex == nullptr) // light offset may be empty
        {
            pTex = Texture::create2D(mpFbo->getWidth(), mpFbo->getHeight(), channel.format);
            //pRenderContext->clearTexture(pTex.get());
        }
        mpVars[channel.texname] = pTex;
    }

    // set internal textures
    const auto& pCurPos = renderData["PosW"]->asTexture(); // FIXME
    const auto& pPrevPos = renderData[kPrePosW]->asTexture();
    mpVars[kTexPrePosW] = pPrevPos;

    // set shader data(need update every frame)
    __preparePassData(InterDict);
    if (InterDict.keyExists("ShadowMapData"))
    {
        SShadowMapData SMD = InterDict["ShadowMapData"];
        mpVars["PerFrameCB"]["gShadowMapData"].setBlob(SMD);
    }
    if (InterDict.keyExists("LightData"))
    {
        SLightData LD = InterDict["LightData"];
        mpVars["PerFrameCB"]["gLightData"].setBlob(LD);
    }
    mpVars["PerFrameCB"][mPassDataOffset].setBlob(mPassData);

    mpGraphicsState->setFbo(mpFbo);

    mpScene->rasterize(pRenderContext, mpGraphicsState.get(), mpVars.get());

    // update preposw
    pRenderContext->blit(pCurPos->getSRV(), pPrevPos->getRTV());
}

void MS_Visibility::renderUI(Gui::Widgets& widget)
{
    static float MVscale = 1.;
    widget.var("Motion Vector Scale Factor", MVscale,(0.1f),(10.f),(0.01f));
    mPassData.MVScale = float2(MVscale, MVscale);

    widget.checkbox("Use Shadow Motion Vector", mControls.UseSMV);
}

void MS_Visibility::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    assert(pScene);
    mpScene = pScene;
    mpProgram->addDefines(mpScene->getSceneDefines());
    mpVars = GraphicsVars::create(mpProgram.get());
    mPassDataOffset = mpVars->getParameterBlock("PerFrameCB")->getVariableOffset("Data");

    if (!(mpLight = mpScene->getLightByName("RectLight")))
    {
        mpLight = (mpScene && mpScene->getLightCount() ? mpScene->getLight(0) : nullptr);
    }
    assert(mpLight);
}

void MS_Visibility::compile(RenderContext* pContext, const CompileData& compileData)
{
    Fbo::Desc FboDesc;
    FboDesc.setDepthStencilTarget(ResourceFormat::D32Float);
    uint2 Dim = compileData.defaultTexDims;

    mpFbo = Fbo::create2D(Dim.x, Dim.y, FboDesc);
}

void MS_Visibility::__preparePassData(const InternalDictionary& vDict)
{
    Camera::SharedConstPtr pCamera = mpScene->getCamera();
    uint2 ScrDim = uint2(mpFbo->getWidth(), mpFbo->getHeight());
    Helper::ShadowVPHelper SVPHelper(pCamera, mpLight, 1);
    //Helper::ShadowVPHelper SVPHelper(pCamera, mpLight, (float)ScrDim.x / (float)ScrDim.y);

    mPassData.CameraInvVPMat = pCamera->getInvViewProjMatrix();
    mPassData.ScreenDim = ScrDim;
    mPassData.ShadowProj = SVPHelper.getProj();
    mPassData.ShadowVP = SVPHelper.getVP();
    mPassData.InvShadowVP = inverse(SVPHelper.getVP());
    mPassData.ShadowView = SVPHelper.getView();
    mPassData.InvShadowView = inverse(SVPHelper.getView());
    mPassData.ShadowProj= SVPHelper.getProj();
    mPassData.InvShadowProj= inverse(SVPHelper.getProj());
    mPassData.PreCamVP = pCamera->getProjMatrix() * pCamera->getPrevViewMatrix();
    //__prepareLightData(vDict);
}

//void MS_Visibility::__prepareLightData(const InternalDictionary& vDict)
//{
//    if (mpLight->getType() == LightType::Point)
//    {
//        PointLight* pPL = (PointLight*)mpLight.get();
//        mPassData.LightPos = pPL->getWorldPosition();
//        mPassData.LightGridSize = 1;
//        mPassData.HalfLightSize = float2(0,0);
//    }
//    else if (mpLight->getType() == LightType::Rect)
//    {
//        RectLight* pPL = (RectLight*)mpLight.get();
//        mPassData.LightPos = pPL->getCenter();
//        if (vDict.keyExists(dkGridSize))
//        {
//            mLightGridSize = 4;
//            //mLightGridSize = vDict[dkGridSize];
//            mPassData.LightGridSize = mLightGridSize;
//        }
//        else
//        {
//            mPassData.LightGridSize = 4;
//        }
//        mPassData.HalfLightSize = pPL->getSize()*float2(0.5);
//    }
//    
//}
