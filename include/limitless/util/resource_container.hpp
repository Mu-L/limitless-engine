#pragma once

#include <unordered_map>
#include <stdexcept>
#include <memory>
#include <mutex>

namespace LimitlessEngine {
    struct resource_container_error : public std::runtime_error {
        explicit resource_container_error(const std::string& error) : runtime_error(error) {}
        explicit resource_container_error(const char* error) : runtime_error(error) {}
    };

    template<typename T>
    class ResourceContainer {
    private:
        std::unordered_map<std::string, std::shared_ptr<T>> resource;
        std::mutex mutex;
    public:
        auto& operator[](const std::string& name) noexcept {
            std::unique_lock lock(mutex);
            return resource[name];
        }

        const auto& at(const std::string& name) {
            try {
                return resource.at(name);
            } catch (...) {
                throw resource_container_error("No such resource called " + name);
            }
        }

        const auto& at(const std::string& name) const {
            try {
                return resource.at(name);
            } catch (...) {
                throw resource_container_error("No such resource called " + name);
            }
        }

        void add(const std::string& name, std::shared_ptr<T> res) {
            std::unique_lock lock(mutex);
            const auto result = resource.emplace(name, std::move(res));
            if (!result.second) {
                throw resource_container_error("Failed to add resource " + name + ", already exists.");
            }
        }

        void remove(const std::string& name) {
            std::unique_lock lock(mutex);
            resource.erase(name);
        }

        [[nodiscard]] bool exists(const std::string& name) noexcept {
            return resource.find(name) != resource.end();
        }

        auto begin() noexcept { return resource.begin(); }
        auto begin() const noexcept { return resource.begin(); }

        auto end() noexcept { return resource.end(); }
        auto end() const noexcept { return resource.end(); }
    };
}