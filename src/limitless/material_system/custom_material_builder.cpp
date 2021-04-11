#include <limitless/material_system/custom_material_builder.hpp>
#include <limitless/core/shader_compiler.hpp>
#include <limitless/assets.hpp>
#include <limitless/material_system/material_compiler.hpp>
#include <limitless/shader_storage.hpp>
#include <iostream>

using namespace LimitlessEngine;

CustomMaterialBuilder& CustomMaterialBuilder::addUniform(std::string name, Uniform* uniform) {
    auto result = dynamic_cast<CustomMaterial&>(*material).uniforms.emplace(std::move(name), uniform);
    if (!result.second) {
        throw std::runtime_error("Failed to add uniform to custom material, already contains.");
    }
    return *this;
}

CustomMaterialBuilder& CustomMaterialBuilder::setVertexCode(std::string vs_code) noexcept {
    static_cast<CustomMaterial&>(*material).vertex_code = std::move(vs_code);
    return *this;
}

CustomMaterialBuilder& CustomMaterialBuilder::setFragmentCode(std::string fs_code) noexcept {
    static_cast<CustomMaterial&>(*material).fragment_code = std::move(fs_code);
    return *this;
}

std::shared_ptr<Material> CustomMaterialBuilder::build(const ModelShaders& model_shaders, const MaterialShaders& material_shaders) {
    // check for initialized ptr
    if (!material) {
        throw std::runtime_error("Forgot to call create(name)!");
    }
    // checks for empty buffer
    if (material->properties.empty() && static_cast<CustomMaterial&>(*material).uniforms.empty()) {
        throw std::runtime_error("Properties & Uniforms cannot be empty.");
    }

    // sets used model shaders
    material->model_shaders = model_shaders;

    {
        std::unique_lock lock(mutex);
        // every custom material is unique
        material->shader_index = next_shader_index++;
    }

    MaterialCompiler compiler {context, assets};
    for (const auto& mat_shader : material_shaders) {
        for (const auto& mod_shader : model_shaders) {
            // Effect custom material shaders can be used if even it is not compiled, because we do not know that modules it will use
            if (mod_shader != ModelShader::Effect) {
                compiler.compile(*material, mat_shader, mod_shader);
            }
        }
    }

    // initializes uniform material buffer using program shader introspection
    if (auto it = std::find_if(model_shaders.begin(), model_shaders.end(), [] (auto& shader) { return shader != ModelShader::Effect; }); it != model_shaders.end()) {
        initializeMaterialBuffer(*material, assets.shaders.get(material_shaders[0], *it, material->shader_index));
    }

    auto shared = std::shared_ptr(std::move(material));
    assets.materials.add(shared->getName(), shared);
    return shared;
}

CustomMaterialBuilder& CustomMaterialBuilder::create(std::string name) {
    material = std::unique_ptr<Material>(new CustomMaterial());
    material->name = std::move(name);
    return *this;
}

CustomMaterialBuilder& CustomMaterialBuilder::setUniforms(decltype(CustomMaterial::uniforms)&& uniforms) {
    static_cast<CustomMaterial&>(*material).uniforms = std::move(uniforms);
    return *this;
}
