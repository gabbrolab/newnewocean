#include "Camera.h"
#include "FftOcean.h"
#include "Ocean.h"
#include "Shader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
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
    bool showWire = false;
    std::string quality = "medium";
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
        } else if (arg == "--quality" && i + 1 < argc) {
            options.quality = argv[++i];
        } else if (arg == "--wire") {
            options.showWire = true;
        }
    }
    return options;
}

int meshResolutionFor(const std::string& quality)
{
    if (quality == "low") {
        return 256;
    }
    if (quality == "high") {
        return 1024;
    }
    return 512;
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

void setFloatArray(const Shader& shader, const std::string& name, const std::array<float, 4>& values)
{
    for (int i = 0; i < 4; ++i) {
        shader.setFloat(name + "[" + std::to_string(i) + "]", values[i]);
    }
}

void processInput(GLFWwindow* window, Camera& camera, float deltaTime)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
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
    const int meshResolution = meshResolutionFor(options.quality);

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(options.width, options.height, "Gabbro's Lab - FFT Ocean", nullptr, nullptr);
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

    Camera camera(glm::vec3(0.0f, 12.0f, 70.0f));
    gInput.camera = &camera;
    glfwSetCursorPosCallback(window, mouseCallback);

    try {
    Shader oceanShader("shaders/ocean.vert", "shaders/ocean.frag");
    Shader skyShader("shaders/sky.vert", "shaders/sky.frag");

    FftOcean fftOcean;
    const float oceanSize = fftOcean.lengthScales()[0]; // seamless tiling of the largest cascade
    Ocean ocean(oceanSize, meshResolution);

    // A bound vertex array object is required to issue the attribute-less sky draw.
    unsigned int skyVao = 0;
    glGenVertexArrays(1, &skyVao);

    const glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.62f, 0.28f, -0.73f));

    std::cout << "FFT ocean: " << FftOcean::kResolution << "x" << FftOcean::kResolution
              << ", " << fftOcean.cascadeCount() << " cascades"
              << ", mesh " << meshResolution << "x" << meshResolution
              << ", patch " << oceanSize << "m\n";

    auto previousTime = std::chrono::steady_clock::now();
    int renderedFrames = 0;

    while (!glfwWindowShouldClose(window)) {
        const auto currentTime = std::chrono::steady_clock::now();
        const float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;

        processInput(window, camera, deltaTime);
        const float appTime = static_cast<float>(glfwGetTime());

        fftOcean.update(appTime);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 projection = glm::perspective(
            glm::radians(58.0f),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.1f,
            4000.0f);
        const glm::mat4 view = camera.viewMatrix();

        glDepthFunc(GL_LEQUAL);
        skyShader.use();
        skyShader.setMat4("uView", glm::mat4(glm::mat3(view)));
        skyShader.setMat4("uProjection", projection);
        skyShader.setVec3("uLightDirection", sunDirection);
        glBindVertexArray(skyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        oceanShader.use();
        oceanShader.setMat4("uModel", glm::mat4(1.0f));
        oceanShader.setMat4("uView", view);
        oceanShader.setMat4("uProjection", projection);
        oceanShader.setInt("uCascadeCount", fftOcean.cascadeCount());
        setFloatArray(oceanShader, "uLengthScales", fftOcean.lengthScales());
        setFloatArray(oceanShader, "uTiles", fftOcean.tiles());
        oceanShader.setVec3("uCameraPosition", camera.position());
        oceanShader.setVec3("uSunDirection", sunDirection);
        oceanShader.setVec3("uSunColor", glm::vec3(3.0f, 2.7f, 2.3f));
        oceanShader.setVec3("uFogColor", glm::vec3(0.026f, 0.060f, 0.082f));
        oceanShader.setFloat("uNormalStrength", 1.0f);
        oceanShader.setFloat("uRoughness", 0.08f);
        oceanShader.setFloat("uFoamRoughnessModifier", 0.4f);
        oceanShader.setFloat("uHeightModifier", 1.0f);
        oceanShader.setVec3("uScatterColor", glm::vec3(0.03f, 0.10f, 0.13f));
        oceanShader.setVec3("uBubbleColor", glm::vec3(0.0f, 0.02f, 0.03f));
        oceanShader.setVec3("uFoamColor", glm::vec3(0.85f, 0.92f, 0.92f));
        oceanShader.setFloat("uBubbleDensity", 0.45f);
        oceanShader.setFloat("uWavePeakScatterStrength", 1.1f);
        oceanShader.setFloat("uScatterStrength", 0.5f);
        oceanShader.setFloat("uScatterShadowStrength", 0.4f);
        oceanShader.setFloat("uEnvironmentLightStrength", 1.0f);
        oceanShader.setInt("uDisplacement", 0);
        oceanShader.setInt("uSlope", 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, fftOcean.displacementArray());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, fftOcean.slopeArray());

        if (options.showWire) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        ocean.draw();
        if (options.showWire) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glActiveTexture(GL_TEXTURE0);

        ++renderedFrames;
        if (!options.capturePath.empty() && renderedFrames >= options.captureFrames) {
            saveFramebufferBmp(options.capturePath, framebufferWidth, framebufferHeight);
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &skyVao);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
