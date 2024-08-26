#pragma once

#include <limitless/pipeline/pipeline_pass.hpp>

namespace Limitless::fx {
    class EffectRenderer;
}

namespace Limitless {
    /**
     * Depth pre-pass to render only to depth buffer from DeferredFramebufferPass, fragments color gets discarded
     *
     * so we can discard useless fragments later and restore positions from depth for postprocessing effects
     */
    class DepthPass final : public PipelinePass {
    public:
        DepthPass(Pipeline& pipeline);
        ~DepthPass() override = default;

        void draw(InstanceRenderer &renderer, Scene &scene, Context &ctx, const Assets &assets, const Camera &camera, UniformSetter &setter) override;
    };
}