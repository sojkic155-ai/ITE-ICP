#pragma once

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <algorithm>

#include "assets.hpp"
#include "Mesh.hpp"
#include "ShaderProgram.hpp"
#include "stb_image.h"

class Heightmap {
public:
    // Public data used by the app (mesh list + transform + height data for queries).
    std::vector<Mesh> meshes;
    std::string name;

    glm::vec3 origin{ 0.0 };
    glm::vec3 orientation{ 0.0 };  // rotation around x/y/z in radians
    glm::vec3 scale{ 1.0 };

    glm::mat4 local_model_matrix = glm::identity<glm::mat4>();
    glm::mat3 normal_matrix = glm::identity<glm::mat3>();

    GLuint texture_id{ 0 };
    ShaderProgram shader;

    std::vector<vertex> vertices{};
    GLuint NUM_STRIPS = 0;
    GLuint NUM_VERTS_PER_STRIP = 0;

    int width = 1;
    int height = 1;
    std::vector<std::vector<float>> heightmap{};

    // Default setup: empty mesh list + identity transform.
    Heightmap()
        : origin(0.0f),
        orientation(0.0f),
        scale(1.0f),
        local_model_matrix(glm::identity<glm::mat4>()),
        normal_matrix(glm::identity<glm::mat3>()),
        texture_id(0),
        NUM_STRIPS(0),
        NUM_VERTS_PER_STRIP(0),
        width(1),
        height(1)
    {
    }

    // Build a terrain mesh from a grayscale heightmap image (also stores heights for collision queries).
    Heightmap(const std::filesystem::path& filename, ShaderProgram shader, GLuint const texture_id = 0) {

        // 1) Load heightmap image (stb_image).
        // Force a single channel (grayscale) to make height extraction deterministic.
        int nChannels = 0;
        unsigned char* data = stbi_load(filename.string().c_str(), &width, &height, &nChannels, 1);
        nChannels = 1; // we requested 1 channel explicitly
        if (!data) {
            std::cerr << "Failed to load heightmap named: " << filename << std::endl;
            return;
        }
        std::cout << "Loaded heightmap named: " << filename << " (" << width << "x" << height << ", " << nChannels << "ch)" << std::endl;

        // 2) Generate vertices (+ normals) and cache heights for later queries.
        // Reserve capacity to avoid reallocations for medium/large maps.
        vertices.clear();
        vertices.reserve(static_cast<size_t>(width) * static_cast<size_t>(height));
        std::vector<std::vector<float>> getheightmap(height, std::vector<float>(width));
        const float yScale = 64.0f / 255.0f;
        const float yShift = 16.0f;
        const float heightscale = 1.0f; // additional height multiplier if needed

        // Iterate rows (i) and columns (j). Using signed ints for stb image dims is fine.
        for (int i = 0; i < height; ++i)
        {
            for (int j = 0; j < width; ++j)
            {
                // Helper lambda to safely read the (clamped) texel height.
                auto getHeight = [&](int row, int col) -> float {
                    row = glm::clamp(row, 0, height - 1);
                    col = glm::clamp(col, 0, width - 1);
                    unsigned char* texel = data + (col + width * row) * nChannels;
                    // single-channel grayscale -> texel[0] is intensity
                    return static_cast<float>(texel[0]) * yScale - yShift;
                };

                float h = getHeight(i, j);
                getheightmap[i][j] = h;

                // Sample neighbors to compute normals via central differences.
                glm::vec3 pL = glm::vec3(-height / 2.0f + (i - 1), getHeight(i - 1, j) * heightscale, -width / 2.0f + j);
                glm::vec3 pR = glm::vec3(-height / 2.0f + (i + 1), getHeight(i + 1, j) * heightscale, -width / 2.0f + j);
                glm::vec3 pD = glm::vec3(-height / 2.0f + i, getHeight(i, j - 1) * heightscale, -width / 2.0f + (j - 1));
                glm::vec3 pU = glm::vec3(-height / 2.0f + i, getHeight(i, j + 1) * heightscale, -width / 2.0f + (j + 1));

                glm::vec3 dX = pR - pL;
                glm::vec3 dZ = pU - pD;
                glm::vec3 N = glm::normalize(glm::cross(dZ, dX));

                vertex v;
                v.position = glm::vec3(-height / 2.0f + i, h, -width / 2.0f + j);
                v.normal = N;
                v.texcoord = glm::vec2(static_cast<float>(j) / std::max(1, width - 1),
                                       static_cast<float>(i) / std::max(1, height - 1));
                vertices.push_back(v);
            }
        }

        heightmap = std::move(getheightmap);
        stbi_image_free(data);

        // 3) Build indices for triangle strips (one strip per row).
        std::vector<GLuint> indices;
        indices.reserve(static_cast<size_t>(height > 1 ? (height - 1) * width * 2 : 0));
        for (int i = 0; i < height - 1; ++i)
        {
            for (int j = 0; j < width; ++j)
            {
                // push two vertices per column to form the strip for the two rows (i and i+1)
                indices.push_back(static_cast<GLuint>(j + width * (i + 0)));
                indices.push_back(static_cast<GLuint>(j + width * (i + 1)));
            }
        }

        NUM_STRIPS = static_cast<GLuint>(std::max(0, height - 1));
        NUM_VERTS_PER_STRIP = static_cast<GLuint>(width * 2u);

        // Create mesh (triangle strips) and store it. Mesh constructor receives transform + texture.
        Mesh mesh(GL_TRIANGLE_STRIP, shader, vertices, indices, origin, orientation, texture_id, NUM_STRIPS, NUM_VERTS_PER_STRIP);
        meshes.push_back(std::move(mesh));

        // Store a local model matrix that represents the heightmap's base transform.
        // This makes draw() variant that uses local_model_matrix meaningful.
        {
            glm::mat4 t = glm::translate(glm::mat4(1.0f), origin);
            glm::mat4 rx = glm::rotate(glm::mat4(1.0f), orientation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 ry = glm::rotate(glm::mat4(1.0f), orientation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 rz = glm::rotate(glm::mat4(1.0f), orientation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
            local_model_matrix = s * rz * ry * rx * t;
        }
    }

    // Draw all meshes with base transform + optional per-draw offset/rotation/scale.
    void draw(glm::vec3 const& offset = glm::vec3(0.0f),
        glm::vec3 const& rotation = glm::vec3(0.0f),
        glm::vec3 const& scale_change = glm::vec3(1.0f)) {

        // Compute combined model matrix for this draw call.
        glm::mat4 t = glm::translate(glm::mat4(1.0f), origin);
        glm::mat4 rx = glm::rotate(glm::mat4(1.0f), orientation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 ry = glm::rotate(glm::mat4(1.0f), orientation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rz = glm::rotate(glm::mat4(1.0f), orientation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);

        glm::mat4 m_off = glm::translate(glm::mat4(1.0f), offset);
        glm::mat4 m_rx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 m_ry = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 m_rz = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 m_s = glm::scale(glm::mat4(1.0f), scale_change);

        glm::mat4 mv_m = glm::identity<glm::mat4>();
        glm::mat4 local_model = mv_m * s * rz * ry * rx * t;
        glm::mat4 model_matrix = local_model * m_s * m_rz * m_ry * m_rx * m_off;
        normal_matrix = glm::mat3(glm::inverseTranspose(model_matrix));

        for (auto& mesh : meshes) {
            mesh.draw(model_matrix);
        }
    }

    // Draw using the stored local_model_matrix (no per-draw overrides).
    void draw() {
        for (auto& mesh : meshes) {
            mesh.draw(local_model_matrix);
        }
    }
};
