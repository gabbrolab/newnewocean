#include "Camera.h"
#include "FftOcean.h"
#include "GpuFft.h"
#include "Ocean.h"
#include "Shader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
struct AppOptions {
    int width = 1280;
    int height = 720;
    int captureFrames = 2;
    int waveMode = 2;
    bool showWire = false;
    std::string quality = "medium";
    std::string capturePath;
};

struct QualitySettings {
    int fftSampleResolution = 64;
    int oceanMeshResolution = 384;
    float updateInterval = 0.055f;
    bool useDetailCascade = true;
};

struct InputState {
    bool firstMouse = true;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    Camera* camera = nullptr;
};

InputState gInput;

void framebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED || gInput.camera == nullptr) {
        gInput.firstMouse = true;
        return;
    }

    if (gInput.firstMouse) {
        gInput.lastMouseX = xpos;
        gInput.lastMouseY = ypos;
        gInput.firstMouse = false;
    }

    const float deltaX = static_cast<float>(xpos - gInput.lastMouseX);
    const float deltaY = static_cast<float>(ypos - gInput.lastMouseY);
    gInput.lastMouseX = xpos;
    gInput.lastMouseY = ypos;
    gInput.camera->rotate(deltaX, deltaY);
}

AppOptions parseOptions(int argc, char** argv)
{
    AppOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--capture" && i + 1 < argc) {
            options.capturePath = argv[++i];
        } else if (arg == "--frames" && i + 1 < argc) {
            options.captureFrames = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--width" && i + 1 < argc) {
            options.width = std::max(320, std::stoi(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            options.height = std::max(240, std::stoi(argv[++i]));
        } else if (arg == "--mode" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "flat") {
                options.waveMode = 0;
            } else if (mode == "sine") {
                options.waveMode = 1;
            } else if (mode == "gerstner") {
                options.waveMode = 2;
            } else if (mode == "fft") {
                options.waveMode = 3;
            }
        } else if (arg == "--quality" && i + 1 < argc) {
            options.quality = argv[++i];
        } else if (arg == "--wire") {
            options.showWire = true;
        }
    }
    return options;
}

QualitySettings qualitySettingsFor(const std::string& quality)
{
    if (quality == "low") {
        return {256, 384, 0.033f, false};
    }
    if (quality == "high") {
        return {256, 1024, 0.016f, true};
    }
    return {256, 768, 0.016f, true};
}

void saveFramebufferBmp(const std::filesystem::path& path, int width, int height)
{
    std::vector<unsigned char> rgb(static_cast<size_t>(width * height * 3));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

    const int rowStride = (width * 3 + 3) & ~3;
    const int pixelDataSize = rowStride * height;
    const int fileSize = 54 + pixelDataSize;

    std::vector<unsigned char> bmp(static_cast<size_t>(fileSize), 0);
    bmp[0] = 'B';
    bmp[1] = 'M';
    *reinterpret_cast<int*>(&bmp[2]) = fileSize;
    *reinterpret_cast<int*>(&bmp[10]) = 54;
    *reinterpret_cast<int*>(&bmp[14]) = 40;
    *reinterpret_cast<int*>(&bmp[18]) = width;
    *reinterpret_cast<int*>(&bmp[22]) = height;
    *reinterpret_cast<short*>(&bmp[26]) = 1;
    *reinterpret_cast<short*>(&bmp[28]) = 24;
    *reinterpret_cast<int*>(&bmp[34]) = pixelDataSize;

    for (int y = 0; y < height; ++y) {
        unsigned char* dst = bmp.data() + 54 + y * rowStride;
        const unsigned char* src = rgb.data() + static_cast<size_t>(y * width * 3);
        for (int x = 0; x < width; ++x) {
            dst[x * 3 + 0] = src[x * 3 + 2];
            dst[x * 3 + 1] = src[x * 3 + 1];
            dst[x * 3 + 2] = src[x * 3 + 0];
        }
    }

    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
}

