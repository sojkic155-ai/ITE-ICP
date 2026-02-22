#pragma once

// Minimal includes for a header-only camera interface.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h> // only for key constants used by ProcessInput (optional)

/*
  Lightweight Camera class:
  - používá standardní typy (float, bool) místo GL types
  - odstranìny zbyteèné includes (OpenCV, iostream)
  - magic numbers pøevedeny na constexpr
  - lepší porovnání vektorù (epsilon)
*/

class Camera {
public:
    // Camera basis / state (updated by updateCameraVectors()).
    glm::vec3 Position{ 0.0f };
    glm::vec3 Front{ 0.0f, 0.0f, -1.0f };
    glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
    glm::vec3 Up{ 0.0f, 1.0f, 0.0f };

    // Euler angles (degrees)
    float Yaw = -90.0f;
    float Pitch = 0.0f;
    float Roll = 0.0f;

    // Movement tuning
    float MovementSpeed = 10.0f;
    float MouseSensitivity = 0.15f;

    // Vertical physics
    bool onground = false;
    float gravity = -9.81f;
    glm::vec3 Velocity{ 0.0f, 0.0f, 0.0f };

    Camera() = default;
    explicit Camera(const glm::vec3& position) : Position(position) {
        updateCameraVectors();
    }

    // Build view matrix
    glm::mat4 GetViewMatrix() const noexcept {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // Process keyboard state read from GLFW (keeps compatibility).
    // Doporuèené zlepšení: oddìlit polling od logiky (pøedat struct s key states).
    glm::vec3 ProcessInput(GLFWwindow* window, float deltaTime) noexcept {
        constexpr float EPS = 1e-6f;
        glm::vec3 direction{ 0.0f };

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction += Front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction -= Front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) direction -= Right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction += Right;

        const bool walk = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
        const bool crouch = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

        // Jump (only when grounded and not crouching)
        if (onground && !crouch && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            Velocity.y = kJumpVelocity;
            onground = false;
        }

        // gravity integration
        if (!onground) {
            Velocity.y += gravity * deltaTime;
        }

        // horizontal movement (XZ plane)
        if (glm::length(direction) > EPS) {
            direction = glm::normalize(glm::vec3(direction.x, 0.0f, direction.z));
            float speedMult = 1.0f;
            if (crouch) speedMult = 0.25f;
            else if (walk) speedMult = 0.4f;

            Velocity.x = direction.x * MovementSpeed * speedMult;
            Velocity.z = direction.z * MovementSpeed * speedMult;
        } else {
            Velocity.x = 0.0f;
            Velocity.z = 0.0f;
        }

        Position += Velocity * deltaTime;
        return Position;
    }

    // Mouse look (x/y offsets in pixels or arbitrary units).
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
