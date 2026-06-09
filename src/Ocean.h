#pragma once

#include "Mesh.h"

#include <glm/glm.hpp>

#include <functional>
#include <memory>

struct GerstnerWave {
    glm::vec2 direction;
    float amplitude;
    float wavelength;
    float steepness;
    float phase;
};

class Ocean {
public:
    Ocean(float sizeMeters, int resolution);
    Ocean(float sizeMeters, int resolution, const std::function<float(float, float)>& heightSampler);

    void draw() const;
    void updateHeights(const std::function<float(float, float)>& heightSampler);
    float sizeMeters() const { return sizeMeters_; }
    int resolution() const { return resolution_; }

private:
    float sizeMeters_ = 0.0f;
    int resolution_ = 0;
    std::vector<Vertex> vertices_;
    std::unique_ptr<Mesh> mesh_;
};
