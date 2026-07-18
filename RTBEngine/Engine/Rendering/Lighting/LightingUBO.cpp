#include "LightingUBO.h"

#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "../RHI/RenderDevice.h"

#include <cstring>

namespace RTBEngine {
    namespace Rendering {

        namespace LightingUBOLayout {
            constexpr std::size_t kDirectionalLight = 0;
            constexpr std::size_t kDirIntensity = 28;
            constexpr std::size_t kPointLights = 32;
            constexpr std::size_t kSpotLights = 416;
            constexpr std::size_t kNumPointLights = 1056;
            constexpr std::size_t kNumSpotLights = 1060;
            constexpr std::size_t kBufferSize = 1064;
            constexpr std::size_t kPointLightStride = 48;
            constexpr std::size_t kSpotLightStride = 80;
        }

        namespace {
            void WriteVec3(std::uint8_t* buffer, const std::size_t offset, const Math::Vector3& value)
            {
                std::memcpy(buffer + offset, &value.x, sizeof(float) * 3);
            }

            void WriteFloat(std::uint8_t* buffer, const std::size_t offset, const float value)
            {
                std::memcpy(buffer + offset, &value, sizeof(float));
            }

            void WriteInt32(std::uint8_t* buffer, const std::size_t offset, const std::int32_t value)
            {
                std::memcpy(buffer + offset, &value, sizeof(std::int32_t));
            }

            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }
        }

        LightingUBO& LightingUBO::GetInstance()
        {
            static LightingUBO instance;
            return instance;
        }

        LightingUBO::LightingUBO()
        {
            buffer = Device().CreateBuffer();
            Device().SetUniformBufferData(buffer, nullptr, LightingUBOLayout::kBufferSize, RHI::BufferUsage::Dynamic);
        }

        LightingUBO::~LightingUBO()
        {
            if (buffer != RHI::kInvalidGpuId && RHI::RenderDevice::HasDevice()) {
                Device().DestroyBuffer(buffer);
                buffer = RHI::kInvalidGpuId;
            }
        }

        void LightingUBO::Upload(const std::vector<Light*>& lights)
        {
            std::uint8_t data[LightingUBOLayout::kBufferSize]{};
            WriteVec3(data, LightingUBOLayout::kDirectionalLight + 0, Math::Vector3(0.0f, -1.0f, 0.0f));
            WriteVec3(data, LightingUBOLayout::kDirectionalLight + 16, Math::Vector3::Zero());
            WriteFloat(data, LightingUBOLayout::kDirIntensity, 0.0f);

            bool directionalLightSet = false;
            int pointLightIndex = 0;
            int spotLightIndex = 0;

            for (Light* light : lights) {
                if (!light) {
                    continue;
                }

                switch (light->GetType()) {
                case LightType::Directional:
                    if (!directionalLightSet) {
                        const auto* dirLight = static_cast<const DirectionalLight*>(light);
                        WriteVec3(data, LightingUBOLayout::kDirectionalLight + 0, dirLight->GetDirection());
                        WriteVec3(data, LightingUBOLayout::kDirectionalLight + 16, dirLight->GetColor());
                        WriteFloat(data, LightingUBOLayout::kDirIntensity, dirLight->GetIntensity());
                        directionalLightSet = true;
                    }
                    break;

                case LightType::Point:
                    if (pointLightIndex < kMaxPointLightsUBO) {
                        const auto* pointLight = static_cast<const PointLight*>(light);
                        const std::size_t base =
                            LightingUBOLayout::kPointLights + static_cast<std::size_t>(pointLightIndex) * LightingUBOLayout::kPointLightStride;

                        WriteVec3(data, base + 0, pointLight->GetPosition());
                        WriteVec3(data, base + 16, pointLight->GetColor());
                        WriteFloat(data, base + 28, pointLight->GetIntensity());
                        WriteFloat(data, base + 32, pointLight->GetConstant());
                        WriteFloat(data, base + 36, pointLight->GetLinear());
                        WriteFloat(data, base + 40, pointLight->GetQuadratic());
                        WriteFloat(data, base + 44, pointLight->GetRange());
                        ++pointLightIndex;
                    }
                    break;

                case LightType::Spot:
                    if (spotLightIndex < kMaxSpotLightsUBO) {
                        const auto* spotLight = static_cast<const SpotLight*>(light);
                        const std::size_t base =
                            LightingUBOLayout::kSpotLights + static_cast<std::size_t>(spotLightIndex) * LightingUBOLayout::kSpotLightStride;

                        WriteVec3(data, base + 0, spotLight->GetPosition());
                        WriteVec3(data, base + 16, spotLight->GetDirection());
                        WriteVec3(data, base + 32, spotLight->GetColor());
                        WriteFloat(data, base + 44, spotLight->GetIntensity());
                        WriteFloat(data, base + 48, spotLight->GetInnerCutOff());
                        WriteFloat(data, base + 52, spotLight->GetOuterCutOff());
                        WriteFloat(data, base + 56, spotLight->GetConstant());
                        WriteFloat(data, base + 60, spotLight->GetLinear());
                        WriteFloat(data, base + 64, spotLight->GetQuadratic());
                        WriteFloat(data, base + 68, spotLight->GetRange());
                        ++spotLightIndex;
                    }
                    break;

                default:
                    break;
                }
            }

            WriteInt32(data, LightingUBOLayout::kNumPointLights, pointLightIndex);
            WriteInt32(data, LightingUBOLayout::kNumSpotLights, spotLightIndex);

            Device().UpdateUniformBufferData(buffer, data, LightingUBOLayout::kBufferSize);
        }

        void LightingUBO::Bind() const
        {
            Device().BindUniformBufferBase(buffer, kLightingUBOBindingPoint);
        }

    }
}
