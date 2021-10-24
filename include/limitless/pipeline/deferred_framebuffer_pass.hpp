#pragma once

#include <limitless/pipeline/render_pass.hpp>
#include <limitless/core/framebuffer.hpp>

namespace Limitless {
    class DeferredFramebufferPass final : public RenderPass {
    private:
        Framebuffer framebuffer;
    public:
        DeferredFramebufferPass(Pipeline& pipeline, ContextEventObserver& ctx);
        ~DeferredFramebufferPass() override = default;

        auto& getFramebuffer() noexcept { return framebuffer; }
        auto& getAlbedo() noexcept { return framebuffer.get(FramebufferAttachment::Color0).texture; }
        auto& getNormal() noexcept { return framebuffer.get(FramebufferAttachment::Color1).texture; }
        auto& getProperties() noexcept { return framebuffer.get(FramebufferAttachment::Color2).texture; }
        auto& getEmissive() noexcept { return framebuffer.get(FramebufferAttachment::Color3).texture; }
        auto& getDepth() noexcept { return framebuffer.get(FramebufferAttachment::Depth).texture; }
        auto& getShaded() noexcept { return framebuffer.get(FramebufferAttachment::Color4).texture; }

        void draw(Instances& instances, Context& ctx, const Assets& assets, const Camera& camera, UniformSetter& setter) override;
    };
}