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

float PrototypeHeightField::sample(float x, float z) const
{
    if (resolution <= 0 || heights.empty() || patchLength <= 0.0f) {
        return 0.0f;
    }

    const float uWrapped = x / patchLength - std::floor(x / patchLength);
    const float vWrapped = z / patchLength - std::floor(z / patchLength);
    const float fx = uWrapped * static_cast<float>(resolution);
    const float fz = vWrapped * static_cast<float>(resolution);
    const int x0 = static_cast<int>(std::floor(fx)) % resolution;
    const int z0 = static_cast<int>(std::floor(fz)) % resolution;
    const int x1 = (x0 + 1) % resolution;
    const int z1 = (z0 + 1) % resolution;
    const float tx = fx - std::floor(fx);
    const float tz = fz - std::floor(fz);

    const float h00 = heights[static_cast<size_t>(z0 * resolution + x0)];
    const float h10 = heights[static_cast<size_t>(z0 * resolution + x1)];
    const float h01 = heights[static_cast<size_t>(z1 * resolution + x0)];
    const float h11 = heights[static_cast<size_t>(z1 * resolution + x1)];
    const float hx0 = h00 + (h10 - h00) * tx;
    const float hx1 = h01 + (h11 - h01) * tx;
    return hx0 + (hx1 - hx0) * tz;
}

glm::vec2 PrototypeHeightField::sampleSlope(float x, float z) const
{
    if (resolution <= 0 || slopes.empty() || patchLength <= 0.0f) {
        return glm::vec2(0.0f);
    }

    const float uWrapped = x / patchLength - std::floor(x / patchLength);
    const float vWrapped = z / patchLength - std::floor(z / patchLength);
    const float fx = uWrapped * static_cast<float>(resolution);
    const float fz = vWrapped * static_cast<float>(resolution);
    const int x0 = static_cast<int>(std::floor(fx)) % resolution;
    const int z0 = static_cast<int>(std::floor(fz)) % resolution;
    const int x1 = (x0 + 1) % resolution;
    const int z1 = (z0 + 1) % resolution;
    const float tx = fx - std::floor(fx);
    const float tz = fz - std::floor(fz);

    const glm::vec2 s00 = slopes[static_cast<size_t>(z0 * resolution + x0)];
    const glm::vec2 s10 = slopes[static_cast<size_t>(z0 * resolution + x1)];
    const glm::vec2 s01 = slopes[static_cast<size_t>(z1 * resolution + x0)];
    const glm::vec2 s11 = slopes[static_cast<size_t>(z1 * resolution + x1)];
    const glm::vec2 sx0 = s00 + (s10 - s00) * tx;
    const glm::vec2 sx1 = s01 + (s11 - s01) * tx;
    return sx0 + (sx1 - sx0) * tz;
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

PrototypeHeightField FftOcean::buildPrototypeHeightField(int outputResolution, float timeSeconds) const
{
    if (outputResolution <= 0 || (outputResolution & (outputResolution - 1)) != 0) {
        throw std::runtime_error("Prototype FFT height field resolution must be a positive power of two.");
    }
    if (outputResolution > config_.resolution) {
        throw std::runtime_error("Prototype FFT height field cannot exceed the source spectrum resolution.");
    }

    PrototypeHeightField field;
    field.resolution = outputResolution;
    field.patchLength = config_.patchLength;
    field.heights.assign(static_cast<size_t>(outputResolution * outputResolution), 0.0f);
    field.slopes.assign(static_cast<size_t>(outputResolution * outputResolution), glm::vec2(0.0f));
    field.minHeight = std::numeric_limits<float>::max();
    field.maxHeight = std::numeric_limits<float>::lowest();

    const int n = config_.resolution;
    const int m = outputResolution;
    const float dk = twoPi / config_.patchLength;
    const float dx = config_.patchLength / static_cast<float>(m);
    const float normalization = 1.0f / static_cast<float>(m * m);

    for (int z = 0; z < m; ++z) {
        for (int x = 0; x < m; ++x) {
            const glm::vec2 position(
                (static_cast<float>(x) - static_cast<float>(m) * 0.5f) * dx,
                (static_cast<float>(z) - static_cast<float>(m) * 0.5f) * dx);

            std::complex<float> height(0.0f, 0.0f);
            for (int ky = -m / 2; ky < m / 2; ++ky) {
                for (int kx = -m / 2; kx < m / 2; ++kx) {
                    if (kx == 0 && ky == 0) {
                        continue;
                    }

                    const int sourceX = (kx + n) % n;
                    const int sourceY = (ky + n) % n;
                    const int sourceNegX = (-kx + n) % n;
                    const int sourceNegY = (-ky + n) % n;
                    const std::complex<float> h0 = initialSpectrum_[static_cast<size_t>(sourceY * n + sourceX)];
                    const std::complex<float> h0Neg = initialSpectrum_[static_cast<size_t>(sourceNegY * n + sourceNegX)];
                    const glm::vec2 k(static_cast<float>(kx) * dk, static_cast<float>(ky) * dk);
                    const float omega = std::sqrt(spectrum_.gravity * glm::length(k));
                    const std::complex<float> positive(std::cos(omega * timeSeconds), std::sin(omega * timeSeconds));
                    const std::complex<float> negative(std::cos(-omega * timeSeconds), std::sin(-omega * timeSeconds));
                    const std::complex<float> evolved = h0 * positive + std::conj(h0Neg) * negative;
                    const float phase = glm::dot(k, position);
                    height += evolved * std::complex<float>(std::cos(phase), std::sin(phase));
                }
            }

            const float finalHeight = height.real() * normalization * 110.0f;
            field.heights[static_cast<size_t>(z * m + x)] = finalHeight;
            field.minHeight = std::min(field.minHeight, finalHeight);
            field.maxHeight = std::max(field.maxHeight, finalHeight);
        }
    }

    const float cellSize = config_.patchLength / static_cast<float>(m);
    for (int z = 0; z < m; ++z) {
        const int zPrev = (z - 1 + m) % m;
        const int zNext = (z + 1) % m;
        for (int x = 0; x < m; ++x) {
            const int xPrev = (x - 1 + m) % m;
            const int xNext = (x + 1) % m;
            const float heightLeft = field.heights[static_cast<size_t>(z * m + xPrev)];
            const float heightRight = field.heights[static_cast<size_t>(z * m + xNext)];
            const float heightDown = field.heights[static_cast<size_t>(zPrev * m + x)];
            const float heightUp = field.heights[static_cast<size_t>(zNext * m + x)];
            field.slopes[static_cast<size_t>(z * m + x)] = glm::vec2(
                (heightRight - heightLeft) / (2.0f * cellSize),
                (heightUp - heightDown) / (2.0f * cellSize));
        }
    }

    return field;
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
