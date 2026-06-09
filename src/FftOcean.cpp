#include "FftOcean.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
constexpr float pi = 3.14159265359f;
constexpr float twoPi = 2.0f * pi;

float square(float value)
{
    return value * value;
}

unsigned char toByte(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return static_cast<unsigned char>(value * 255.0f + 0.5f);
}
}

FftOcean::FftOcean(FftOceanConfig config, SpectrumParameters spectrum)
    : config_(config),
      spectrum_(spectrum)
{
    if (config_.resolution <= 0 || (config_.resolution & (config_.resolution - 1)) != 0) {
        throw std::runtime_error("FFT ocean resolution must be a positive power of two.");
    }
    if (config_.patchLength <= 0.0f) {
        throw std::runtime_error("FFT ocean patch length must be positive.");
    }

    spectrum_.windDirection = glm::normalize(spectrum_.windDirection);
    generateInitialSpectrum();
    updateStats();
}

float FftOcean::jonswapSpectrum(const glm::vec2& k) const
{
    const float kLength = glm::length(k);
    if (kLength < spectrum_.lowCutoff || kLength > spectrum_.highCutoff) {
        return 0.0f;
    }

    const float omega = std::sqrt(spectrum_.gravity * kLength);
    if (omega <= 0.0f) {
        return 0.0f;
    }

    const float fetch = std::max(spectrum_.fetch, 1.0f);
    const float windSpeed = std::max(spectrum_.windSpeed, 0.1f);
    const float peakOmega = 22.0f * std::pow(spectrum_.gravity * spectrum_.gravity / (windSpeed * fetch), 1.0f / 3.0f);
    const float sigma = omega <= peakOmega ? 0.07f : 0.09f;
    const float r = std::exp(-square(omega - peakOmega) / (2.0f * square(sigma) * square(peakOmega)));

    const float alpha = 0.076f * std::pow(windSpeed * windSpeed / (spectrum_.gravity * fetch), 0.22f);
    const float base = alpha * spectrum_.gravity * spectrum_.gravity
        * std::exp(-1.25f * std::pow(peakOmega / omega, 4.0f))
        / std::pow(omega, 5.0f);
    const float jonswap = base * std::pow(spectrum_.gamma, r);

    const glm::vec2 direction = k / kLength;
    const float downwind = std::max(glm::dot(direction, spectrum_.windDirection), 0.0f);
    const float spreading = std::pow(downwind, spectrum_.directionalSpreadPower);
    const float jacobian = spectrum_.gravity / (2.0f * std::max(omega, 0.0001f));

    return spectrum_.amplitudeScale * jonswap * spreading * jacobian;
}

void FftOcean::generateInitialSpectrum()
{
    const int n = config_.resolution;
    initialSpectrum_.assign(static_cast<size_t>(n * n), std::complex<float>(0.0f, 0.0f));

    std::mt19937 rng(config_.seed);
    std::normal_distribution<float> gaussian(0.0f, 1.0f);
    const float dk = twoPi / config_.patchLength;

    for (int y = 0; y < n; ++y) {
        const int signedY = y < n / 2 ? y : y - n;
        for (int x = 0; x < n; ++x) {
            const int signedX = x < n / 2 ? x : x - n;
            const glm::vec2 k(static_cast<float>(signedX) * dk, static_cast<float>(signedY) * dk);
            const float spectrumValue = jonswapSpectrum(k);
            const float scale = std::sqrt(std::max(spectrumValue, 0.0f)) * 0.70710678f;
            initialSpectrum_[static_cast<size_t>(y * n + x)] = std::complex<float>(
                gaussian(rng) * scale,
                gaussian(rng) * scale);
        }
    }

    initialSpectrum_[0] = std::complex<float>(0.0f, 0.0f);
}

void FftOcean::updateStats()
{
    double totalMagnitude = 0.0;
    double totalEnergy = 0.0;
    float maxMagnitude = 0.0f;
    bool invalid = false;

    for (const std::complex<float>& value : initialSpectrum_) {
        const float magnitude = std::abs(value);
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag()) || !std::isfinite(magnitude)) {
            invalid = true;
            continue;
        }

        maxMagnitude = std::max(maxMagnitude, magnitude);
        totalMagnitude += magnitude;
        totalEnergy += static_cast<double>(magnitude) * static_cast<double>(magnitude);
    }

    stats_.maxMagnitude = maxMagnitude;
    stats_.averageMagnitude = initialSpectrum_.empty()
        ? 0.0f
        : static_cast<float>(totalMagnitude / static_cast<double>(initialSpectrum_.size()));
    stats_.totalEnergy = static_cast<float>(totalEnergy);
    stats_.hasInvalidValues = invalid;
}

void FftOcean::saveSpectrumDebugImage(const std::filesystem::path& path) const
{
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    const int n = config_.resolution;
    const int rowStride = (n * 3 + 3) & ~3;
    const int pixelDataSize = rowStride * n;
    const int fileSize = 54 + pixelDataSize;
    std::vector<unsigned char> bmp(static_cast<size_t>(fileSize), 0);

    bmp[0] = 'B';
    bmp[1] = 'M';
    *reinterpret_cast<int*>(&bmp[2]) = fileSize;
    *reinterpret_cast<int*>(&bmp[10]) = 54;
    *reinterpret_cast<int*>(&bmp[14]) = 40;
    *reinterpret_cast<int*>(&bmp[18]) = n;
    *reinterpret_cast<int*>(&bmp[22]) = n;
    *reinterpret_cast<short*>(&bmp[26]) = 1;
    *reinterpret_cast<short*>(&bmp[28]) = 24;
    *reinterpret_cast<int*>(&bmp[34]) = pixelDataSize;

    const float invMax = stats_.maxMagnitude > 0.0f ? 1.0f / std::log(1.0f + stats_.maxMagnitude) : 0.0f;

    for (int y = 0; y < n; ++y) {
        unsigned char* dst = bmp.data() + 54 + y * rowStride;
        for (int x = 0; x < n; ++x) {
            const int sourceX = (x + n / 2) % n;
            const int sourceY = (y + n / 2) % n;
            const float magnitude = std::abs(initialSpectrum_[static_cast<size_t>(sourceY * n + sourceX)]);
            const float value = invMax > 0.0f ? std::log(1.0f + magnitude) * invMax : 0.0f;
            const unsigned char shade = toByte(std::pow(value, 0.45f));
            dst[x * 3 + 0] = shade;
            dst[x * 3 + 1] = shade;
            dst[x * 3 + 2] = shade;
        }
    }

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
}
