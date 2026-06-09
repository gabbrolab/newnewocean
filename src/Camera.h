#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    explicit Camera(glm::vec3 position);

    glm::mat4 viewMatrix() const;
    glm::vec3 position() const { return position_; }

    void update(float deltaTime, bool forward, bool backward, bool left, bool right, bool up, bool down);
    void rotate(float deltaX, float deltaY);

private:
    glm::vec3 forward() const;
    glm::vec3 right() const;

    glm::vec3 position_;
    float yaw_ = -90.0f;
    float pitch_ = -7.5f;
    float moveSpeed_ = 18.0f;
    float mouseSensitivity_ = 0.12f;
};
