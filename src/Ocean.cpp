#include "Ocean.h"

#include <vector>

Ocean::Ocean(float sizeMeters, int resolution)
    : Ocean(sizeMeters, resolution, {})
{
}

Ocean::Ocean(float sizeMeters, int resolution, const std::function<float(float, float)>& heightSampler)
    : sizeMeters_(sizeMeters),
      resolution_(resolution)
{
    std::vector<unsigned int> indices;
    vertices_.reserve(static_cast<size_t>((resolution + 1) * (resolution + 1)));
    indices.reserve(static_cast<size_t>(resolution * resolution * 6));

    const float halfSize = sizeMeters * 0.5f;
    for (int z = 0; z <= resolution; ++z) {
        for (int x = 0; x <= resolution; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(resolution);
            const float v = static_cast<float>(z) / static_cast<float>(resolution);
            vertices_.push_back({
                -halfSize + u * sizeMeters,
                heightSampler ? heightSampler(-halfSize + u * sizeMeters, -halfSize + v * sizeMeters) : 0.0f,
                -halfSize + v * sizeMeters
            });
        }
    }

    for (int z = 0; z < resolution; ++z) {
        for (int x = 0; x < resolution; ++x) {
            const unsigned int row0 = static_cast<unsigned int>(z * (resolution + 1));
            const unsigned int row1 = static_cast<unsigned int>((z + 1) * (resolution + 1));
            const unsigned int i0 = row0 + static_cast<unsigned int>(x);
            const unsigned int i1 = row0 + static_cast<unsigned int>(x + 1);
            const unsigned int i2 = row1 + static_cast<unsigned int>(x);
            const unsigned int i3 = row1 + static_cast<unsigned int>(x + 1);

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    mesh_ = std::make_unique<Mesh>(vertices_, indices);
}

void Ocean::draw() const
{
    mesh_->draw();
}

void Ocean::updateHeights(const std::function<float(float, float)>& heightSampler)
{
    if (!heightSampler || vertices_.empty()) {
        return;
    }

    for (Vertex& vertex : vertices_) {
        vertex.y = heightSampler(vertex.x, vertex.z);
    }
    mesh_->updateVertices(vertices_);
}
