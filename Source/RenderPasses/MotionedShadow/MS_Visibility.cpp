#include "MS_Visibility.h"

namespace
{
    const char kDesc[] = "Create Shadow Map with ID";

    const std::string kProgramFile = "RenderPasses/MotionedShadow/MS_Visibility.slang";
    const std::string shaderModel = "6_2";

    // Name = pass channel name; TexName = name in shader
    //const std::string kVisibilityName = "visibility";
    const ChannelList kInChannels =
    {
        { "SM", "gSM",  "Light Space Depth/Shadow Map", true /* optional */, ResourceFormat::D32Float},
        { "Id", "gID",  "Light Space Instance ID", true /* optional */, ResourceFormat::RGBA32Uint},
    };
    const ChannelList kOutChannels =
    {
        { "vis", "gVisibility",  "Scene Visibility", true /* optional */, ResourceFormat::RGBA32Float},
        { "smv", "gShadowMotionVector", "Shadow Motion Vector", true/* optional */, ResourceFormat::RGBA32Float},  
        { "debug", "gLightSpaceOBJs", "Light Space Objects Visualize", true/* optional */, ResourceFormat::RGBA32Float},  
    };

    const std::string kDepthName = "depth";
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

    mpFbo = Fbo::create();
}

std::string MS_Visibility::getDesc() { return kDesc; }

RenderPassReflection MS_Visibility::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    addRenderPassInputs(reflector, kInChannels, Resource::BindFlags::ShaderResource);
    addRenderPassOutputs(reflector, kOutChannels, Resource::BindFlags::RenderTarget);
    reflector.addOutput(kDepthName, "Depth buffer").format(ResourceFormat::D32Float).bindFlags(Resource::BindFlags::DepthStencil);

    return reflector;
}

void MS_Visibility::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // attach and clear output textures to fbo
    for (size_t i = 0; i < kOutChannels.size(); ++i)
    {
        Texture::SharedPtr pTex = renderData[kOutChannels[i].name]->asTexture();
        mpFbo->attachColorTarget(pTex, uint32_t(i));
    }
    const auto& pDepth = renderData[kDepthName]->asTexture();
    mpFbo->attachDepthStencilTarget(pDepth);

    mpGraphicsState->setFbo(mpFbo);
    pRenderContext->clearDsv(pDepth->getDSV().get(), 1, 0);
    pRenderContext->clearFbo(mpFbo.get(), float4(0), 1.f, 0, FboAttachmentType::Color);
    if (mpScene == nullptr) return; //early return
    if (!mpVars) { mpVars = GraphicsVars::create(mpProgram.get()); }    //assure vars isn't empty

    // set input texture vars
    for (const auto& channel : kInChannels)
    {
        Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
        mpVars[channel.texname] = pTex;
    }

    // set shader data(need update every frame)
    auto pCamera = mpScene->getCamera();
    mPassData.CameraInvVPMat = pCamera->getInvViewProjMatrix();
    mPassData.ScreenDim = uint2(mpFbo->getWidth(), mpFbo->getHeight());
    //mPassData.ShadowVP = Helper::getShadowVP(pCamera.get(), mpLight.get());
    Helper::getShadowVPAndInv(pCamera.get(), mpLight.get(), (float)mPassData.ScreenDim.x/(float)mPassData.ScreenDim.y, mPassData.ShadowVP, mPassData.InvShadowVP);
    mPassData.PreCamVP = pCamera->getProjMatrix()*pCamera->getPrevViewMatrix();
    if (mpLight->getType() == LightType::Point)
    {
        PointLight* pPL = (PointLight*)mpLight.get();
        mPassData.LightPos = pPL->getWorldPosition();
    }
    else
    {
        assert(false);
    }
    mpVars["PerFrameCB"][mPassDataOffset].setBlob(mPassData);

    mpGraphicsState->setFbo(mpFbo);

    mpScene->rasterize(pRenderContext, mpGraphicsState.get(), mpVars.get());
}

void MS_Visibility::renderUI(Gui::Widgets& widget)
{
}

void MS_Visibility::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    assert(pScene);
    mpScene = pScene;
    mpProgram->addDefines(mpScene->getSceneDefines());
    mpVars = GraphicsVars::create(mpProgram.get());
    mPassDataOffset = mpVars->getParameterBlock("PerFrameCB")->getVariableOffset("Data");

    if (!(mpLight = mpScene->getLightByName("Point light")))
    {
        mpLight = (mpScene && mpScene->getLightCount() ? mpScene->getLight(0) : nullptr);
    }
    assert(mpLight);
}