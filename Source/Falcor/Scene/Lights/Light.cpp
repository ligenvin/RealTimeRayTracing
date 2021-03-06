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
#include "stdafx.h"
#include "Light.h"
#include "Utils/UI/Gui.h"
#include "Utils/Color/ColorHelpers.slang"

namespace Falcor
{
    static bool checkOffset(const std::string& structName, UniformShaderVarOffset cbOffset, size_t cppOffset, const char* field)
    {
        if (cbOffset.getByteOffset() != cppOffset)
        {
            logError("Light::" + std::string(structName) + ":: " + std::string(field) + " CB offset mismatch. CB offset is " + std::to_string(cbOffset.getByteOffset()) + ", C++ data offset is " + std::to_string(cppOffset));
            return false;
        }
        return true;
    }

    // Light

    void Light::setActive(bool active)
    {
        if (active != mActive)
        {
            mActive = active;
            mActiveChanged = true;
        }
    }

    void Light::setIntensity(const float3& intensity)
    {
        mData.intensity = intensity;
    }

    Light::Changes Light::beginFrame()
    {
        mChanges = Changes::None;
        if (mActiveChanged) mChanges |= Changes::Active;
        if (mPrevData.posW != mData.posW) mChanges |= Changes::Position;
        if (mPrevData.dirW != mData.dirW) mChanges |= Changes::Direction;
        if (mPrevData.intensity != mData.intensity) mChanges |= Changes::Intensity;
        if (mPrevData.openingAngle != mData.openingAngle) mChanges |= Changes::SurfaceArea;
        if (mPrevData.penumbraAngle != mData.penumbraAngle) mChanges |= Changes::SurfaceArea;
        if (mPrevData.cosSubtendedAngle != mData.cosSubtendedAngle) mChanges |= Changes::SurfaceArea;
        if (mPrevData.surfaceArea != mData.surfaceArea) mChanges |= Changes::SurfaceArea;
        if (mPrevData.transMat != mData.transMat) mChanges |= (Changes::Position | Changes::Direction);

        assert(mPrevData.tangent == mData.tangent);
        assert(mPrevData.bitangent == mData.bitangent);

        mPrevData = mData;
        mActiveChanged = false;

        return getChanges();
    }

    void Light::setShaderData(const ShaderVar& var)
    {
#if _LOG_ENABLED
#define check_offset(_a) {static bool b = true; if(b) {assert(checkOffset("LightData", var.getType()->getMemberOffset(#_a), offsetof(LightData, _a), #_a));} b = false;}
        check_offset(dirW);
        check_offset(intensity);
        check_offset(penumbraAngle);
#undef check_offset
#endif

        var.setBlob(mData);
    }

    float3 Light::getColorForUI()
    {
        if ((mUiLightIntensityColor * mUiLightIntensityScale) != mData.intensity)
        {
            float mag = std::max(mData.intensity.x, std::max(mData.intensity.y, mData.intensity.z));
            if (mag <= 1.f)
            {
                mUiLightIntensityColor = mData.intensity;
                mUiLightIntensityScale = 1.0f;
            }
            else
            {
                mUiLightIntensityColor = mData.intensity / mag;
                mUiLightIntensityScale = mag;
            }
        }

        return mUiLightIntensityColor;
    }

    void Light::setColorFromUI(const float3& uiColor)
    {
        mUiLightIntensityColor = uiColor;
        setIntensity(mUiLightIntensityColor * mUiLightIntensityScale);
    }

    float Light::getIntensityForUI()
    {
        if ((mUiLightIntensityColor * mUiLightIntensityScale) != mData.intensity)
        {
            float mag = std::max(mData.intensity.x, std::max(mData.intensity.y, mData.intensity.z));
            if (mag <= 1.f)
            {
                mUiLightIntensityColor = mData.intensity;
                mUiLightIntensityScale = 1.0f;
            }
            else
            {
                mUiLightIntensityColor = mData.intensity / mag;
                mUiLightIntensityScale = mag;
            }
        }

        return mUiLightIntensityScale;
    }

