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
#include "MultiViewShadowMapBase.h"
#include "Helper.h"
#include "ShadowMapConstant.slangh"

namespace
{
    // output
    const std::string kShadowMapSet = "ShadowMapSet";
    const std::string kIdSet = "IdSet";
}

std::string STSM_MultiViewShadowMapBase::mKeyShadowMapSet = kShadowMapSet;
std::string STSM_MultiViewShadowMapBase::mKeyIdSet = kIdSet;

void STSM_MultiViewShadowMapBase::registerScriptBindings(pybind11::module& m)
{
    pybind11::class_<STSM_MultiViewShadowMapBase, RenderPass, STSM_MultiViewShadowMapBase::SharedPtr> SMPass(m, "STSM_MultiViewShadowMapBase");

    SMPass.def_property("MaskTextureFile", &STSM_MultiViewShadowMapBase::getMaskTexture, &STSM_MultiViewShadowMapBase::setMaskTexture);
}

STSM_MultiViewShadowMapBase::STSM_MultiViewShadowMapBase()
{
}

Dictionary STSM_MultiViewShadowMapBase::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection STSM_MultiViewShadowMapBase::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addOutput(mKeyShadowMapSet, "ShadowMapSet").bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource).format(gShadowMapDepthFormat).texture2D(gShadowMapSize.x, gShadowMapSize.y, 0, 1, _SHADOW_MAP_NUM);
    reflector.addOutput(mKeyIdSet, "IdSet").bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource).format(ResourceFormat::R32Uint).texture2D(gShadowMapSize.x, gShadowMapSize.y, 0, 1, _SHADOW_MAP_NUM);
    return reflector;
}

void STSM_MultiViewShadowMapBase::execute(RenderContext* vRenderContext, const RenderData& vRenderData)
{
    if (!mpScene || !mLightInfo.pLight) return;

    // update light
    __sampleLight();

    // write internal data
    InternalDictionary& Dict = vRenderData.getDictionary();
    Dict["ShadowMapData"] = mShadowMapInfo.ShadowMapData;
    Dict["LightData"] = mShadowMapInfo.LightData;
    Dict["LightMaskTexture"] = mLightInfo.pMaskTexture;
}

void STSM_MultiViewShadowMapBase::renderUI(Gui::Widgets& widget)
{
    if (!mLightInfo.RectLightList.empty())
    {
        widget.dropdown("Current Light", mLightInfo.RectLightList, mLightInfo.CurrentRectLightIndex);
        __updateAreaLight(mLightInfo.CurrentRectLightIndex);
    }
    else
        widget.text("No Light in Scene");

    widget.checkbox("Jitter Area Light Camera", mVContronls.jitterAreaLightCamera);
    widget.checkbox("Opimize Sample", mVContronls.optimizeSample);
    if (widget.var("Sample Count", mJitterPattern.mSampleCount, 1u, (uint)_SHADOW_MAP_NUM * 32, 1u))
        __initSamplePattern();

    if (mLightInfo.pMaskTexture)
    {
        widget.image("Mask Texture", mLightInfo.pMaskTexture, float2(100.f));
        if (widget.button("Remove Mask texture"))
        {
            setMaskTexture("");
            //auto NewPos = mpScene->getCamera()->getPosition() + float3(0.001f);
            //mpScene->getCamera()->setPosition(NewPos); // dirty and reset accumulate
        }
    }
    if (widget.button("Choose Mask texture"))
    {
        FileDialogFilterVec Filters{ { "png", "png" }, { "bmp", "bmp" }, { "jpg", "jpg" } };
        std::string FileName;
        if (openFileDialog(Filters, FileName))
        {
            setMaskTexture(FileName);
            //auto NewPos = mpScene->getCamera()->getPosition() + float3(0.001f);
            //mpScene->getCamera()->setPosition(NewPos); // dirty and reset accumulate

            // print mask
            /*std::ofstream File("bitmapMaskData.ppm");
            File << "P2\n";
            File << mLightInfo.pMaskBitmap->getWidth() << " " << mLightInfo.pMaskBitmap->getHeight() << "\n";
            File << "255\n";
            for (uint32_t i = 0; i < mLightInfo.pMaskBitmap->getHeight(); ++i)
            {
                for (uint32_t k = 0; k < mLightInfo.pMaskBitmap->getWidth(); ++k)
                {
                    uint32_t ChannelNum = getFormatChannelCount(mLightInfo.pMaskBitmap->getFormat());
                    uint32_t Index = (i * mLightInfo.pMaskBitmap->getWidth() + k) * ChannelNum;
                    uint8_t* pData = mLightInfo.pMaskBitmap->getData();
                    uint8_t Pixel = pData[Index];
                    File << std::to_string(Pixel) << " ";
                }
                File << "\n";
            }*/
        }
    }
}

