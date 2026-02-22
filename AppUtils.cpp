#include "AppUtils.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>

void App::update_projection_matrix() {  // Update the projection matrix
    if (window == nullptr) {
        // No window available, nothing to do.
        return;
    }

    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    if (fbh <= 0) // avoid division by 0
        fbh = 1;
    if (fbw <= 0)
        fbw = 1;

    width = fbw;
    height = fbh;

    float ratio = static_cast<float>(width) / static_cast<float>(height);

    // Validate FOV
    float useFov = fov;
    if (!(useFov > 0.0f && useFov < 179.0f)) {
        useFov = 45.0f;
    }

    projection_matrix = glm::perspective(
        glm::radians(useFov), // use current member FOV (degrees)
        ratio,                // Aspect Ratio
        0.1f,                 // Near clipping plane
        300.0f                // Far clipping plane
    );

    // Assume my_shader is valid; add a guard here if it can be uninitialized.
    my_shader.setUniform("uP_m", projection_matrix);
}

void App::updateFPS() { // Calculate the FPS of the application by counting the frames for 1 second
    frame_count++;

    TimePoint currentTime = Clock::now();

    // If last_time is default-constructed (uninitialized), set it and skip FPS counting for this frame.
    if (last_time.time_since_epoch().count() == 0) {
        last_time = currentTime;
        frame_count = 0;
        return;
    }

    float elapsed = std::chrono::duration<float>(currentTime - last_time).count();

    if (elapsed >= 1.0f) {
        // Use a more accurate measurement than simple counting for >=1s:
        if (elapsed > 0.0f) {
            fps = static_cast<int>(std::round(static_cast<float>(frame_count) / elapsed));
        } else {
            fps = frame_count;
        }
        frame_count = 0;
        last_time = currentTime;
    }
}

float App::getTerrainHeight(float x, float z, const std::vector<std::vector<float>>& heightmap) const {
    // Validate input
    if (heightmap.empty() || heightmap[0].empty()) {
        return 0.0f;
    }

    int hmWidth = static_cast<int>(heightmap.size());
    int hmHeight = static_cast<int>(heightmap[0].size());
    for (const auto& col : heightmap) {
        if (static_cast<int>(col.size()) != hmHeight) {
            // Inconsistent heightmap dimensions -> safely return 0
            return 0.0f;
        }
    }

    float terrainScale = Ground.scale.x;
    if (terrainScale == 0.0f) {
        return 0.0f;
    }

    // Map world (x,z) -> heightmap coordinates (fx,fz)
    // This code assumes heightmap[x][z] (first index = width).
    float fx = (x + static_cast<float>(hmWidth) * 0.5f) / terrainScale;
    float fz = (z + static_cast<float>(hmHeight) * 0.5f) / terrainScale;

    // Clamp to valid range before floor to avoid OOB and compute correct fractions.
    float fx_clamped = std::clamp(fx, 0.0f, static_cast<float>(hmWidth - 1));
    float fz_clamped = std::clamp(fz, 0.0f, static_cast<float>(hmHeight - 1));

    int ix = static_cast<int>(std::floor(fx_clamped));
    int iz = static_cast<int>(std::floor(fz_clamped));

    // If heightmap too small for bilinear (less than 2x2), return nearest sample
    if (hmWidth < 2 || hmHeight < 2) {
        int cx = std::clamp(ix, 0, hmWidth - 1);
        int cz = std::clamp(iz, 0, hmHeight - 1);
        return heightmap[cx][cz];
    }

    // Clamp indices so reading ix+1 / iz+1 is safe
    ix = std::max(0, std::min(ix, hmWidth - 2));
    iz = std::max(0, std::min(iz, hmHeight - 2));

    // Fractional part for interpolation (computed from clamped coords)
    float fracx = fx_clamped - static_cast<float>(ix);
    float fracz = fz_clamped - static_cast<float>(iz);

    // Heights of surrounding points
    float h00 = heightmap[ix][iz];
    float h10 = heightmap[ix + 1][iz];
    float h01 = heightmap[ix][iz + 1];
    float h11 = heightmap[ix + 1][iz + 1];

    // Bilinear interpolation
    float h0 = h00 * (1.0f - fracx) + h10 * fracx;
    float h1 = h01 * (1.0f - fracx) + h11 * fracx;
    return h0 * (1.0f - fracz) + h1 * fracz;
}