    void Light::setIntensityFromUI(float intensity)
    {
        mUiLightIntensityScale = intensity;
        setIntensity(mUiLightIntensityColor * mUiLightIntensityScale);
    }

    void Light::renderUI(Gui::Widgets& widget)
    {
        bool active = isActive();
        if (widget.checkbox("Active", active)) setActive(active);

        if (mHasAnimation) widget.checkbox("Animated", mIsAnimated);

        float3 color = getColorForUI();
        if (widget.rgbColor("Color", color))
        {
            setColorFromUI(color);
        }

        float intensity = getIntensityForUI();
        if (widget.var("Intensity", intensity))
        {
            setIntensityFromUI(intensity);
        }
    }

    Light::Light(const std::string& name, LightType type)
        : mName(name)
    {
        mData.type = (uint32_t)type;
    }

    // PointLight

    PointLight::SharedPtr PointLight::create(const std::string& name)
    {
        return SharedPtr(new PointLight(name));
    }

    PointLight::PointLight(const std::string& name)
        : Light(name, LightType::Point)
    {
        mPrevData = mData;
    }

    void PointLight::setWorldDirection(const float3& dir)
    {
        if (!(glm::length(dir) > 0.f)) // NaNs propagate
        {
            logWarning("Can't set light direction to zero length vector. Ignoring call.");
            return;
        }
        mData.dirW = normalize(dir);
    }

    void PointLight::setWorldPosition(const float3& pos)
    {
        mData.posW = pos;
    }

    float PointLight::getPower() const
    {
        return luminance(mData.intensity) * 4.f * (float)M_PI;
    }

    void PointLight::renderUI(Gui::Widgets& widget)
    {
        Light::renderUI(widget);

        widget.var("World Position", mData.posW, -FLT_MAX, FLT_MAX);
        widget.direction("Direction", mData.dirW);

        float openingAngle = getOpeningAngle();
        if (widget.var("Opening Angle", openingAngle, 0.f, (float)M_PI)) setOpeningAngle(openingAngle);
        float penumbraAngle = getPenumbraAngle();
        if (widget.var("Penumbra Width", penumbraAngle, 0.f, (float)M_PI)) setPenumbraAngle(penumbraAngle);
    }

    void PointLight::setOpeningAngle(float openingAngle)
    {
        openingAngle = glm::clamp(openingAngle, 0.f, (float)M_PI);
        if (openingAngle == mData.openingAngle) return;

        mData.openingAngle = openingAngle;
        mData.penumbraAngle = std::min(mData.penumbraAngle, openingAngle);

        // Prepare an auxiliary cosine of the opening angle to quickly check whether we're within the cone of a spot light.
        mData.cosOpeningAngle = std::cos(openingAngle);
    }

    void PointLight::setPenumbraAngle(float angle)
    {
        angle = glm::clamp(angle, 0.0f, mData.openingAngle);
        if (mData.penumbraAngle == angle) return;
        mData.penumbraAngle = angle;
    }

    void PointLight::updateFromAnimation(const glm::mat4& transform)
    {
        float3 fwd = float3(-transform[2]);
        float3 pos = float3(transform[3]);
        setWorldPosition(pos);
        setWorldDirection(fwd);
    }

    // DirectionalLight

    DirectionalLight::DirectionalLight(const std::string& name)
        : Light(name, LightType::Directional)
    {
        mPrevData = mData;
    }

    DirectionalLight::SharedPtr DirectionalLight::create(const std::string& name)
    {
        return SharedPtr(new DirectionalLight(name));
    }

    void DirectionalLight::renderUI(Gui::Widgets& widget)
    {
        Light::renderUI(widget);

        if (widget.direction("Direction", mData.dirW))
        {
            setWorldDirection(mData.dirW);
        }
    }

    void DirectionalLight::setWorldDirection(const float3& dir)
    {
        if (!(glm::length(dir) > 0.f)) // NaNs propagate
        {
            logWarning("Can't set light direction to zero length vector. Ignoring call.");
            return;
        }
        mData.dirW = normalize(dir);
    }

