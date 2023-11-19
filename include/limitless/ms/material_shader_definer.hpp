#pragma once

#include <string>
#include <unordered_map>
#include <limitless/ms/property.hpp>
#include <limitless/ms/shading.hpp>
#include <limitless/pipeline/shader_type.hpp>

namespace Limitless {
    class Shader;

    namespace ms {
        class Material;

        class MaterialShaderDefiner {
        private:
            static inline std::unordered_map<Property, std::string> SHADER_PROPERTY_DEFINE =
            {
                { Property::Color, "ENGINE_MATERIAL_COLOR" },
                { Property::EmissiveColor, "ENGINE_MATERIAL_EMISSIVE_COLOR" },
                { Property::Diffuse, "ENGINE_MATERIAL_DIFFUSE_TEXTURE" },
                { Property::Normal, "ENGINE_MATERIAL_NORMAL_TEXTURE" },
                { Property::EmissiveMask, "ENGINE_MATERIAL_EMISSIVEMASK_TEXTURE" },
                { Property::BlendMask, "ENGINE_MATERIAL_BLENDMASK_TEXTURE" },
                { Property::MetallicTexture, "ENGINE_MATERIAL_METALLIC_TEXTURE" },
                { Property::RoughnessTexture, "ENGINE_MATERIAL_ROUGHNESS_TEXTURE" },
                { Property::AmbientOcclusionTexture, "ENGINE_MATERIAL_AMBIENT_OCCLUSION_TEXTURE" },
                { Property::ORM, "ENGINE_MATERIAL_ORM_TEXTURE" },
                { Property::TessellationFactor, "ENGINE_MATERIAL_TESSELLATION_FACTOR" },
                { Property::Metallic, "ENGINE_MATERIAL_METALLIC" },
                { Property::Roughness, "ENGINE_MATERIAL_ROUGHNESS" },
                { Property::IoR, "ENGINE_MATERIAL_IOR" },
                { Property::Absorption, "ENGINE_MATERIAL_ABSORPTION" },
            };

            static inline std::unordered_map<Shading, std::string> SHADER_SHADING_DEFINE =
            {
                { Shading::Lit, "ENGINE_MATERIAL_SHADING_REGULAR_MODEL" },
                { Shading::Unlit, "ENGINE_MATERIAL_SHADING_UNLIT_MODEL" },
                { Shading::Cloth, "ENGINE_MATERIAL_SHADING_CLOTH_MODEL" },
                { Shading::Subsurface, "ENGINE_MATERIAL_SHADING_SUBSURFACE_MODEL" },
                { Shading::Custom, "ENGINE_MATERIAL_SHADING_CUSTOM_MODEL" },
            };

            static inline std::unordered_map<InstanceType, std::string> SHADER_MODEL_DEFINE =
            {
                { InstanceType::Model, "ENGINE_MATERIAL_REGULAR_MODEL" },
                { InstanceType::Skeletal, "ENGINE_MATERIAL_SKELETAL_MODEL" },
                { InstanceType::Instanced, "ENGINE_MATERIAL_INSTANCED_MODEL" },
                { InstanceType::SkeletalInstanced, "ENGINE_MATERIAL_SKELETAL_INSTANCED_MODEL" },
                { InstanceType::Effect, "ENGINE_MATERIAL_EFFECT_MODEL" },
            };

            enum class SnippetDefineType {
                Vertex,
                Tesselation,
                Fragment,
                Global,
                CustomScalar,
                CustomSamplers
            };

            static inline std::unordered_map<SnippetDefineType, std::string> SHADER_SNIPPET_DEFINE =
            {
                { SnippetDefineType::Vertex, "ENGINE_MATERIAL_VERTEX_SNIPPET" },
                { SnippetDefineType::Tesselation, "ENGINE_MATERIAL_TESSELATION_SNIPPET" },
                { SnippetDefineType::Fragment, "ENGINE_MATERIAL_FRAGMENT_SNIPPET" },
                { SnippetDefineType::Global, "ENGINE_MATERIAL_GLOBAL_DEFINITIONS" },
                { SnippetDefineType::CustomScalar, "ENGINE_MATERIAL_CUSTOM_SCALARS" },
                { SnippetDefineType::CustomSamplers, "ENGINE_MATERIAL_CUSTOM_SAMPLERS" },
            };

            static std::string getPropertyDefines(const Material& material);
            static std::string getShadingDefines(const Material& material);
            static std::string getModelDefines(InstanceType model_shader);
            static std::string getMaterialDefines(const Material& material);
            static std::string getScalarUniformDefines(const Material& material);
            static std::string getSamplerUniformDefines(const Material& material);

            MaterialShaderDefiner() noexcept = default;
        public:
            static void define(Shader &shader, const Material& material, InstanceType model_shader);
        };
    }
}