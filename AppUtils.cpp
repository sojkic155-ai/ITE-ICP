#include "AppUtils.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>

void App::update_projection_matrix() {  //Update the projection matrix
    glfwGetFramebufferSize(window, &width, &height);
    if (height <= 0) // avoid division by 0
        height = 1;

    float ratio = static_cast<float>(width) / height;

    projection_matrix = glm::perspective(
        glm::radians(45.0f), // The vertical Field of View
        ratio,               // Aspect Ratio
        0.1f,                // Near clipping plane
        300.0f               // Far clipping plane
    );
    my_shader.setUniform("uP_m", projection_matrix);
}

void App::updateFPS() { // Calculate the FPS of the application by counting the frames for 1 second
    frame_count++;

    TimePoint currentTime = Clock::now();
    float elapsed = std::chrono::duration<float>(currentTime - last_time).count();

    if (elapsed >= 1.0f) {
        fps = frame_count;
        frame_count = 0;
        last_time = currentTime;
    }
}

float App::getTerrainHeight(float x, float z, const std::vector<std::vector<float>>& heightmap) const {
    int terrainWidth = Ground.width;
    int terrainHeight = Ground.height;
    float terrainScale = Ground.scale.x;  // Only need to be changed if the scale of the heightmap is changed
    float fx = (x + terrainHeight / 2.0f) / terrainScale;
    float fz = (z + terrainWidth / 2.0f) / terrainScale;

    int ix = static_cast<int>(fx);
    int iz = static_cast<int>(fz);

    // Clamp to terrain boundaries
    ix = std::max(0, std::min(ix, terrainWidth - 2));
    iz = std::max(0, std::min(iz, terrainHeight - 2));

    // Heights of surrounding points
    float h00 = heightmap[ix][iz];
    float h10 = heightmap[ix + 1][iz];
    float h01 = heightmap[ix][iz + 1];
    float h11 = heightmap[ix + 1][iz + 1];

    // Fractional part for interpolation
    float fracx = fx - ix;
    float fracz = fz - iz;

    // Bilinear interpolation
    float h0 = h00 * (1.0f - fracx) + h10 * fracx;
    float h1 = h01 * (1.0f - fracx) + h11 * fracx;
    return h0 * (1.0f - fracz) + h1 * fracz;
}