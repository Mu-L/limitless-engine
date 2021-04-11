#pragma once

#include <vector>

namespace LimitlessEngine {
    enum class MaterialShader {
        Forward,
        Deferred,
        ForwardPlus,
        DirectionalShadow,
        PointShadow,
        ColorPicker,
        Text
    };

    enum class ModelShader {
        Model,
        Skeletal,
        Instanced,
        SkeletalInstanced,
        Effect,
        Text
    };

    using MaterialShaders = std::vector<MaterialShader>;
    using ModelShaders = std::vector<ModelShader>;
}