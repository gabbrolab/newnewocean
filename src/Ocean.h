#pragma once

#include "Mesh.h"

#include <memory>

class Ocean {
public:
    Ocean(float sizeMeters, int resolution);

    void draw() const;
    float sizeMeters() const { return sizeMeters_; }
    int resolution() const { return resolution_; }

private:
    float sizeMeters_ = 0.0f;
    int resolution_ = 0;
    std::unique_ptr<Mesh> mesh_;
};

