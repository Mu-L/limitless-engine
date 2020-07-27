#pragma once

#include <model_instance.hpp>
#include <lighting.hpp>

namespace GraphicsEngine {
    class Scene {
    public:
        Lighting lighting;
    private:
        std::unordered_map<uint64_t, std::unique_ptr<AbstractInstance>> instances;

        friend class Render;

        void updateSkeletalInstances() const noexcept;
        void removeDeadInstances() noexcept;
    public:
        template<typename T>
        T& addInstance(T* instance) noexcept {
            instances.emplace(instance->getId(), instance);
            return *instance;
        }

        AbstractInstance& getInstance(uint64_t id) const;

        void update();
    };
}