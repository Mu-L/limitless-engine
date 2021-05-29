#pragma once

#include <limitless/fx/modules/module.hpp>

namespace Limitless::fx {
    template<typename Particle>
    class SizeByLife : public Module<Particle> {
    private:
        std::unique_ptr<Distribution<float>> distribution;
        float factor;
    public:
        explicit SizeByLife(std::unique_ptr<Distribution<float>> _distribution, float _factor) noexcept
            : Module<Particle>(ModuleType::SizeByLife)
            , distribution {std::move(_distribution)}
            , factor{_factor} {}

        ~SizeByLife() override = default;

        SizeByLife(const SizeByLife& module)
            : Module<Particle>(module.type)
            , distribution {module.distribution->clone()}
            , factor {module.factor} {}

        void update([[maybe_unused]] AbstractEmitter& emitter, std::vector<Particle>& particles, float dt, [[maybe_unused]] Context& ctx, [[maybe_unused]] const Camera& camera) noexcept override {
            for (auto& particle : particles) {
                const auto tick = particle.getLifetime() / dt;
                const auto tick_size = (distribution->get() - particle.getSize()) / tick;
                particle.getSize() += tick_size;
            }
        }

        [[nodiscard]] SizeByLife* clone() const override {
            return new SizeByLife(*this);
        }

        [[nodiscard]] auto& getDistribution() noexcept { return distribution; }
        [[nodiscard]] const auto& getDistribution() const noexcept { return distribution; }
        [[nodiscard]] auto& getFactor() noexcept { return factor; }
        [[nodiscard]] const auto& getFactor() const noexcept { return factor; }
    };

    template<>
    class SizeByLife<MeshParticle> : public Module<MeshParticle> {
    private:
        std::unique_ptr<Distribution<glm::vec3>> distribution;
        float factor;
    public:
        explicit SizeByLife(std::unique_ptr<Distribution<glm::vec3>> _distribution, float _factor) noexcept
            : Module<MeshParticle>(ModuleType::SizeByLife)
            , distribution {std::move(_distribution)}
            , factor{_factor} {}

        ~SizeByLife() override = default;

        SizeByLife(const SizeByLife& module)
            : Module<MeshParticle>(module.type)
            , distribution {module.distribution->clone()}
            , factor {module.factor} {}

        void update([[maybe_unused]] AbstractEmitter& emitter, std::vector<MeshParticle>& particles, float dt, [[maybe_unused]] Context& ctx, [[maybe_unused]] const Camera& camera) noexcept override {
            for (auto& particle : particles) {
                const auto tick = particle.getLifetime() / dt;
                const auto tick_size = (distribution->get() - particle.getSize()) / tick;
                particle.getSize() += tick_size;
            }
        }

        [[nodiscard]] SizeByLife* clone() const override {
            return new SizeByLife(*this);
        }

        [[nodiscard]] auto& getDistribution() noexcept { return distribution; }
        [[nodiscard]] const auto& getDistribution() const noexcept { return distribution; }
        [[nodiscard]] auto& getFactor() noexcept { return factor; }
        [[nodiscard]] const auto& getFactor() const noexcept { return factor; }
    };
}