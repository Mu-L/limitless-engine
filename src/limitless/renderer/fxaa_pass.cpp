#include <limitless/renderer/fxaa_pass.hpp>

#include <limitless/core/context.hpp>
#include "limitless/core/uniform/uniform.hpp"
#include <limitless/assets.hpp>
#include "limitless/core/shader/shader_program.hpp"
#include <limitless/pipeline/pipeline.hpp>
#include <limitless/renderer/composite_pass.hpp>
#include <limitless/core/texture/texture_builder.hpp>
#include <limitless/renderer/deferred_framebuffer_pass.hpp>

using namespace Limitless;

FXAAPass::FXAAPass(Pipeline& pipeline, glm::uvec2 frame_size)
    : RendererPass(pipeline)
    , framebuffer {Framebuffer::asRGB8LinearClampToEdge(frame_size)} {
}

std::shared_ptr<Texture> FXAAPass::getResult() {
    return framebuffer.get(FramebufferAttachment::Color0).texture;
}

void FXAAPass::draw(InstanceRenderer &renderer, Scene &scene, Context &ctx, const Assets &assets, const Camera &camera,
                    UniformSetter &setter) {
    ctx.disable(Capabilities::DepthTest);
    ctx.disable(Capabilities::Blending);

    {
        framebuffer.clear();
        auto& shader = assets.shaders.get("fxaa");

        shader.setUniform("scene", getPreviousResult());

        shader.use();

        assets.meshes.at("quad")->draw();
    }
}

void FXAAPass::onFramebufferChange(glm::uvec2 size) {
    framebuffer.onFramebufferChange(size);
}