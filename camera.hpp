#pragma once

// Minimal includes for a header-only camera interface.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h> // only for key constants used by ProcessInput (optional)

/*
  Lightweight Camera class:
  - uses standard float/bool types instead of GL types
  - removed unnecessary includes to keep the header lightweight
  - magic numbers converted to constexpr where appropriate
  - small epsilon used to avoid unstable vector normalization
*/

class Camera {
public:
    // Camera basis / state (kept in world space and updated by updateCameraVectors()).
    glm::vec3 Position{ 0.0f };
    glm::vec3 Front{ 0.0f, 0.0f, -1.0f };
    glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
    glm::vec3 Up{ 0.0f, 1.0f, 0.0f };

    // Euler angles in degrees for intuitive editing.
    float Yaw = -90.0f;
    float Pitch = 0.0f;
    float Roll = 0.0f;

    // Movement tuning parameters.
    float MovementSpeed = 10.0f;
    float MouseSensitivity = 0.15f;

    // Vertical physics state.
    bool onground = false;
    float gravity = -9.81f;
    glm::vec3 Velocity{ 0.0f, 0.0f, 0.0f };

    Camera() = default;
    explicit Camera(const glm::vec3& position) : Position(position) {
        updateCameraVectors();
    }

    // Build the view matrix using glm::lookAt with the current basis vectors.
    glm::mat4 GetViewMatrix() const noexcept {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // Process keyboard state polled directly via GLFW. Returns updated position.
    // Note: for testability it is preferable to decouple input polling from logic by
    // passing a key-state struct instead of calling glfwGetKey() here.
    glm::vec3 ProcessInput(GLFWwindow* window, float deltaTime) noexcept {
        constexpr float EPS = 1e-6f;
        glm::vec3 direction{ 0.0f };

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction += Front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction -= Front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) direction -= Right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction += Right;

        const bool walk = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
        const bool crouch = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

        // Jump: only when grounded and not crouching.
        if (onground && !crouch && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            Velocity.y = kJumpVelocity;
            onground = false;
        }

        // Integrate vertical velocity with gravity when airborne.
        if (!onground) {
            Velocity.y += gravity * deltaTime;
        }

        // Horizontal movement constrained to the XZ plane.
        if (glm::length(direction) > EPS) {
            direction = glm::normalize(glm::vec3(direction.x, 0.0f, direction.z));
            float speedMult = 1.0f;
            if (crouch) speedMult = 0.25f;
            else if (walk) speedMult = 0.4f;

            Velocity.x = direction.x * MovementSpeed * speedMult;
            Velocity.z = direction.z * MovementSpeed * speedMult;
        } else {
            // No horizontal input -> zero horizontal velocity (instant stop, could be smoothed).
            Velocity.x = 0.0f;
            Velocity.z = 0.0f;
        }

        // Integrate final position.
        Position += Velocity * deltaTime;
        return Position;
    }

    // Mouse look: apply offsets (in pixels or arbitrary units) to yaw/pitch and update basis.
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) noexcept {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch) {
            if (Pitch > 89.0f) Pitch = 89.0f;
            if (Pitch < -89.0f) Pitch = -89.0f;
        }

        updateCameraVectors();
    }

private:
    static constexpr float kJumpVelocity = 7.0f;

    // Recompute Front/Right/Up vectors from Euler angles. Keeps the camera orthonormal.
    void updateCameraVectors() noexcept {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));

        Front = glm::normalize(front);
        Right = glm::normalize(glm::cross(Front, glm::vec3(0.0f, 1.0f, 0.0f)));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};
