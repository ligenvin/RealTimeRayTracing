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
#include "Reuse.h"

using namespace Falcor;

class STSM_TemporalReuse : public RenderPass
{
public:
    using SharedPtr = std::shared_ptr<STSM_TemporalReuse>;

    /** Create a new render pass object.
        \param[in] pRenderContext The render context.
        \param[in] dict Dictionary of serialized parameters.
        \return A new object, or an exception is thrown if creation failed.
    */
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual std::string getDesc() override;
    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pContext, const CompileData& compileData) override;
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual bool onMouseEvent(const MouseEvent& mouseEvent) override { return false; }
    virtual bool onKeyEvent(const KeyboardEvent& keyEvent) override { return false; }

    static void registerScriptBindings(pybind11::module& m);

    bool getAccumulateBlend() { return mVControls.accumulateBlend; }
    void setAccumulateBlend(bool v) { mVControls.accumulateBlend = v; }
    bool getClamp() { return mVControls.clamp; }
    void setClamp(bool v) { mVControls.clamp = v; }
    uint getClampSearchRadius() { return mVControls.clampSearchRadius; }
    void setClampSearchRadius(uint v) { mVControls.clampSearchRadius = v; }
    float getClampExtendRange() { return mVControls.clampExtendRange; }
    void setClampExtendRange(float v) { mVControls.clampExtendRange = v; }

    float getDiscardByPositionStrength() { return mVControls.discardByPositionStrength; }
    void setDiscardByPositionStrength(float v) { mVControls.discardByPositionStrength = v; }

    float getDiscardByNormalStrength() { return mVControls.discardByNormalStrength; }
    void setDiscardByNormalStrength(float v) { mVControls.discardByNormalStrength = v; }

    bool getAdaptiveAlpha() { return mVControls.adaptiveAlpha; }
    void setAdaptiveAlpha(bool v) { mVControls.adaptiveAlpha = v; }
    float getAlpha() { return mVControls.alpha; }
    void setAlpha(float v) { mVControls.alpha = v; }
    float getBeta() { return mVControls.beta; }
    void setBeta(float v) { mVControls.beta = v; }
    float getRatiodv() { return mVControls.ratiodv; }
    void setRatiodv(float v) { mVControls.ratiodv = v; }
    float getRatioddv() { return mVControls.ratioddv; }
    void setRatioddv(float v) { mVControls.ratioddv = v; }

private:
    STSM_TemporalReuse();
    Scene::SharedPtr mpScene;
    Sampler::SharedPtr mpSamplerLinear;
    static Gui::DropdownList mReuseSampleTypeList;

    // temporal blending pass
    struct  
    {
        FullScreenPass::SharedPtr mpPass;
        Fbo::SharedPtr mpFbo;
        EReuseSampleType mReuseType = EReuseSampleType::BILINEAR;
    } mVReusePass;

    struct 
    {   
        bool accumulateBlend = true;
        bool clamp = false;
        uint clampSearchRadius = 3;
        float clampExtendRange = 0.0;
        float discardByPositionStrength = 1.0f;
        float discardByNormalStrength = 1.0f;
        bool adaptiveAlpha = true;
        float alpha = 0.1f;
        float beta = 1.0f;
        float ratiodv = 11.0f;
        float ratioddv = 30.0f;
    } mVControls;

    uint mIterationIndex = 1;

    void createVReusePassResouces();
    void updateBlendWeight();
    void __loadVariationTextures(const RenderData& vRenderData, Texture::SharedPtr& voVariation, Texture::SharedPtr& voVarOfVar);

    void __loadParams(const pybind11::dict& Dict);
    pybind11::dict __getParams();
};
