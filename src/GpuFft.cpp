#include "GpuFft.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int log2PowerOfTwo(int value)
{
    int bits = 0;
    while ((1 << bits) < value) {
        ++bits;
    }
    return bits;
}

GLuint createComplexTexture(int resolution, const std::vector<float>* data)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RG32F,
        resolution,
        resolution,
        0,
        GL_RG,
        GL_FLOAT,
        data != nullptr ? data->data() : nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void dispatch2D(int width, int height)
{
    const GLuint groupsX = static_cast<GLuint>((width + 7) / 8);
    const GLuint groupsY = static_cast<GLuint>((height + 7) / 8);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void bindImages(GLuint source, GLuint destination)
{
    glBindImageTexture(0, source, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
    glBindImageTexture(1, destination, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
}

void bitReversePass(const Shader& shader, GLuint source, GLuint destination, int resolution, bool horizontal)
{
    shader.use();
    shader.setInt("uResolution", resolution);
    shader.setInt("uBits", log2PowerOfTwo(resolution));
    shader.setInt("uHorizontal", horizontal ? 1 : 0);
    bindImages(source, destination);
    dispatch2D(resolution, resolution);
}

GLuint stagePasses(const Shader& shader, GLuint source, GLuint destination, int resolution, bool horizontal)
{
    GLuint currentSource = source;
    GLuint currentDestination = destination;

    for (int halfSize = 1; halfSize < resolution; halfSize *= 2) {
        shader.use();
        shader.setInt("uResolution", resolution);
        shader.setInt("uHalfSize", halfSize);
        shader.setInt("uHorizontal", horizontal ? 1 : 0);
        shader.setInt("uInverse", 1);
        bindImages(currentSource, currentDestination);
        dispatch2D(resolution / 2, resolution);
        std::swap(currentSource, currentDestination);
    }

    return currentSource;
}

GLuint inverse1DPass(const Shader& bitReverseShader, const Shader& stageShader, GLuint source, GLuint scratch, int resolution, bool horizontal)
{
    bitReversePass(bitReverseShader, source, scratch, resolution, horizontal);
    return stagePasses(stageShader, scratch, source, resolution, horizontal);
}
}

GpuFftTransform::GpuFftTransform(int resolution)
    : resolution_(resolution),
      bitReverseShader_("shaders/fft_bit_reverse.comp"),
      stageShader_("shaders/fft_stage.comp"),
      scaleShader_("shaders/fft_scale.comp")
{
    if (resolution_ <= 0 || (resolution_ & (resolution_ - 1)) != 0) {
        throw std::runtime_error("GPU FFT resolution must be a positive power of two.");
    }

    scratchTexture_ = createComplexTexture(resolution_, nullptr);
}

GpuFftTransform::~GpuFftTransform()
{
    if (scratchTexture_ != 0) {
        glDeleteTextures(1, &scratchTexture_);
    }
}

unsigned int GpuFftTransform::inverse(unsigned int sourceTexture)
{
    unsigned int current = inverse1DPass(bitReverseShader_, stageShader_, sourceTexture, scratchTexture_, resolution_, true);
    unsigned int scratch = current == sourceTexture ? scratchTexture_ : sourceTexture;
    current = inverse1DPass(bitReverseShader_, stageShader_, current, scratch, resolution_, false);
    scratch = current == sourceTexture ? scratchTexture_ : sourceTexture;

    scaleShader_.use();
    scaleShader_.setInt("uResolution", resolution_);
    scaleShader_.setFloat("uScale", 1.0f / static_cast<float>(resolution_ * resolution_));
    bindImages(current, scratch);
    dispatch2D(resolution_, resolution_);
    return scratch;
}

GpuFftStats GpuFftSelfTest::runInverseTransform(int resolution)
{
    if (resolution <= 0 || (resolution & (resolution - 1)) != 0) {
        throw std::runtime_error("GPU FFT self test resolution must be a positive power of two.");
    }

    std::vector<float> input(static_cast<size_t>(resolution * resolution * 2), 0.0f);
    const int frequencyX = 4;
    const int frequencyY = 7;
    input[static_cast<size_t>((frequencyY * resolution + frequencyX) * 2 + 0)] = static_cast<float>(resolution * resolution);

    GLuint textureA = createComplexTexture(resolution, &input);
    GLuint textureB = createComplexTexture(resolution, nullptr);

    Shader bitReverseShader("shaders/fft_bit_reverse.comp");
    Shader stageShader("shaders/fft_stage.comp");
    Shader scaleShader("shaders/fft_scale.comp");

    GLuint current = inverse1DPass(bitReverseShader, stageShader, textureA, textureB, resolution, true);
    GLuint scratch = current == textureA ? textureB : textureA;
    current = inverse1DPass(bitReverseShader, stageShader, current, scratch, resolution, false);
    scratch = current == textureA ? textureB : textureA;

    scaleShader.use();
    scaleShader.setInt("uResolution", resolution);
    scaleShader.setFloat("uScale", 1.0f / static_cast<float>(resolution * resolution));
    bindImages(current, scratch);
    dispatch2D(resolution, resolution);
    current = scratch;

    std::vector<float> output(static_cast<size_t>(resolution * resolution * 2), 0.0f);
    glBindTexture(GL_TEXTURE_2D, current);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, output.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    GpuFftStats stats;
    stats.minValue = std::numeric_limits<float>::max();
    stats.maxValue = std::numeric_limits<float>::lowest();
    double totalAbs = 0.0;

    for (int i = 0; i < resolution * resolution; ++i) {
        const float value = output[static_cast<size_t>(i * 2)];
        if (!std::isfinite(value)) {
            stats.hasInvalidValues = true;
            continue;
        }
        stats.minValue = std::min(stats.minValue, value);
        stats.maxValue = std::max(stats.maxValue, value);
        totalAbs += std::abs(value);
    }

    stats.averageAbsValue = static_cast<float>(totalAbs / static_cast<double>(resolution * resolution));

    glDeleteTextures(1, &textureA);
    glDeleteTextures(1, &textureB);

    return stats;
}
