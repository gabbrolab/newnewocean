#pragma once

#include "Shader.h"

struct GpuFftStats {
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float averageAbsValue = 0.0f;
    bool hasInvalidValues = false;
};

class GpuFftTransform {
public:
    explicit GpuFftTransform(int resolution);
    ~GpuFftTransform();

    GpuFftTransform(const GpuFftTransform&) = delete;
    GpuFftTransform& operator=(const GpuFftTransform&) = delete;

    unsigned int inverse(unsigned int sourceTexture);

private:
    int resolution_ = 0;
    unsigned int scratchTexture_ = 0;
    Shader bitReverseShader_;
    Shader stageShader_;
    Shader scaleShader_;
};

class GpuFftSelfTest {
public:
    static GpuFftStats runInverseTransform(int resolution);
};
