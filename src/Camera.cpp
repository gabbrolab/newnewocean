#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

Camera::Camera(glm::vec3 position)
    : position_(position)
{
}

glm::mat4 Camera::viewMatrix() const
{
    return glm::lookAt(position_, position_ + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::update(float deltaTime, bool forwardKey, bool backward, bool left, bool rightKey, bool up, bool down)
{
    glm::vec3 movement(0.0f);
    if (forwardKey) {
        movement += forward();
    }
    if (backward) {
        movement -= forward();
    }
    if (left) {
        movement -= right();
    }
    if (rightKey) {
        movement += right();
    }
    if (up) {
        movement += glm::vec3(0.0f, 1.0f, 0.0f);
    }
    if (down) {
        movement -= glm::vec3(0.0f, 1.0f, 0.0f);
    }

    if (glm::length(movement) > 0.001f) {
        position_ += glm::normalize(movement) * moveSpeed_ * deltaTime;
    }
}

void Camera::rotate(float deltaX, float deltaY)
{
    yaw_ += deltaX * mouseSensitivity_;
    pitch_ -= deltaY * mouseSensitivity_;
    pitch_ = std::clamp(pitch_, -84.0f, 84.0f);
}

glm::vec3 Camera::forward() const
{
    const float yawRad = glm::radians(yaw_);
    const float pitchRad = glm::radians(pitch_);
    return glm::normalize(glm::vec3(
        cos(yawRad) * cos(pitchRad),
        sin(pitchRad),
        sin(yawRad) * cos(pitchRad)));
}

glm::vec3 Camera::right() const
{
    return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

