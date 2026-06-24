#include "FftOcean.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr float pi = 3.14159265358979323846f;

float jonswapAlpha(float gravity, float fetch, float windSpeed)
{
    return 0.076f * std::pow(gravity * fetch / (windSpeed * windSpeed), -0.22f);
}

float jonswapPeakOmega(float gravity, float fetch, float windSpeed)
{
    return 22.0f * std::pow(windSpeed * fetch / (gravity * gravity), -0.33f);
}
}

FftOcean::FftOcean()
    : initShader_("shaders/ocean_spectrum_init.comp"),
      packShader_("shaders/ocean_spectrum_pack.comp"),
      updateShader_("shaders/ocean_spectrum_update.comp"),
      fftHorizontalShader_("shaders/ocean_fft_horizontal.comp"),
      fftVerticalShader_("shaders/ocean_fft_vertical.comp"),
      assembleShader_("shaders/ocean_assemble.comp")
{
    // Four cascades whose world period divides the 1024 m ocean grid so every
    // layer tiles seamlessly. Wind speed and short-wave fade are staggered so
    // each cascade carries a distinct band: long swell -> wind sea -> chop ->
    // ripples.
    cascades_[0] = {1024.0f, 1.0f,
        {0.36f, 14.0f, 30.0f, 300000.0f, 0.5f, 0.8f, 3.3f, 6.0f},
        {0.0f, 1.0f, 0.0f, 100000.0f, 1.0f, 0.0f, 3.3f, 0.0f}};
    cascades_[1] = {256.0f, 1.0f,
        {0.30f, 9.0f, 30.0f, 100000.0f, 0.8f, 0.3f, 3.3f, 1.5f},
        {0.0f, 1.0f, 0.0f, 100000.0f, 1.0f, 0.0f, 3.3f, 0.0f}};
    cascades_[2] = {64.0f, 1.0f,
        {0.24f, 5.0f, 40.0f, 30000.0f, 1.0f, 0.1f, 3.3f, 0.3f},
        {0.0f, 1.0f, 0.0f, 100000.0f, 1.0f, 0.0f, 3.3f, 0.0f}};
    cascades_[3] = {16.0f, 1.0f,
        {0.18f, 3.0f, 25.0f, 10000.0f, 1.0f, 0.0f, 3.3f, 0.04f},
        {0.0f, 1.0f, 0.0f, 100000.0f, 1.0f, 0.0f, 3.3f, 0.0f}};

    for (int i = 0; i < kCascadeCount; ++i) {
        lengthScales_[i] = cascades_[i].lengthScale;
        tiles_[i] = cascades_[i].tile;
    }

    createTextures();
    uploadSpectrumBuffer();
    generateInitialSpectrum();
}

FftOcean::~FftOcean()
{
    const unsigned int textures[] = {
        initialSpectrumTexture_, spectrumTexture_, displacementTexture_, slopeTexture_};
    glDeleteTextures(4, textures);
    if (spectrumBuffer_ != 0) {
        glDeleteBuffers(1, &spectrumBuffer_);
    }
}

FftOcean::SpectrumParameters FftOcean::toSpectrumParameters(const OceanDisplaySpectrum& settings) const
{
    SpectrumParameters params;
    params.scale = settings.scale;
    params.angle = settings.windDirectionDeg / 180.0f * pi;
    params.spreadBlend = settings.spreadBlend;
    params.swell = std::clamp(settings.swell, 0.01f, 1.0f);
    params.alpha = jonswapAlpha(gravity_, settings.fetch, settings.windSpeed);
    params.peakOmega = jonswapPeakOmega(gravity_, settings.fetch, settings.windSpeed);
    params.gamma = settings.peakEnhancement;
    params.shortWavesFade = settings.shortWavesFade;
    return params;
}

