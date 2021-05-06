#pragma once

#include <limitless/fx/modules/module.hpp>

namespace Limitless::fx {
    template<typename Particle>
    class ColorByLife : public Module<Particle> {
    private:
        std::unique_ptr<Distribution<glm::vec4>> distribution;
    public:
        explicit ColorByLife(std::unique_ptr<Distribution<glm::vec4>> _distribution) noexcept
            : Module<Particle>(ModuleType::ColorByLife)
            , distribution {std::move(_distribution)} {}
        ~ColorByLife() override = default;

        ColorByLife(const ColorByLife& module)
            : Module<Particle>(module.type)
            , distribution {module.distribution->clone()} {}

        void update([[maybe_unused]] AbstractEmitter& emitter, std::vector<Particle>& particles, float dt, [[maybe_unused]] Context& ctx, [[maybe_unused]] const Camera& camera) noexcept override {
            for (auto& particle : particles) {
                const auto tick = particle.lifetime / dt;
                const auto tick_color = glm::abs(distribution->get() - particle.color) / tick;
                particle.color += tick_color;
            }
        }

        [[nodiscard]] ColorByLife* clone() const override {
            return new ColorByLife(*this);
        }

        [[nodiscard]] auto& getDistribution() noexcept { return distribution; }
    };
}