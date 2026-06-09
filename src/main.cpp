#include "Camera.h"
#include "FftOcean.h"
#include "Ocean.h"
#include "Shader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct AppOptions {
    int width = 1280;
    int height = 720;
    int captureFrames = 2;
    int waveMode = 2;
    bool showWire = false;
    std::string capturePath;
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
        } else if (arg == "--wire") {
            options.showWire = true;
        }
    }
    return options;
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

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(options.width, options.height, "Gabbro's Lab - Gerstner Ocean", nullptr, nullptr);
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

    Camera camera(glm::vec3(-18.0f, 3.1f, 30.0f));
    gInput.camera = &camera;
    glfwSetCursorPosCallback(window, mouseCallback);

    Shader oceanShader("shaders/ocean.vert", "shaders/ocean.frag");
    Shader skyShader("shaders/sky.vert", "shaders/sky.frag");
    Ocean ocean(800.0f, 768);
    const std::vector<GerstnerWave> waves = makeMultipleWaves();
    const FftOcean fftOcean(FftOceanConfig {}, SpectrumParameters {});
    const PrototypeHeightField fftPrototypeHeight = fftOcean.buildPrototypeHeightField(64, 0.0f);
    Ocean fftPrototypeOcean(800.0f, 384, [&fftPrototypeHeight](float x, float z) {
        return fftPrototypeHeight.sample(x, z);
    });
    const FftSpectrumStats& fftStats = fftOcean.stats();
    std::cout << "FFT spectrum: "
              << fftOcean.config().resolution << "x" << fftOcean.config().resolution
              << ", patch " << fftOcean.config().patchLength << "m"
              << ", max |H0| " << fftStats.maxMagnitude
              << ", avg |H0| " << fftStats.averageMagnitude
              << ", energy " << fftStats.totalEnergy
              << (fftStats.hasInvalidValues ? " (invalid values detected)" : "")
              << "\n";
    std::cout << "FFT prototype height: min " << fftPrototypeHeight.minHeight
              << ", max " << fftPrototypeHeight.maxHeight << "\n";
    fftOcean.saveSpectrumDebugImage("build/fft-spectrum-debug.bmp");

    auto previousTime = std::chrono::steady_clock::now();
    int renderedFrames = 0;
    int waveMode = options.waveMode;

    while (!glfwWindowShouldClose(window)) {
        const auto currentTime = std::chrono::steady_clock::now();
        const float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;

        processInput(window, camera, deltaTime, waveMode);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 projection = glm::perspective(
            glm::radians(58.0f),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.1f,
            1400.0f);

        glDepthFunc(GL_LEQUAL);
        skyShader.use();
        skyShader.setMat4("uView", glm::mat4(glm::mat3(camera.viewMatrix())));
        skyShader.setMat4("uProjection", projection);
        const glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.62f, 0.28f, -0.73f));
        skyShader.setVec3("uLightDirection", sunDirection);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDepthFunc(GL_LESS);

        oceanShader.use();
        oceanShader.setMat4("uModel", glm::mat4(1.0f));
        oceanShader.setMat4("uView", camera.viewMatrix());
        oceanShader.setMat4("uProjection", projection);
        oceanShader.setFloat("uTime", static_cast<float>(glfwGetTime()));
        oceanShader.setInt("uWaveMode", waveMode);
        uploadWaves(oceanShader, waves);
        oceanShader.setVec3("uCameraPosition", camera.position());
        oceanShader.setVec3("uLightDirection", sunDirection);
        oceanShader.setVec3("uFogColor", glm::vec3(0.026f, 0.060f, 0.082f));
        oceanShader.setVec3("uBaseColor", glm::vec3(0.05f, 0.22f, 0.28f));
        oceanShader.setFloat("uAlpha", 1.0f);
        if (waveMode == 3) {
            fftPrototypeOcean.draw();
        } else {
            ocean.draw();
        }

        if (options.showWire) {
            oceanShader.setVec3("uBaseColor", glm::vec3(0.62f, 0.84f, 0.88f));
            oceanShader.setFloat("uAlpha", 0.45f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            if (waveMode == 3) {
                fftPrototypeOcean.draw();
            } else {
                ocean.draw();
            }
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
    glfwTerminate();
    return 0;
}
