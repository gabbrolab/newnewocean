#include "Camera.h"
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
            }
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
        {glm::normalize(glm::vec2(1.00f, 0.15f)), 1.25f, 48.0f, 0.16f, 0.0f},
        {glm::normalize(glm::vec2(0.75f, 0.66f)), 0.75f, 32.0f, 0.14f, 1.3f},
        {glm::normalize(glm::vec2(0.20f, 0.98f)), 0.45f, 22.0f, 0.12f, 3.1f},
        {glm::normalize(glm::vec2(-0.35f, 0.94f)), 0.24f, 14.0f, 0.10f, 4.6f},
        {glm::normalize(glm::vec2(0.93f, -0.36f)), 0.16f, 9.5f, 0.08f, 2.2f},
        {glm::normalize(glm::vec2(0.45f, 0.89f)), 0.10f, 6.8f, 0.07f, 5.4f},
        {glm::normalize(glm::vec2(-0.85f, 0.52f)), 0.065f, 4.8f, 0.06f, 0.7f},
        {glm::normalize(glm::vec2(0.14f, -0.99f)), 0.035f, 3.2f, 0.04f, 2.8f},
        {glm::normalize(glm::vec2(0.99f, 0.05f)), 0.022f, 2.2f, 0.03f, 4.1f},
        {glm::normalize(glm::vec2(-0.55f, -0.83f)), 0.014f, 1.5f, 0.02f, 5.8f},
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
    glClearColor(0.04f, 0.07f, 0.10f, 1.0f);

    Camera camera(glm::vec3(0.0f, 14.0f, 35.0f));
    gInput.camera = &camera;
    glfwSetCursorPosCallback(window, mouseCallback);

    Shader oceanShader("shaders/ocean.vert", "shaders/ocean.frag");
    Ocean ocean(120.0f, 160);
    const std::vector<GerstnerWave> waves = makeMultipleWaves();

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
            glm::radians(62.0f),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.1f,
            400.0f);

        oceanShader.use();
        oceanShader.setMat4("uModel", glm::mat4(1.0f));
        oceanShader.setMat4("uView", camera.viewMatrix());
        oceanShader.setMat4("uProjection", projection);
        oceanShader.setFloat("uTime", static_cast<float>(glfwGetTime()));
        oceanShader.setInt("uWaveMode", waveMode);
        uploadWaves(oceanShader, waves);
        oceanShader.setVec3("uCameraPosition", camera.position());
        oceanShader.setVec3("uLightDirection", glm::normalize(glm::vec3(-0.42f, 0.74f, -0.52f)));
        oceanShader.setVec3("uBaseColor", glm::vec3(0.05f, 0.22f, 0.28f));
        oceanShader.setFloat("uAlpha", 1.0f);
        ocean.draw();

        oceanShader.setVec3("uBaseColor", glm::vec3(0.62f, 0.84f, 0.88f));
        oceanShader.setFloat("uAlpha", 0.45f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        ocean.draw();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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
