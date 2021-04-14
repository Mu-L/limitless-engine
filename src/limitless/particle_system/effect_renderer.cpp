#include <limitless/particle_system/effect_renderer.hpp>

#include <limitless/instances/effect_instance.hpp>
#include <limitless/material_system/material.hpp>
#include <limitless/particle_system/emitter.hpp>
#include <limitless/core/context.hpp>
#include <limitless/core/uniform_setter.hpp>

using namespace LimitlessEngine;

void EffectRenderer::visitEmitters(const std::vector<AbstractInstance*>& instances, EmitterVisitor& emitter_visitor) noexcept {
    auto effect_instance_visitor = [] (const EffectInstance& effect_instance, EmitterVisitor& visitor) {
        for (const auto& [name, emitter] : effect_instance) {
            emitter->accept(visitor);
        }
    };

    // iterates through effect instance attachments
    for (const auto* instance : instances)
        for (const auto& effect_attachment : *instance)
            effect_instance_visitor(*effect_attachment.instance, emitter_visitor);

    // iterates through effect instances itself
    for (const auto* instance : instances) {
        if (instance->getShaderType() == ModelShader::Effect) {
            effect_instance_visitor(static_cast<const EffectInstance&>(*instance), emitter_visitor);
        }
    }
}

void EffectRenderer::addStorage(const std::vector<AbstractInstance*>& instances) noexcept {
    EmitterStorageFiller filler{sprite_renderer, mesh_renderer};
    visitEmitters(instances, filler);
}

void EffectRenderer::update(const std::vector<AbstractInstance*>& instances) {
    addStorage(instances);

    for (auto& [emitter, storage] : sprite_renderer) {
        SpriteParticleCollector collector{emitter};
        visitEmitters(instances, collector);
        storage.update(collector);
    }

    for (auto& [emitter, storage] : mesh_renderer) {
        MeshParticleCollector collector{emitter};
        visitEmitters(instances, collector);
        storage.update(collector);
    }
}

void EffectRenderer::draw(const Assets& assets, MaterialShader shader, Blending blending, const UniformSetter& setter) {
    for (auto& [emitter, storage] : sprite_renderer) {
        if (emitter.material->getBlending() == blending)
            storage.draw(assets, shader, emitter, setter);
    }
    for (auto& [emitter, storage] : mesh_renderer) {
        if (emitter.material->getBlending() == blending)
            storage.draw(assets, shader, emitter, setter);
    }
}

EffectRenderer::EffectRenderer(Context &context) noexcept {
    context.enable(Capabilities::ProgramPointSize);
}
