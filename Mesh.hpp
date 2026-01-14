#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "assets.hpp"
#include "ShaderProgram.hpp"

class Mesh {
public:
    // Mesh state: transform + geometry + rendering setup.
    glm::vec3 origin{};
    glm::vec3 orientation{};
    glm::mat4 model_matrix{};

    GLuint texture_id{ 0 };           // 0 => no texture bound
    GLenum primitive_type = GL_POINT;

    ShaderProgram shader;
    std::vector<vertex> vertices{};
    std::vector<GLuint> indices{};

    // Extra params used by the heightmap renderer (triangle strips).
    GLuint NUM_STRIPS = 0;
    GLuint NUM_VERTS_PER_STRIP = 0;

    // Simple material parameters (used by lighting shader).
    glm::vec4 ambient_material{ 1.0f };
    glm::vec4 diffuse_material{ 1.0f };
    glm::vec4 specular_material{ 1.0f };
    float reflectivity{ 1.0f };

    // Build an indexed mesh and upload its data to GPU (VAO/VBO/EBO).
    Mesh(GLenum primitive_type,
        ShaderProgram shader,
        std::vector<vertex> const& vertices,
        std::vector<GLuint> const& indices,
        glm::vec3 const& origin,
        glm::vec3 const& orientation,
        GLuint const texture_id = 0,
        GLuint NUM_STRIPS = 0,
        GLuint NUM_VERTS_PER_STRIP = 0)
        : primitive_type(primitive_type),
        shader(shader),
        vertices(vertices),
        indices(indices),
        origin(origin),
        orientation(orientation),
        texture_id(texture_id),
        NUM_STRIPS(NUM_STRIPS),
        NUM_VERTS_PER_STRIP(NUM_VERTS_PER_STRIP)
    {
        glCreateVertexArrays(1, &VAO);
        glObjectLabel(GL_VERTEX_ARRAY, VAO, -1, "MyMeshVAO");

        // Tell OpenGL how vertex data is laid out (pos/normal/texcoord).
        GLint position_attrib_location = glGetAttribLocation(shader.getID(), "aPos");
        glVertexArrayAttribFormat(VAO, position_attrib_location, 3, GL_FLOAT, GL_FALSE, offsetof(vertex, position));
        glVertexArrayAttribBinding(VAO, position_attrib_location, 0);
        glEnableVertexArrayAttrib(VAO, position_attrib_location);

        GLint normal_attrib_location = glGetAttribLocation(shader.getID(), "aNorm");
        glVertexArrayAttribFormat(VAO, normal_attrib_location, 3, GL_FLOAT, GL_FALSE, offsetof(vertex, normal));
        glVertexArrayAttribBinding(VAO, normal_attrib_location, 0);
        glEnableVertexArrayAttrib(VAO, normal_attrib_location);

        GLint texture_attrib_location = glGetAttribLocation(shader.getID(), "aTex");
        glVertexArrayAttribFormat(VAO, texture_attrib_location, 2, GL_FLOAT, GL_FALSE, offsetof(vertex, texcoord));
        glVertexArrayAttribBinding(VAO, texture_attrib_location, 0);
        glEnableVertexArrayAttrib(VAO, texture_attrib_location);

        // Upload vertex/index buffers.
        glCreateBuffers(1, &VBO);
        glObjectLabel(GL_BUFFER, VBO, -1, "MyMeshVBO");
        glCreateBuffers(1, &EBO);
        glObjectLabel(GL_BUFFER, EBO, -1, "MyMeshEBO");

        glNamedBufferData(VBO, vertices.size() * sizeof(vertex), vertices.data(), GL_STATIC_DRAW);
        glNamedBufferData(EBO, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

        glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(vertex));
        glVertexArrayElementBuffer(VAO, EBO);
    };

    // Render the mesh with the given model matrix and optional single texture.
    void draw(glm::mat4 const& model_matrix) {
        if (VAO == 0) {
            std::cerr << "VAO not initialized!\n";
            return;
        }

        shader.activate();
        glObjectLabel(GL_PROGRAM, shader.getID(), -1, "MyMeshShader");
        shader.setUniform("uM_m", model_matrix);

        if (texture_id > 0) {
            int i = 0;
            glBindTextureUnit(i, texture_id);
            shader.setUniform("tex0", i);
        }

        glBindVertexArray(VAO);

        if (primitive_type == GL_TRIANGLE_STRIP) {
            GLsizei vertsPerStrip = static_cast<GLsizei>(NUM_VERTS_PER_STRIP);
            for (GLuint strip = 0; strip < NUM_STRIPS; ++strip)
            {
                uintptr_t offset =
                    static_cast<uintptr_t>(sizeof(unsigned int)) *
                    static_cast<uintptr_t>(NUM_VERTS_PER_STRIP) *
                    static_cast<uintptr_t>(strip);

                glDrawElements(GL_TRIANGLE_STRIP, vertsPerStrip, GL_UNSIGNED_INT, reinterpret_cast<void*>(offset));
            }
        }
        else {
            glDrawElements(primitive_type, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        }
    }

    // Free GPU objects, reset render state, and clear CPU-side geometry.
    void clear(void) {
        if (texture_id) {
            glDeleteTextures(1, &texture_id);
            texture_id = 0;
        }

        primitive_type = GL_POINT;
        ambient_material = glm::vec4(1.0f);
        diffuse_material = glm::vec4(1.0f);
        specular_material = glm::vec4(1.0f);
        reflectivity = 1.0f;

        vertices.clear();
        indices.clear();
        origin = glm::vec3(0.0f);
        orientation = glm::vec3(0.0f);

        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &EBO);
    };

private:
    // OpenGL object IDs (0 means "not created").
    unsigned int VAO{ 0 }, VBO{ 0 }, EBO{ 0 };
};