unsigned int createHeightTexture(const PrototypeHeightField& field)
{
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        field.resolution,
        field.resolution,
        0,
        GL_RED,
        GL_FLOAT,
        field.heights.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

unsigned int createComplexTexture(int resolution, const float* data)
{
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, resolution, resolution, 0, GL_RG, GL_FLOAT, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

std::vector<float> packSpectrumTexture(const FftOcean& ocean)
{
    std::vector<float> packed;
    packed.reserve(ocean.initialSpectrum().size() * 2);
    for (const std::complex<float>& value : ocean.initialSpectrum()) {
        packed.push_back(value.real());
        packed.push_back(value.imag());
    }
    return packed;
}

void uploadHeightTexture(unsigned int texture, const PrototypeHeightField& field)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        field.resolution,
        field.resolution,
        GL_RED,
        GL_FLOAT,
        field.heights.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

unsigned int createSlopeTexture(const PrototypeHeightField& field)
{
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RG32F,
        field.resolution,
        field.resolution,
        0,
        GL_RG,
        GL_FLOAT,
        field.slopes.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void uploadSlopeTexture(unsigned int texture, const PrototypeHeightField& field)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        field.resolution,
        field.resolution,
        GL_RG,
        GL_FLOAT,
        field.slopes.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

unsigned int createDisplacementTexture(const PrototypeHeightField& field)
{
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RG32F,
        field.resolution,
        field.resolution,
        0,
        GL_RG,
        GL_FLOAT,
        field.displacements.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void uploadDisplacementTexture(unsigned int texture, const PrototypeHeightField& field)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        field.resolution,
        field.resolution,
        GL_RG,
        GL_FLOAT,
        field.displacements.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

unsigned int createFoamTexture(const PrototypeHeightField& field)
{
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        field.resolution,
        field.resolution,
        0,
        GL_RED,
        GL_FLOAT,
        field.foam.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void uploadFoamTexture(unsigned int texture, const PrototypeHeightField& field)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        field.resolution,
        field.resolution,
        GL_RED,
        GL_FLOAT,
        field.foam.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void accumulateFoam(PrototypeHeightField& field, std::vector<float>& accumulatedFoam, float deltaTime)
{
    if (accumulatedFoam.size() != field.foam.size()) {
        accumulatedFoam = field.foam;
        return;
    }

    const float decay = std::exp(-deltaTime * 0.85f);
    for (size_t i = 0; i < field.foam.size(); ++i) {
        accumulatedFoam[i] = std::max(field.foam[i], accumulatedFoam[i] * decay);
        field.foam[i] = accumulatedFoam[i];
    }
}

PrototypeHeightField combineCascades(const PrototypeHeightField& large, const PrototypeHeightField& detail)
{
    PrototypeHeightField combined = large;
    const float cellSize = large.patchLength / static_cast<float>(large.resolution);
    combined.minHeight = std::numeric_limits<float>::max();
    combined.maxHeight = std::numeric_limits<float>::lowest();

    for (int z = 0; z < large.resolution; ++z) {
        for (int x = 0; x < large.resolution; ++x) {
            const float worldX = (static_cast<float>(x) - static_cast<float>(large.resolution) * 0.5f) * cellSize;
            const float worldZ = (static_cast<float>(z) - static_cast<float>(large.resolution) * 0.5f) * cellSize;
            const size_t index = static_cast<size_t>(z * large.resolution + x);
            combined.heights[index] += detail.sample(worldX, worldZ) * 0.42f;
            combined.slopes[index] += detail.sampleSlope(worldX, worldZ) * 0.58f;
            combined.displacements[index] += detail.sampleDisplacement(worldX, worldZ) * 0.30f;
            combined.foam[index] = std::max(combined.foam[index], detail.sampleFoam(worldX, worldZ) * 0.65f);
            combined.minHeight = std::min(combined.minHeight, combined.heights[index]);
            combined.maxHeight = std::max(combined.maxHeight, combined.heights[index]);
        }
    }

    return combined;
}

std::vector<GerstnerWave> makeMultipleWaves()
{
    std::vector<GerstnerWave> waves = {
        {glm::normalize(glm::vec2(1.00f, 0.15f)), 1.18f, 53.0f, 0.15f, 0.0f},
        {glm::normalize(glm::vec2(0.68f, 0.73f)), 0.68f, 34.7f, 0.13f, 1.9f},
        {glm::normalize(glm::vec2(0.08f, 0.997f)), 0.39f, 23.6f, 0.11f, 4.2f},
        {glm::normalize(glm::vec2(-0.41f, 0.91f)), 0.22f, 15.3f, 0.09f, 2.6f},
        {glm::normalize(glm::vec2(0.96f, -0.27f)), 0.14f, 10.4f, 0.07f, 5.7f},
        {glm::normalize(glm::vec2(0.36f, 0.93f)), 0.085f, 7.1f, 0.055f, 0.8f},
        {glm::normalize(glm::vec2(-0.79f, 0.61f)), 0.052f, 5.2f, 0.045f, 3.4f},
        {glm::normalize(glm::vec2(0.18f, -0.98f)), 0.030f, 3.65f, 0.035f, 2.2f},
        {glm::normalize(glm::vec2(-0.67f, 0.74f)), 0.020f, 2.85f, 0.026f, 5.1f},
        {glm::normalize(glm::vec2(0.91f, 0.41f)), 0.012f, 2.25f, 0.018f, 1.4f},
    };

    constexpr float maxTotalSteepness = 0.90f;
    float totalSteepness = 0.0f;
    for (const GerstnerWave& wave : waves) {
        totalSteepness += wave.steepness;
    }

    if (totalSteepness > maxTotalSteepness) {
        const float scale = maxTotalSteepness / totalSteepness;
        for (GerstnerWave& wave : waves) {
            wave.steepness *= scale;
        }
    }

    return waves;
}

void uploadWaves(const Shader& shader, const std::vector<GerstnerWave>& waves)
{
    shader.setInt("uWaveCount", static_cast<int>(waves.size()));
    for (size_t i = 0; i < waves.size(); ++i) {
        const std::string prefix = "uWaves[" + std::to_string(i) + "].";
        shader.setVec2(prefix + "direction", waves[i].direction);
        shader.setFloat(prefix + "amplitude", waves[i].amplitude);
        shader.setFloat(prefix + "wavelength", waves[i].wavelength);
        shader.setFloat(prefix + "steepness", waves[i].steepness);
        shader.setFloat(prefix + "phase", waves[i].phase);
    }
}

void processInput(GLFWwindow* window, Camera& camera, float deltaTime, int& waveMode)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
        waveMode = 0;
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        waveMode = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        waveMode = 2;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        waveMode = 3;
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    camera.update(
        deltaTime,
        glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS);
}
}

int main(int argc, char** argv)
{
    const AppOptions options = parseOptions(argc, argv);
    const QualitySettings quality = qualitySettingsFor(options.quality);

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(options.width, options.height, "Gabbro's Lab - Ocean Lab", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to load OpenGL functions.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glClearColor(0.04f, 0.07f, 0.10f, 1.0f);

    Camera camera(options.waveMode == 3 ? glm::vec3(-34.0f, 2.35f, 46.0f) : glm::vec3(-18.0f, 3.1f, 30.0f));
    gInput.camera = &camera;
    glfwSetCursorPosCallback(window, mouseCallback);

    Shader oceanShader("shaders/ocean.vert", "shaders/ocean.frag");
    Shader skyShader("shaders/sky.vert", "shaders/sky.frag");
    Ocean ocean(800.0f, 768);
    const std::vector<GerstnerWave> waves = makeMultipleWaves();
    const FftOcean fftOcean(FftOceanConfig {}, SpectrumParameters {});
    Ocean fftPrototypeOcean(920.0f, quality.oceanMeshResolution);
    const std::vector<float> h0Packed = packSpectrumTexture(fftOcean);
    const unsigned int fftH0Texture = createComplexTexture(fftOcean.config().resolution, h0Packed.data());
    const unsigned int fftSpectrumTexture = createComplexTexture(fftOcean.config().resolution, nullptr);
    unsigned int fftHeightTexture = fftSpectrumTexture;
    GpuFftTransform fftTransform(fftOcean.config().resolution);
    Shader fftEvolveShader("shaders/fft_evolve_height.comp");
    const FftSpectrumStats& fftStats = fftOcean.stats();
    std::cout << "FFT spectrum: "
              << fftOcean.config().resolution << "x" << fftOcean.config().resolution
              << ", patch " << fftOcean.config().patchLength << "m"
              << ", max |H0| " << fftStats.maxMagnitude
              << ", avg |H0| " << fftStats.averageMagnitude
              << ", energy " << fftStats.totalEnergy
              << (fftStats.hasInvalidValues ? " (invalid values detected)" : "")
              << "\n";
    std::cout << "FFT quality: " << options.quality
              << ", GPU FFT " << fftOcean.config().resolution << "x" << fftOcean.config().resolution
              << ", mesh " << quality.oceanMeshResolution << "x" << quality.oceanMeshResolution
              << ", update " << quality.updateInterval << "s\n";
    fftOcean.saveSpectrumDebugImage("build/fft-spectrum-debug.bmp");

    auto previousTime = std::chrono::steady_clock::now();
    float previousFftUpdateTime = -1.0f;
    int renderedFrames = 0;
    int waveMode = options.waveMode;

    while (!glfwWindowShouldClose(window)) {
        const auto currentTime = std::chrono::steady_clock::now();
        const float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;

        processInput(window, camera, deltaTime, waveMode);
        const float appTime = static_cast<float>(glfwGetTime());

        if (waveMode == 3 && (previousFftUpdateTime < 0.0f || appTime - previousFftUpdateTime > quality.updateInterval)) {
            fftEvolveShader.use();
            fftEvolveShader.setInt("uResolution", fftOcean.config().resolution);
            fftEvolveShader.setFloat("uPatchLength", fftOcean.config().patchLength);
            fftEvolveShader.setFloat("uTime", appTime * 0.85f);
            fftEvolveShader.setFloat("uGravity", fftOcean.spectrum().gravity);
            fftEvolveShader.setFloat("uHeightScale", 110.0f);
            glBindImageTexture(0, fftH0Texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
            glBindImageTexture(1, fftSpectrumTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
            glDispatchCompute(static_cast<unsigned int>((fftOcean.config().resolution + 7) / 8), static_cast<unsigned int>((fftOcean.config().resolution + 7) / 8), 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
            fftHeightTexture = fftTransform.inverse(fftSpectrumTexture);
            previousFftUpdateTime = appTime;
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 projection = glm::perspective(
            glm::radians(58.0f),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.1f,
            2400.0f);

        glDepthFunc(GL_LEQUAL);
        skyShader.use();
        skyShader.setMat4("uView", glm::mat4(glm::mat3(camera.viewMatrix())));
        skyShader.setMat4("uProjection", projection);
        const glm::vec3 sunDirection = waveMode == 3
            ? glm::normalize(glm::vec3(-0.72f, 0.18f, -0.67f))
            : glm::normalize(glm::vec3(-0.62f, 0.28f, -0.73f));
        skyShader.setVec3("uLightDirection", sunDirection);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDepthFunc(GL_LESS);

        oceanShader.use();
        oceanShader.setMat4("uModel", glm::mat4(1.0f));
        oceanShader.setMat4("uView", camera.viewMatrix());
        oceanShader.setMat4("uProjection", projection);
        oceanShader.setFloat("uTime", appTime);
        oceanShader.setInt("uWaveMode", waveMode);
        oceanShader.setInt("uFftHeightMap", 0);
        oceanShader.setFloat("uFftPatchLength", fftOcean.config().patchLength);
        oceanShader.setFloat("uFftChoppiness", 0.85f);
        uploadWaves(oceanShader, waves);
        oceanShader.setVec3("uCameraPosition", camera.position());
        oceanShader.setVec3("uLightDirection", sunDirection);
        oceanShader.setVec3("uFogColor", waveMode == 3 ? glm::vec3(0.020f, 0.052f, 0.070f) : glm::vec3(0.026f, 0.060f, 0.082f));
        oceanShader.setVec3("uBaseColor", glm::vec3(0.05f, 0.22f, 0.28f));
        oceanShader.setFloat("uAlpha", 1.0f);
        glBindTexture(GL_TEXTURE_2D, fftHeightTexture);
        if (waveMode == 3) {
            fftPrototypeOcean.draw();
        } else {
            ocean.draw();
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        if (options.showWire) {
            oceanShader.setVec3("uBaseColor", glm::vec3(0.62f, 0.84f, 0.88f));
            oceanShader.setFloat("uAlpha", 0.45f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glBindTexture(GL_TEXTURE_2D, fftHeightTexture);
            if (waveMode == 3) {
                fftPrototypeOcean.draw();
            } else {
                ocean.draw();
            }
            glBindTexture(GL_TEXTURE_2D, 0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        ++renderedFrames;
        if (!options.capturePath.empty() && renderedFrames >= options.captureFrames) {
            saveFramebufferBmp(options.capturePath, framebufferWidth, framebufferHeight);
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glDeleteTextures(1, &fftH0Texture);
    glDeleteTextures(1, &fftSpectrumTexture);
    glfwTerminate();
    return 0;
}
