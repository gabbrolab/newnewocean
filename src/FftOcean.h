#pragma once

#include <glm/glm.hpp>

#include <complex>
#include <filesystem>
#include <vector>

struct SpectrumParameters {
    float gravity = 9.81f;
    float windSpeed = 12.0f;
    glm::vec2 windDirection = glm::normalize(glm::vec2(1.0f, 0.28f));
    float fetch = 80000.0f;
    float gamma = 3.3f;
    float amplitudeScale = 0.75f;
    float lowCutoff = 0.015f;
    float highCutoff = 3.25f;
    float directionalSpreadPower = 6.0f;
};

struct FftOceanConfig {
    int resolution = 256;
    float patchLength = 520.0f;
    unsigned int seed = 1337u;
};

struct FftSpectrumStats {
    float maxMagnitude = 0.0f;
    float averageMagnitude = 0.0f;
    float totalEnergy = 0.0f;
    bool hasInvalidValues = false;
};

struct PrototypeHeightField {
    int resolution = 0;
    float patchLength = 0.0f;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    std::vector<float> heights;
    std::vector<glm::vec2> slopes;

    float sample(float x, float z) const;
    glm::vec2 sampleSlope(float x, float z) const;
};

class FftOcean {
public:
    FftOcean(FftOceanConfig config, SpectrumParameters spectrum);

    const FftOceanConfig& config() const { return config_; }
    const SpectrumParameters& spectrum() const { return spectrum_; }
    const FftSpectrumStats& stats() const { return stats_; }
    const std::vector<std::complex<float>>& initialSpectrum() const { return initialSpectrum_; }

    PrototypeHeightField buildPrototypeHeightField(int outputResolution, float timeSeconds) const;
    void saveSpectrumDebugImage(const std::filesystem::path& path) const;

private:
    float jonswapSpectrum(const glm::vec2& k) const;
    void generateInitialSpectrum();
    void updateStats();

    FftOceanConfig config_;
    SpectrumParameters spectrum_;
    std::vector<std::complex<float>> initialSpectrum_;
    FftSpectrumStats stats_;
};