void FftOcean::createTextures()
{
    const int levels = static_cast<int>(std::floor(std::log2(static_cast<float>(kResolution)))) + 1;

    auto createArray = [](unsigned int& texture, GLenum internalFormat, int layers, int mipLevels) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipLevels, internalFormat, kResolution, kResolution, layers);
        const GLint minFilter = mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST;
        const GLint magFilter = mipLevels > 1 ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, magFilter);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    };

    createArray(initialSpectrumTexture_, GL_RGBA32F, kCascadeCount, 1);
    createArray(spectrumTexture_, GL_RGBA32F, kCascadeCount * 2, 1);
    createArray(displacementTexture_, GL_RGBA16F, kCascadeCount, levels);
    createArray(slopeTexture_, GL_RG16F, kCascadeCount, levels);

    // The foam channel of the displacement map accumulates across frames, so
    // clear it once to a known zero state.
    std::vector<float> zeros(static_cast<size_t>(kResolution) * kResolution * kCascadeCount * 4, 0.0f);
    glBindTexture(GL_TEXTURE_2D_ARRAY, displacementTexture_);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, kResolution, kResolution, kCascadeCount,
        GL_RGBA, GL_FLOAT, zeros.data());
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void FftOcean::uploadSpectrumBuffer()
{
    std::array<SpectrumParameters, kCascadeCount * 2> spectrums;
    for (int i = 0; i < kCascadeCount; ++i) {
        spectrums[i * 2] = toSpectrumParameters(cascades_[i].primary);
        spectrums[i * 2 + 1] = toSpectrumParameters(cascades_[i].secondary);
    }

    if (spectrumBuffer_ == 0) {
        glGenBuffers(1, &spectrumBuffer_);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, spectrumBuffer_);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(spectrums.size() * sizeof(SpectrumParameters)),
        spectrums.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void FftOcean::setLengthScaleUniform(const Shader& shader) const
{
    for (int i = 0; i < kCascadeCount; ++i) {
        shader.setFloat("uLengthScales[" + std::to_string(i) + "]", lengthScales_[i]);
    }
}

void FftOcean::generateInitialSpectrum()
{
    const GLuint groups = static_cast<GLuint>(kResolution / 8);

    initShader_.use();
    setLengthScaleUniform(initShader_);
    initShader_.setInt("uN", kResolution);
    initShader_.setInt("uSeed", seed_);
    initShader_.setFloat("uGravity", gravity_);
    initShader_.setFloat("uDepth", depth_);
    initShader_.setFloat("uLowCutoff", lowCutoff_);
    initShader_.setFloat("uHighCutoff", highCutoff_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, spectrumBuffer_);
    glBindImageTexture(0, initialSpectrumTexture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    packShader_.use();
    packShader_.setInt("uN", kResolution);
    glBindImageTexture(0, initialSpectrumTexture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void FftOcean::updateSpectrum(float time)
{
    const GLuint groups = static_cast<GLuint>(kResolution / 8);

    updateShader_.use();
    setLengthScaleUniform(updateShader_);
    updateShader_.setInt("uN", kResolution);
    updateShader_.setFloat("uGravity", gravity_);
    updateShader_.setFloat("uRepeatTime", repeatTime_);
    updateShader_.setFloat("uFrameTime", time);
    glBindImageTexture(0, initialSpectrumTexture_, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, spectrumTexture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void FftOcean::runInverseFft()
{
    fftHorizontalShader_.use();
    glBindImageTexture(0, spectrumTexture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glDispatchCompute(1, static_cast<GLuint>(kResolution), 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    fftVerticalShader_.use();
    glBindImageTexture(0, spectrumTexture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glDispatchCompute(1, static_cast<GLuint>(kResolution), 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void FftOcean::assembleMaps()
{
    const GLuint groups = static_cast<GLuint>(kResolution / 8);

    assembleShader_.use();
    assembleShader_.setInt("uN", kResolution);
    assembleShader_.setVec2("uLambda", lambda_);
    assembleShader_.setFloat("uFoamDecayRate", foamDecayRate_);
    assembleShader_.setFloat("uFoamBias", foamBias_);
    assembleShader_.setFloat("uFoamThreshold", foamThreshold_);
    assembleShader_.setFloat("uFoamAdd", foamAdd_);
    glBindImageTexture(0, spectrumTexture_, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, displacementTexture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    glBindImageTexture(2, slopeTexture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RG16F);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    glBindTexture(GL_TEXTURE_2D_ARRAY, displacementTexture_);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, slopeTexture_);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void FftOcean::update(float time)
{
    updateSpectrum(time);
    runInverseFft();
    assembleMaps();
}
