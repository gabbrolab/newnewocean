#pragma once

#include "Shader.h"

#include <glm/glm.hpp>

#include <array>

// Artist-facing JONSWAP spectrum description for a single wave system.
struct OceanDisplaySpectrum {
    float scale = 0.0f;
    float windSpeed = 1.0f;
    float windDirectionDeg = 0.0f;
    float fetch = 100000.0f;
    float spreadBlend = 1.0f;
    float swell = 0.2f;
    float peakEnhancement = 3.3f;
    float shortWavesFade = 0.01f;
};

// One FFT cascade: a periodic patch of a given world size plus the two wave
// systems whose energy lives in that band.
struct OceanCascade {
    float lengthScale = 256.0f;
    float tile = 1.0f;
    OceanDisplaySpectrum primary;
    OceanDisplaySpectrum secondary; // scale <= 0 disables the second system
};

// GPU FFT ocean simulation. Owns the cascade texture arrays and the compute
// passes that turn a JONSWAP spectrum into displacement, slope and foam maps.
// Mirrors the structure of the reference Unity project (Acerola / gasgiant).
class FftOcean {
public:
    static constexpr int kResolution = 512;
    static constexpr int kCascadeCount = 4;

    FftOcean();
    ~FftOcean();

    FftOcean(const FftOcean&) = delete;
    FftOcean& operator=(const FftOcean&) = delete;

    // Runs the time-evolution -> IFFT -> assemble pipeline for this frame.
    void update(float time);

    unsigned int displacementArray() const { return displacementTexture_; }
    unsigned int slopeArray() const { return slopeTexture_; }

    int cascadeCount() const { return kCascadeCount; }
    const std::array<float, kCascadeCount>& lengthScales() const { return lengthScales_; }
    const std::array<float, kCascadeCount>& tiles() const { return tiles_; }
    glm::vec2 lambda() const { return lambda_; }

private:
    // Packed layout matching the std430 SSBO consumed by the spectrum shaders.
    struct SpectrumParameters {
        float scale;
        float angle;
        float spreadBlend;
        float swell;
        float alpha;
        float peakOmega;
        float gamma;
        float shortWavesFade;
    };

    void createTextures();
    void uploadSpectrumBuffer();
    void generateInitialSpectrum();
    void updateSpectrum(float time);
    void runInverseFft();
    void setLengthScaleUniform(const Shader& shader) const;
    SpectrumParameters toSpectrumParameters(const OceanDisplaySpectrum& settings) const;

    unsigned int initialSpectrumTexture_ = 0;
    unsigned int spectrumTexture_ = 0;
    unsigned int displacementTexture_ = 0;
    unsigned int slopeTexture_ = 0;
    unsigned int spectrumBuffer_ = 0;

    Shader initShader_;
    Shader packShader_;
    Shader updateShader_;
    Shader fftHorizontalShader_;
    Shader fftVerticalShader_;
    Shader assembleShader_;

    std::array<OceanCascade, kCascadeCount> cascades_;
    std::array<float, kCascadeCount> lengthScales_;
    std::array<float, kCascadeCount> tiles_;

    glm::vec2 lambda_ = glm::vec2(1.0f, 1.0f);
    int seed_ = 1337;
    float gravity_ = 9.81f;
    float depth_ = 500.0f;
    float lowCutoff_ = 0.0001f;
    float highCutoff_ = 9000.0f;
    float repeatTime_ = 200.0f;

    float foamBias_ = -0.8f;
    float foamThreshold_ = 0.0f;
    float foamAdd_ = 0.35f;
    float foamDecayRate_ = 0.06f;
};