void STSM_MultiViewShadowMapBase::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    _ASSERTE(mpScene);
    // find all rect area light
    mLightInfo.pLight = nullptr;
    mLightInfo.RectLightList.clear();
    uint32_t LightNum = mpScene->getLightCount();
    for (uint32_t i = 0; i < LightNum; ++i)
    {
        auto pLight = mpScene->getLight(i);
        if (pLight->getType() == LightType::Rect)
            mLightInfo.RectLightList.emplace_back(Gui::DropdownValue{ i, pLight->getName() });
    }
    _ASSERTE(mLightInfo.RectLightList.size() > 0);
    if (!mLightInfo.RectLightList.empty())
    {
        __updateAreaLight(mLightInfo.RectLightList[0].value);
    }

    // set light camera
    mLightInfo.pCamera = nullptr;
    const auto& CameraSet = mpScene->getCameras();
    for (auto pCamera : CameraSet)
    {
        if (pCamera->getName() == "LightCamera")
        {
            mLightInfo.pCamera = pCamera;
            mLightInfo.pCamera->setUpVector(float3(1.0, 0.0, 0.0));
            mLightInfo.pCamera->setAspectRatio((float)gShadowMapSize.x / (float)gShadowMapSize.y);
            break;
        }
    }

    __initSamplePattern(); //default as halton
}

void STSM_MultiViewShadowMapBase::__sampleLight()
{
    PROFILE("Sample Light");
    if (!mVContronls.jitterAreaLightCamera) __sampleAreaPosW();
    else __sampleWithDirectionFixed();
}

void STSM_MultiViewShadowMapBase::__sampleWithDirectionFixed()
{
    auto pCamera = mpScene->getCamera();
    float Aspect = (float)gShadowMapSize.x / (float)gShadowMapSize.y;

    /*float2 BaseUv = mJitterPattern.pSampleGenerator->getNextSample();
    float2 BaseUvEdge = BaseUv / std::max(std::abs(BaseUv.x), std::abs((BaseUv.y)));
    float2 BaseRoundUv = BaseUv / glm::length(BaseUvEdge);
    float BaseRadian = glm::atan(BaseRoundUv.y, BaseRoundUv.x);
    float BaseRadius = glm::length(BaseRoundUv);
    float RadianStep = glm::pi<float>() * 2.0f / _SHADOW_MAP_NUM;*/

    float2 UvSet[_SHADOW_MAP_NUM];
    for (uint i = 0; i < _SHADOW_MAP_NUM; ++i)
    {
        /*float Radian = BaseRadian + i * RadianStep;
        float2 uv;
        uv.x = glm::cos(Radian);
        uv.y = glm::sin(Radian);
        uv = uv / std::max(std::abs(uv.x), std::abs((uv.y)));
        uv *= BaseRadius;
        UvSet[i] = uv;*/

        //UvSet[i] = BaseUv * (i + 1.0f) / float(_SHADOW_MAP_NUM);

        /*float Radian = BaseRadian + i * RadianStep;
        float2 uv;
        uv.x = glm::cos(Radian);
        uv.y = glm::sin(Radian);
        uv = uv / std::max(std::abs(uv.x), std::abs((uv.y)));
        uv *= BaseRadius;
        UvSet[i] = uv * float(_SHADOW_MAP_NUM - i) / float(_SHADOW_MAP_NUM);*/

        float2 uv = mJitterPattern.pSampleGenerator->getNextSample();
        while (mVContronls.optimizeSample)
        {
            bool Pass = true;
            uv = mJitterPattern.pSampleGenerator->getNextSample();
            for (uint k = 0; k < i; ++k)
            {
                if (glm::distance(uv, UvSet[k]) < 0.4f)
                {
                    Pass = false;
                    break;
                }
            }

            if (Pass) break;
        }
        UvSet[i] = uv;
    }

    for (uint i = 0; i < _SHADOW_MAP_NUM; ++i)
    {
        uint Index = i;
        /*
        *      v
        *  ... | ...
        *  8 9 | ...
        * -----------> u
        *  4 5 | 6 7
        *  0 1 | 2 3
        */

        float2 uv = UvSet[i];
        if (mLightInfo.pMaskBitmap)
        {
            while (true)
            {
                float2 TexCoord = uv * 0.5f + 0.5f;
                int y = (int)glm::round((1 - TexCoord.y) * mLightInfo.pMaskBitmap->getHeight());
                int x = (int)glm::round(TexCoord.x * mLightInfo.pMaskBitmap->getWidth());
                uint32_t ChannelNum = getFormatChannelCount(mLightInfo.pMaskBitmap->getFormat());
                int Index = (y * mLightInfo.pMaskBitmap->getWidth() + x) * ChannelNum;
                uint8_t* pData = mLightInfo.pMaskBitmap->getData();
                uint8_t Pixel = pData[Index];
                if (Pixel > std::numeric_limits<uint8_t>::max() / 2)
                    break;

                uv = mJitterPattern.pSampleGenerator->getNextSample();
            }
        }

        Helper::ShadowVPHelper ShadowVP(pCamera, mLightInfo.pLight, Aspect, uv);
        //Helper::ShadowVPHelper ShadowVP(pCamera, mLightInfo.pLight, Aspect, IrregularUv);
        //float4x4 VP = ShadowVP.getProj()*glm::inverse(mLightInfo.pLight->getData().transMat);
        float4x4 VP = ShadowVP.getVP();
        mShadowMapInfo.ShadowMapData.allGlobalMat[Index] = VP;
        mShadowMapInfo.ShadowMapData.allInvGlobalMat[Index] = inverse(VP);
        mShadowMapInfo.ShadowMapData.allUv[Index] = uv;
        float4 LPos = float4(mLightInfo.pLight->getPosByUv(uv), 1);
        float3 LightLocalPos = mLightInfo.pLight->getPosLocalByUv(uv);
        if (mLightPreTransMat == float4x4(0)) mLightPreTransMat = mLightInfo.pLight->getData().transMat;
        float4 LPrePos = mLightPreTransMat * float4(LightLocalPos, 1.0f);
        mShadowMapInfo.LightData.allLightPos[Index] = LPos;
        mShadowMapInfo.LightData.allLightPrePos[Index] = LPrePos;

        if (mLightInfo.pCamera)
        {
            float3 Pos = mLightInfo.pLight->getPosByUv(uv);
            float3 Direction = mLightInfo.pLight->getDirection();

            mLightInfo.pCamera->setPosition(Pos);
            mLightInfo.pCamera->setTarget(Pos + Direction);
            float FocalLength = fovYToFocalLength(mLightInfo.pLight->getOpeningAngle(), mLightInfo.pCamera->getFrameHeight());
            mLightInfo.pCamera->setFocalLength(FocalLength);
        }
    }

    mLightPreTransMat = mLightInfo.pLight->getData().transMat;
}

