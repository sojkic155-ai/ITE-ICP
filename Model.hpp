#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "assets.hpp"
#include "Mesh.hpp"
#include "ShaderProgram.hpp"
#include "OBJloader.hpp"

class Model {
public:
    // Model = a container of one or more meshes + a shared transform.
    std::vector<Mesh> meshes;
    std::string name;

    glm::vec3 origin{ 0.0 };
    glm::vec3 orientation{ 0.0 };  // rotation around x/y/z in radians
    glm::vec3 scale{ 1.0 };

    glm::mat4 model_matrix{};
    glm::mat4 local_model_matrix{}; // base transform of the model
    glm::mat3 normal_matrix{};      // derived from model_matrix for lighting

    GLuint texture_id{ 0 };
    ShaderProgram shader;
    std::vector<vertex> vertices{};

    // Simple physics helpers (used by flyghtpath()).
    glm::vec3 velocity;
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);

    // Set true if the model needs transparent rendering (alpha < 1).
    bool transparent{ false };

    // Default setup: identity transform and zero velocity.
    Model()
        : origin(0.0f),
        orientation(0.0f),
        scale(1.0f),
        model_matrix{},
        local_model_matrix(glm::identity<glm::mat4>()),
        normal_matrix(glm::identity<glm::mat3>()),
        texture_id(0),
        velocity(0.0f),
        gravity(0.0f, -9.81f, 0.0f)
    {
    }

    // Load an OBJ, build vertices/indices, and create a single mesh for rendering.
    Model(const std::filesystem::path& filename, ShaderProgram shader, GLuint const texture_id = 0) {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> texcoords;
        std::vector<glm::vec3> normals;

        if (!loadOBJ(filename.string().c_str(), positions, texcoords, normals)) {
            std::cerr << "Failed to load OBJ file: " << filename << std::endl;
            return;
        }

        // Flip UVs to match the texture coordinate convention used in this project.
        for (auto& tex : texcoords) {
            tex.x = 1.0f - tex.x;
            tex.y = 1.0f - tex.y;
        }

        for (size_t i = 0; i < positions.size(); ++i) {
            vertex v;
            v.position = positions[i];
            v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0.0f);
            v.texcoord = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0.0f);
            vertices.push_back(v);
        }

        // Simple 1:1 indexing (no vertex dedup).
        std::vector<GLuint> indices(vertices.size());
        for (GLuint i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }

        Mesh Mesh(GL_TRIANGLES, shader, vertices, indices, origin, orientation, texture_id);
        meshes.push_back(std::move(Mesh));
    }

    // Move the model on a horizontal circle and rotate it to face the travel direction.
    float angle = 0.0f;
    void circlepath(float delta_t, float height, float radius = 70.0f, float angular_speed = 0.2f) {
        angle += angular_speed * delta_t;

        if (angle > glm::two_pi<float>())
            angle -= glm::two_pi<float>();

        origin.x = radius * cos(angle);
        origin.z = radius * sin(angle);
        origin.y = height + 20.0f;

        float yaw = angle + glm::half_pi<float>();
        orientation.y = -yaw + 0.95f;
    }

    // Apply gravity + input to update the model position (rough "flight" movement).
    void flyghtpath(float delta_t, glm::vec3 input) {
        velocity += gravity * delta_t;
        origin.x += input.x + velocity.x * delta_t;
        origin.y += velocity.y * delta_t;
        origin.z += velocity.z * delta_t;
    }

    // Draw model with base transform + optional per-draw offset/rotation/scale.
    void draw(glm::vec3 const& offset = glm::vec3(0.0f),
        glm::vec3 const& rotation = glm::vec3(0.0f),
        glm::vec3 const& scale_change = glm::vec3(1.0f)) {

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

        local_model_matrix = mv_m * t * rx * ry * rz * s;
        model_matrix = local_model_matrix * m_s * m_rz * m_ry * m_rx * m_off;
        normal_matrix = glm::mat3(glm::inverseTranspose(model_matrix));

        for (auto& mesh : meshes) {
            mesh.draw(model_matrix);
        }
    }

    // Draw model with an external matrix (useful for parent-child transforms).
    void draw(glm::mat4 const& model_matrix) {
        for (auto& mesh : meshes) {
            mesh.draw(local_model_matrix * model_matrix);
        }
    }

public:
    // Basic collision helpers using a local AABB scaled+translated into world space.
    bool solid{ false };
    glm::vec3 aabb_min_local{ 0.0f };
    glm::vec3 aabb_max_local{ 0.0f };

    // Compute local-space AABB from current vertex list.
    void computeAABB() {
        if (vertices.empty()) {
            aabb_min_local = glm::vec3(0.0f);
            aabb_max_local = glm::vec3(0.0f);
            return;
        }

        glm::vec3 mn = vertices[0].position;
        glm::vec3 mx = vertices[0].position;

        for (auto const& v : vertices) {
            mn = glm::min(mn, v.position);
            mx = glm::max(mx, v.position);
        }

        aabb_min_local = mn;
        aabb_max_local = mx;
    }

    // Get world-space AABB from origin + scaled local bounds.
    std::pair<glm::vec3, glm::vec3> getWorldAABB() const {
        glm::vec3 worldMin = origin + aabb_min_local * scale;
        glm::vec3 worldMax = origin + aabb_max_local * scale;

        glm::vec3 mn = glm::min(worldMin, worldMax);
        glm::vec3 mx = glm::max(worldMin, worldMax);
        return { mn, mx };
    }

    // AABB vs sphere intersection test (cheap collision check).
    bool intersectsSphere(glm::vec3 center, float radius) const {
        auto [mn, mx] = getWorldAABB();

        glm::vec3 closest;
        closest.x = std::max(mn.x, std::min(center.x, mx.x));
        closest.y = std::max(mn.y, std::min(center.y, mx.y));
        closest.z = std::max(mn.z, std::min(center.z, mx.z));

        float dist2 = glm::dot(closest - center, closest - center);
        return dist2 <= radius * radius;
    }
};