    void DirectionalLight::updateFromAnimation(const glm::mat4& transform)
    {
        float3 fwd = float3(-transform[2]);
        setWorldDirection(fwd);
    }

    // DistantLight

    DistantLight::SharedPtr DistantLight::create(const std::string& name)
    {
        return SharedPtr(new DistantLight(name));
    }

    DistantLight::DistantLight(const std::string& name)
        : Light(name, LightType::Distant)
    {
        mData.dirW = float3(0.f, -1.f, 0.f);
        setAngle(0.5f * 0.53f * (float)M_PI / 180.f);   // Approximate sun half-angle
        update();
        mPrevData = mData;
    }

    void DistantLight::renderUI(Gui::Widgets& widget)
    {
        Light::renderUI(widget);

        if (widget.direction("Direction", mData.dirW))
        {
            setWorldDirection(mData.dirW);
        }

        if (widget.var("Half-angle", mAngle, 0.f, (float)M_PI_2))
        {
            setAngle(mAngle);
        }
        widget.tooltip("Half-angle subtended by the light, in radians.");
    }

    void DistantLight::setAngle(float angle)
    {
        mAngle = glm::clamp(angle, 0.f, (float)M_PI_2);

        mData.cosSubtendedAngle = std::cos(mAngle);
    }

    void DistantLight::setWorldDirection(const float3& dir)
    {
        if (!(glm::length(dir) > 0.f)) // NaNs propagate
        {
            logWarning("Can't set light direction to zero length vector. Ignoring call.");
            return;
        }
        mData.dirW = normalize(dir);
        update();
    }

    void DistantLight::update()
    {
        // Update transformation matrices
        // Assumes that mData.dirW is normalized
        const float3 up(0.f, 0.f, 1.f);
        float3 vec = glm::cross(up, -mData.dirW);
        float sinTheta = glm::length(vec);
        if (sinTheta > 0.f)
        {
            float cosTheta = glm::dot(up, -mData.dirW);
            mData.transMat = glm::rotate(glm::mat4(), std::acos(cosTheta), vec);
        }
        else
        {
            mData.transMat = glm::mat4();
        }
        mData.transMatIT = glm::inverse(glm::transpose(mData.transMat));
    }

    void DistantLight::updateFromAnimation(const glm::mat4& transform)
    {
        float3 fwd = float3(-transform[2]);
        setWorldDirection(fwd);
    }

    // AnalyticAreaLight

    AnalyticAreaLight::AnalyticAreaLight(const std::string& name, LightType type)
        : Light(name, type)
    {
        mData.tangent = float3(1, 0, 0);
        mData.bitangent = float3(0, 1, 0);
        mData.surfaceArea = 4.0f;

        mScaling = float3(1, 1, 1);
        update();
        mPrevData = mData;
    }

    float AnalyticAreaLight::getPower() const
    {
        return luminance(mData.intensity) * (float)M_PI * mData.surfaceArea;
    }

    void AnalyticAreaLight::update()
    {
        // Update matrix
        auto LookAt = glm::lookAt(mData.posW, mData.posW - mData.dirW, float3(0, 1, 0));
        Transform tr;
        //tr.setScaling(mScaling);
        tr.lookAt(mData.posW, mData.posW + mData.dirW, float3(0, 1, 0));
        mData.transMat = tr.getMatrix();
        //mData.transMat = mTransformMatrix;
        mData.transMatIT = glm::inverse(glm::transpose(mData.transMat));
    }

    // RectLight
    RectLight::SharedPtr RectLight::create(const std::string& name)
    {
        return SharedPtr(new RectLight(name));
    }

    float3 RectLight::transformPoint(float3 vLightLocalPos) const
    {
        float4 PosW = mData.transMat * float4(vLightLocalPos, 1.0f);
        return PosW.xyz * (1.0f / PosW.w);
    }

    float3 RectLight::getDirection() const
    {
        float3 AreaLightDir = float3(0, 0, 1);//use normal as direction
        // normal is axis-aligned, so no need to construct normal transform matrix
        float3 AreaLightDirW = normalize(mData.transMat * float4(AreaLightDir, 0.0f)).xyz;

        return AreaLightDirW;
    }

