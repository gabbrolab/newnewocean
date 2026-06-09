#include "Ocean.h"

#include <vector>

Ocean::Ocean(float sizeMeters, int resolution)
    : sizeMeters_(sizeMeters),
      resolution_(resolution)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(static_cast<size_t>((resolution + 1) * (resolution + 1)));
    indices.reserve(static_cast<size_t>(resolution * resolution * 6));

    const float halfSize = sizeMeters * 0.5f;
    for (int z = 0; z <= resolution; ++z) {
        for (int x = 0; x <= resolution; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(resolution);
            const float v = static_cast<float>(z) / static_cast<float>(resolution);
            vertices.push_back({
                -halfSize + u * sizeMeters,
                0.0f,
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

    mesh_ = std::make_unique<Mesh>(vertices, indices);
}

void Ocean::draw() const
{
    mesh_->draw();
}