void STSM_MultiViewShadowMapBase::__sampleAreaPosW()
{
    float3 EyePosBehindAreaLight = __calcEyePosition();
    float3 Direction = mLightInfo.pLight->getDirection();

    auto pCamera = mpScene->getCamera();
    PointLight::SharedPtr TempLight = PointLight::create("Temp");
    TempLight->setWorldPosition(EyePosBehindAreaLight);
    Helper::ShadowVPHelper ShadowVP(pCamera, TempLight, 1.0f);
    float4x4 VP = ShadowVP.getVP();

    for (uint i = 0; i < _SHADOW_MAP_NUM; ++i)
    {
        mShadowMapInfo.ShadowMapData.allGlobalMat[i] = VP;
        mShadowMapInfo.ShadowMapData.allInvGlobalMat[i] = inverse(VP);
        mShadowMapInfo.ShadowMapData.allUv[i] = float2(0.0f);
        float4 LPos = inverse(ShadowVP.getView()) * float4(0, 0, 0, 1);
        mShadowMapInfo.LightData.allLightPos[i] = LPos;
        mShadowMapInfo.LightData.allLightPrePos[i] = LPos; // no calculation of pre pos
    }

    if (mLightInfo.pCamera)
    {
        mLightInfo.pCamera->setPosition(EyePosBehindAreaLight);
        mLightInfo.pCamera->setTarget(EyePosBehindAreaLight + Direction);
    }
}

void STSM_MultiViewShadowMapBase::__initSamplePattern()
{
    mJitterPattern.pSampleGenerator = std::make_shared<CSampleGenerator>();
    mJitterPattern.pSampleGenerator->init(mJitterPattern.mSamplePattern, mJitterPattern.mSampleCount);
    _ASSERTE(mJitterPattern.pSampleGenerator);
}

float3 STSM_MultiViewShadowMapBase::__calcEyePosition()
{
    float fovy = mpScene->getCamera()->getFovY(); // use main camera fovy
    float halfFovy = fovy / 2.0f;

    float maxAreaL = 2.0f;//Local Space

    float dLightCenter2EyePos = maxAreaL / (2.0f * std::tan(halfFovy));

    float3 LightCenter = float3(0, 0, 0);//Local Space
    float3 LightNormal = float3(0, 0, 1);//Local Space
    float3 EyePos = LightCenter - (dLightCenter2EyePos * LightNormal);

    return EyePos;
}

void STSM_MultiViewShadowMapBase::__updateAreaLight(uint vIndex)
{
    mLightInfo.CurrentRectLightIndex = vIndex;
    RectLight::SharedPtr pNewLight = std::dynamic_pointer_cast<RectLight>(mpScene->getLight(vIndex));

    if (pNewLight == mLightInfo.pLight) return;
    mLightInfo.pLight = pNewLight;
}