    float3 RectLight::getCenter() const
    {
        float3 AreaLightCenter = float3(0, 0, 0);
        return transformPoint(AreaLightCenter);
    }

    float2 RectLight::getSize() const
    {
        /*float3 XMin = float3(-1, 0, 0);
        float3 XMax = float3(1, 0, 0);
        float3 YMin = float3(0, -1, 0);
        float3 YMax = float3(0, 1, 0);

        float4x4 LightTransform = mData.transMat;
        float4 XMinH = LightTransform * float4(XMin, 1.0f);
        float4 XMaxH = LightTransform * float4(XMax, 1.0f);
        float4 YMinH = LightTransform * float4(YMin, 1.0f);
        float4 YMaxH = LightTransform * float4(YMax, 1.0f);

        float3 XMinW = XMinH.xyz * (1.0f / XMinH.w);
        float3 XMaxW = XMaxH.xyz * (1.0f / XMaxH.w);
        float3 YMinW = YMinH.xyz * (1.0f / YMinH.w);
        float3 YMaxW = YMaxH.xyz * (1.0f / YMaxH.w);

        float Width = distance(XMinH, XMaxH);
        float Height = distance(YMinW, YMaxW);*/

        float2 res = float2(mScaling.x * 2., mScaling.y * 2.);
        return res;
    }

    float3 RectLight::getPosLocalByUv(float2 vUv) const
    {
        float2 Size = getSize() * float2(0.5);
        return float3(vUv * Size,0);
    }

    float3 RectLight::getPosByUv(float2 vUv) const
    {
        //return transformPoint(float3(vUv, 0.0f));
        return transformPoint(getPosLocalByUv(vUv));
    }

    float3 RectLight::getPrePosByUv(float2 vUv) const
    {
        float3 LightLocalPos =  (float3((float2(-1, 1) * vUv) * getSize() * float2(0.5), 0.0f));
        float4 PosW = mPrevData.transMat * float4(LightLocalPos, 1.0f);
        return PosW.xyz * (1.0f / PosW.w);
    }

    void RectLight::renderUI(Gui::Widgets& widget)
    {
        Light::renderUI(widget);

        if (widget.var("World Position", mData.posW, -FLT_MAX, FLT_MAX))
        {
            int i = 1;
        }

        if (widget.direction("Direction", mData.dirW))
        {
            int i = 1;
        }

        if (widget.var("Light Size", mScaling, 0.1f, 100.0f))
        {
            int i = 1;
        }

        float Angle = glm::degrees(mOpeningAngle);
        widget.var("Opening Angle", Angle, 10.0f, 170.0f, 1.0f);
        mOpeningAngle = glm::radians(Angle);

        update();
    }

    void RectLight::updateFromAnimation(const glm::mat4& transform)
    {
        mData.posW = (transform*float4(0,0,0,1)).xyz;
        float3 Dir = (transform * float4(0, 0, -1, 0)).xyz;
        if (glm::length(Dir) > 0.f)
        {
            mData.dirW = glm::normalize(Dir);
        }
        float rx = glm::length(transform * float4(1.0f, 0.0f, 0.0f, 0.0f));
        float ry = glm::length(transform * float4(0.0f, 1.0f, 0.0f, 0.0f));
        float rz = glm::length(transform * float4(0.0f, 0.0f, 1.0f, 0.0f));
        mScaling = float3(rx, ry, rz);

        AnalyticAreaLight::updateFromAnimation(transform);
    }

    void RectLight::update()
    {
        AnalyticAreaLight::update();

        float rx = glm::length(mData.transMat * float4(1.0f, 0.0f, 0.0f, 0.0f));
        float ry = glm::length(mData.transMat * float4(0.0f, 1.0f, 0.0f, 0.0f));
        mData.surfaceArea = 4.0f * rx * ry;
    }

    // DiscLight

    DiscLight::SharedPtr DiscLight::create(const std::string& name)
    {
        return SharedPtr(new DiscLight(name));
    }

