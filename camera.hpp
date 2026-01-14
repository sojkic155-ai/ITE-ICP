#pragma once

#include <iostream>
#include <opencv2/opencv.hpp>
#include "assets.hpp"
#include <GLFW/glfw3.h> // input handling (keys, mouse)

class Camera {
public:
    // Camera basis / state (updated by updateCameraVectors()).
    glm::vec3 Position{};
    glm::vec3 Front{};
    glm::vec3 Right{};
    glm::vec3 Up{}; // camera local up vector

    // Euler angles (in degrees).
    GLfloat Yaw = -90.0f;
    GLfloat Pitch = 0.0f;
    GLfloat Roll = 0.0f;

    // Movement tuning.
    GLfloat MovementSpeed = 10.0f;
    GLfloat MouseSensitivity = 0.15f;

    // Simple vertical physics (jump + gravity).
    bool onground = false;
    float gravity = -9.81f;
    glm::vec3 Velocity{ 0.0f, 0.0f, 0.0f };

    Camera(void) = default;

    Camera(glm::vec3 position) : Position(position) {
        this->Up = glm::vec3(0.0f, 1.0f, 0.0f);
        this->updateCameraVectors();
    }

    // Build the view matrix from current position and orientation.
    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(this->Position, this->Position + this->Front, this->Up);
    }

    // Read WASD + SPACE and integrate movement for this frame.
    glm::vec3 ProcessInput(GLFWwindow* window, GLfloat deltaTime)
    {
        glm::vec3 direction{ 0 };

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction += Front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction += -Front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) direction += -Right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction += Right;

        // Jump only when grounded.
        if (onground && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            Velocity.y = 7.0f;  // jump impulse
            onground = false;
        }

        // Apply gravity while in the air.
        if (!onground) {
            Velocity.y += gravity * deltaTime;
        }

        // Move only in XZ plane (no flying when looking up/down).
        if (direction != glm::vec3{ 0 }) {
            direction = glm::normalize(glm::vec3(direction.x, 0.0f, direction.z));
            Velocity.x = direction.x * MovementSpeed;
            Velocity.z = direction.z * MovementSpeed;
        }
        else {
            Velocity.x = 0.0f;
            Velocity.z = 0.0f;
        }

        this->Position += Velocity * deltaTime;
        return this->Position;
    }

    // Apply mouse deltas to yaw/pitch and update camera basis vectors.
    void ProcessMouseMovement(GLfloat xoffset, GLfloat yoffset, GLboolean constraintPitch = GL_TRUE)
    {
        xoffset *= this->MouseSensitivity;
        yoffset *= this->MouseSensitivity;

        this->Yaw += xoffset;
        this->Pitch += yoffset;

        if (constraintPitch)
        {
            if (this->Pitch > 89.0f) this->Pitch = 89.0f;
            if (this->Pitch < -89.0f) this->Pitch = -89.0f;
        }

        this->updateCameraVectors();
    }

private:
    // Recompute Front/Right/Up from current yaw/pitch.
    void updateCameraVectors()
    {
        glm::vec3 front;
        front.x = cos(glm::radians(this->Yaw)) * cos(glm::radians(this->Pitch));
        front.y = sin(glm::radians(this->Pitch));
        front.z = sin(glm::radians(this->Yaw)) * cos(glm::radians(this->Pitch));

        this->Front = glm::normalize(front);
        this->Right = glm::normalize(glm::cross(this->Front, glm::vec3(0.0f, 1.0f, 0.0f)));
        this->Up = glm::normalize(glm::cross(this->Right, this->Front));
    }
};
