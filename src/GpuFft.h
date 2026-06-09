#pragma once

struct GpuFftStats {
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float averageAbsValue = 0.0f;
    bool hasInvalidValues = false;
};

class GpuFftSelfTest {
public:
    static GpuFftStats runInverseTransform(int resolution);
};