    void DiscLight::update()
    {
        AnalyticAreaLight::update();

        float rx = glm::length(mData.transMat * float4(1.0f, 0.0f, 0.0f, 0.0f));
        float ry = glm::length(mData.transMat * float4(0.0f, 1.0f, 0.0f, 0.0f));

        mData.surfaceArea = (float)M_PI * rx * ry;
    }

    // SphereLight

    SphereLight::SharedPtr SphereLight::create(const std::string& name)
    {
        return SharedPtr(new SphereLight(name));
    }

    void SphereLight::update()
    {
        AnalyticAreaLight::update();

        float rx = glm::length(mData.transMat * float4(1.0f, 0.0f, 0.0f, 0.0f));
        float ry = glm::length(mData.transMat * float4(0.0f, 1.0f, 0.0f, 0.0f));
        float rz = glm::length(mData.transMat * float4(0.0f, 0.0f, 1.0f, 0.0f));

        mData.surfaceArea = 4.0f * (float)M_PI * std::pow(std::pow(rx * ry, 1.6f) + std::pow(ry * rz, 1.6f) + std::pow(rx * rz, 1.6f) / 3.0f, 1.0f / 1.6f);
    }


    SCRIPT_BINDING(Light)
    {
        SCRIPT_BINDING_DEPENDENCY(Animatable)

        pybind11::class_<Light, Animatable, Light::SharedPtr> light(m, "Light");
        light.def_property("name", &Light::getName, &Light::setName);
        light.def_property("active", &Light::isActive, &Light::setActive);
        light.def_property("animated", &Light::isAnimated, &Light::setIsAnimated);
        light.def_property("intensity", &Light::getIntensity, &Light::setIntensity);

        pybind11::class_<PointLight, Light, PointLight::SharedPtr> pointLight(m, "PointLight");
        pointLight.def(pybind11::init(&PointLight::create), "name"_a = "");
        pointLight.def_property("position", &PointLight::getWorldPosition, &PointLight::setWorldPosition);
        pointLight.def_property("direction", &PointLight::getWorldDirection, &PointLight::setWorldDirection);
        pointLight.def_property("openingAngle", &PointLight::getOpeningAngle, &PointLight::setOpeningAngle);
        pointLight.def_property("penumbraAngle", &PointLight::getPenumbraAngle, &PointLight::setPenumbraAngle);

        pybind11::class_<DirectionalLight, Light, DirectionalLight::SharedPtr> directionalLight(m, "DirectionalLight");
        directionalLight.def(pybind11::init(&DirectionalLight::create), "name"_a = "");
        directionalLight.def_property("direction", &DirectionalLight::getWorldDirection, &DirectionalLight::setWorldDirection);

        pybind11::class_<DistantLight, Light, DistantLight::SharedPtr> distantLight(m, "DistantLight");
        distantLight.def(pybind11::init(&DistantLight::create), "name"_a = "");
        distantLight.def_property("direction", &DistantLight::getWorldDirection, &DistantLight::setWorldDirection);
        distantLight.def_property("angle", &DistantLight::getAngle, &DistantLight::setAngle);

        pybind11::class_<AnalyticAreaLight, Light, AnalyticAreaLight::SharedPtr> analyticLight(m, "AnalyticAreaLight");

        pybind11::class_<RectLight, AnalyticAreaLight, RectLight::SharedPtr> rectLight(m, "RectLight");
        rectLight.def(pybind11::init(&RectLight::create), "name"_a = "");
        rectLight.def_property("openingAngle", &RectLight::getOpeningAngle, &RectLight::setOpeningAngle);

        pybind11::class_<DiscLight, AnalyticAreaLight, DiscLight::SharedPtr> discLight(m, "DiscLight");
        discLight.def(pybind11::init(&DiscLight::create), "name"_a = "");

        pybind11::class_<SphereLight, AnalyticAreaLight, SphereLight::SharedPtr> sphereLight(m, "SphereLight");
        sphereLight.def(pybind11::init(&SphereLight::create), "name"_a = "");
   }
}
