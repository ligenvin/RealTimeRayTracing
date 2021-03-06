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
#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"
#include "RenderGraph/RenderPassHelpers.h"
#include "LTCData.slangh"

using namespace Falcor;

class LTCLight : public RenderPass
{
public:
    using SharedPtr = std::shared_ptr<LTCLight>;

    /** Create a new render pass object.
        \param[in] pRenderContext The render context.
        \param[in] dict Dictionary of serialized parameters.
        \return A new object, or an exception is thrown if creation failed.
    */
    static SharedPtr create(RenderContext *pRenderContext = nullptr, const Dictionary &dict = {});

    virtual std::string getDesc() override;
    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData &compileData) override;
    virtual void compile(RenderContext *pContext, const CompileData &compileData) override {}
    virtual void execute(RenderContext *pRenderContext, const RenderData &renderData) override;
    virtual void renderUI(Gui::Widgets &widget) override;
    virtual bool onMouseEvent(const MouseEvent &mouseEvent) override { return false; }
    virtual bool onKeyEvent(const KeyboardEvent &keyEvent) override { return false; }
    virtual void setScene(RenderContext *pRenderContext, const Scene::SharedPtr &pScene) override;

private:
    LTCLight();
    void __initPassData();
    void __updateRectLightProperties();
    Texture::SharedPtr __generateLightColorTex();

    void __initDebugDrawerResources();
    void __drawLightDebug(RenderContext *vRenderContext);

    // pass resource
    FullScreenPass::SharedPtr mpPass;
    Fbo::SharedPtr mpFbo;

    // scene resource
    Scene::SharedPtr mpScene;
    RectLight::SharedPtr mpLight;
    Camera::SharedPtr mpCam;

    // LTC shader data
    Texture::SharedPtr mpLTCMatrixTex;
    Texture::SharedPtr mpLTCMagnitueTex;
    Texture::SharedPtr mpLTCLightColorTex;
    SPassData mPassData;
    bool mUseTextureLight = false;

    Sampler::SharedPtr mpSampler;

    // Debug drawer
    TriangleDebugDrawer::SharedPtr mpLightDebugDrawer;
    struct
    {
        RasterPass::SharedPtr mpRasterPass = nullptr;
        GraphicsState::SharedPtr mpGraphicsState = nullptr;
        GraphicsVars::SharedPtr mpVars = nullptr;
        GraphicsProgram::SharedPtr mpProgram = nullptr;
        RasterizerState::SharedPtr mpRasterState = nullptr;

        Sampler::SharedPtr mpSampler;

        float4x4 MatLightLocal2PosW;
        float4x4 MatCamVP;
    } mDebugDrawerResource;

    // env map
    EnvMapLighting::SharedPtr mpEnvMapLighting;
    /*struct
    {
        float3 EularRotation = float3(0,0,0);
        float Intensity = 2.;
    }mEnvLightData;*/
    void __prepareEnvMap(RenderContext *vRenderContext);
};
