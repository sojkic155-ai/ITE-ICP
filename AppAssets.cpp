#include "app.hpp"
#include "assets.hpp"
#include "ShaderProgram.hpp"
#include "Model.hpp"
#include "Heightmap.hpp"

#include <opencv2/opencv.hpp>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

void App::init_assets(void) {
    // Load everything needed for rendering (shader, textures, terrain, models) and register objects into `scene`.

    // Shader used by the whole scene.
    my_shader = ShaderProgram("lighting_shader.vert", "lighting_shader.frag");

    // Base textures (most objects pick one of these).
    my_texture = textureInit("resources/textures/tex_2048.png");
    GLuint lamp = textureInit("resources/textures/Lamp_BaseColor.png");
    GLuint glass = textureInit("resources/textures/glass.jpg");
    GLuint stone = textureInit("resources/textures/Rock.jpg");
    GLuint stone_2 = textureInit("resources/textures/Rock-Texture-Surface.jpg");
    GLuint stone_3 = textureInit("resources/textures/rock_2.jpg");
    GLuint Cactus = textureInit("resources/textures/cactustextur.png");

    // Take one tile from a texture atlas and upload it as a standalone GL texture.
    auto createSubTextureFromAtlas = [&](const std::filesystem::path& atlasPath, int tileX, int tileY, int tilesPerRow = 16) -> GLuint {
        cv::Mat atlas = cv::imread(atlasPath.string(), cv::IMREAD_UNCHANGED);
        if (atlas.empty()) {
            throw std::runtime_error("Cannot open atlas: " + atlasPath.string());
        }

        int tilePx = atlas.cols / tilesPerRow;
        int sx = tileX * tilePx;
        int sy = tileY * tilePx;

        // Keep the selected tile inside the atlas bounds.
        sx = std::max(0, std::min(sx, atlas.cols - tilePx));
        sy = std::max(0, std::min(sy, atlas.rows - tilePx));

        cv::Rect roi(sx, sy, tilePx, tilePx);
        cv::Mat tile = atlas(roi).clone();
        return gen_tex(tile);
        };

    GLuint ground_tex = createSubTextureFromAtlas("resources/textures/tex_2048.png", 14, 7);
    GLuint cactus_tex = createSubTextureFromAtlas("resources/textures/tex_2048.png", 14, 1);
    GLuint plane_tex = createSubTextureFromAtlas("resources/textures/tex_2048.png", 1, 0);

    // Terrain mesh + cached heightmap data for collision / placement.
    Ground = Heightmap("resources/heightmaps/ground_v1.png", my_shader, ground_tex);

    // Random placement config for environment objects.
    const int numPoints = 75;
    const int minCoordinate = -100;
    const int maxCoordinate = 100;
    const int minborder = -15;
    const int maxborder = 15;
    glm::vec2 cactuscoords = glm::vec2(0.0f);

    std::srand(static_cast<unsigned int>(std::time(0)));

    float positionx = 0.0f;
    float positionz = 0.0f;

    // Load templates (some are copied and then placed multiple times).
    Model my_model = Model("resources/objects/Wooden_Crate.obj", my_shader, my_texture);
    Model base = my_model;
    Model transparent_model = my_model;
    Model Cactuses = Model("resources/objects/cactus.obj", my_shader, cactus_tex);
    Model Lamp = Model("resources/objects/lamp.obj", my_shader, lamp);
    Model mini_lamp = Lamp;
    Model plane = Model("resources/objects/plane.obj", my_shader, plane_tex);
    projectile = Model("resources/objects/Rock_1.OBJ", my_shader, stone);
    Model rockTemplate = Model("resources/objects/rock_2.obj", my_shader, stone);
    Model rock3Template = Model("resources/objects/rock_3.obj", my_shader, stone_2);
    Model rock4Template = Model("resources/objects/rock_4.obj", my_shader, stone_3);

    // Place the main crate.
    positionx = -5.0f;
    positionz = 15.0f;
    float terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    my_model.origin = glm::vec3(positionx, terrainYm + 0.10f, positionz);
    my_model.scale = glm::vec3(0.5f);

    // Spawn cactuses randomly, but keep a clear area around the center.
    for (int i = 0; i < numPoints; ++i) {
        float x, z;
        do {
            x = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
            z = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
        } while (x > minborder && x < maxborder && z > minborder && z < maxborder);

        terrainYm = getTerrainHeight(x, z, Ground.heightmap);
        Cactuses.origin = glm::vec3(x, terrainYm, z);

        float s1 = 1.55f + static_cast<float>(std::rand()) / RAND_MAX * 1.65f;
        Cactuses.scale = glm::vec3(s1);
        Cactuses.orientation = glm::vec3(glm::radians(-90.0f), 0.0f, glm::radians(static_cast<float>(std::rand() % 360)));

        Cactuses.solid = true;
        Cactuses.computeAABB();
        scene.insert({ std::string("Cactus:").append(std::to_string(i)).c_str(), Cactuses });
    }

    {
        // Spawn rock_2 instances.
        const int numRocks = 25;
        for (int i = 0; i < numRocks; ++i) {
            float x, z;
            do {
                x = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
                z = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
            } while (x > minborder && x < maxborder && z > minborder && z < maxborder);

            float terrainYat = getTerrainHeight(x, z, Ground.heightmap);
            rockTemplate.origin = glm::vec3(x, terrainYat, z);

            float s = 0.002f + static_cast<float>(std::rand()) / RAND_MAX * 0.04f;
            rockTemplate.scale = glm::vec3(s);
            rockTemplate.orientation = glm::vec3(0.0f, glm::radians(static_cast<float>(std::rand() % 360)), 0.0f);

            rockTemplate.solid = true;
            rockTemplate.computeAABB();
            scene.insert({ std::string("Rock:").append(std::to_string(i)).c_str(), rockTemplate });
        }
    }

    {
        // Spawn rock_3 and rock_4 instances.
        const int numRock3 = 20;
        for (int i = 0; i < numRock3; ++i) {
            float x, z;
            do {
                x = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
                z = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
            } while (x > minborder && x < maxborder && z > minborder && z < maxborder);

            float terrainYat = getTerrainHeight(x, z, Ground.heightmap);
            rock3Template.origin = glm::vec3(x, terrainYat - 0.5f, z);

            float s3 = 0.01f + static_cast<float>(std::rand()) / RAND_MAX * 0.09f;
            rock3Template.scale = glm::vec3(s3);
            rock3Template.orientation = glm::vec3(
                glm::radians(static_cast<float>(std::rand() % 360)),
                glm::radians(static_cast<float>(std::rand() % 360)),
                glm::radians(static_cast<float>(std::rand() % 360))
            );

            rock3Template.solid = true;
            rock3Template.computeAABB();
            scene.insert({ std::string("Rock3:").append(std::to_string(i)).c_str(), rock3Template });
        }

        const int numRock4 = 20;
        for (int i = 0; i < numRock4; ++i) {
            float x, z;
            do {
                x = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
                z = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
            } while (x > minborder && x < maxborder && z > minborder && z < maxborder);

            float terrainYat = getTerrainHeight(x, z, Ground.heightmap);
            rock4Template.origin = glm::vec3(x, terrainYat + 1.0f, z);

            float s4 = 0.008f + static_cast<float>(std::rand()) / RAND_MAX * 0.03f;
            rock4Template.scale = glm::vec3(s4);
            rock4Template.orientation = glm::vec3(0.0f, glm::radians(static_cast<float>(std::rand() % 360)), 0.0f);

            rock4Template.solid = true;
            rock4Template.computeAABB();
            scene.insert({ std::string("Rock4:").append(std::to_string(i)).c_str(), rock4Template });
        }
    }

    // Place lamp.
    positionx = 2.0f;
    positionz = 10.0f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    Lamp.origin = glm::vec3(positionx, terrainYm, positionz);
    Lamp.scale = glm::vec3(3.0f);

    // Place crate base + transparent crate.
    base.origin = glm::vec3(-5.0f, getTerrainHeight(-5.0f, 5.0f, Ground.heightmap) + 0.5f, 10.0f);
    base.scale = glm::vec3(0.25f);

    positionx = -5.0f;
    positionz = 5.0f;
    float groundY = getTerrainHeight(positionx, positionz, Ground.heightmap);

    transparent_model.scale = glm::vec3(0.25f);
    transparent_model.origin.x = positionx;
    transparent_model.origin.z = positionz;
    transparent_model.transparent = true;

    // Compute the model's min/max local Y so we can place it exactly on the terrain.
    auto computeMinMaxY = [](Model const& m) -> std::pair<float, float> {
        if (m.vertices.empty()) return { 0.0f, 0.0f };
        float miny = std::numeric_limits<float>::infinity();
        float maxy = -std::numeric_limits<float>::infinity();
        for (auto const& v : m.vertices) {
            miny = std::min(miny, v.position.y);
            maxy = std::max(maxy, v.position.y);
        }
        return { miny, maxy };
        };

    auto [t_minY, t_maxY] = computeMinMaxY(transparent_model);
    if (t_minY == std::numeric_limits<float>::infinity()) {
        transparent_model.origin.y = groundY + 0.01f;
    }
    else {
        transparent_model.origin.y = groundY - t_minY * transparent_model.scale.y;
    }

    // Place mini_lamp on top of the transparent block (centered in XZ).
    mini_lamp.scale = glm::vec3(0.3f);

    glm::vec3 centroid_local(0.0f);
    if (!transparent_model.vertices.empty()) {
        for (auto const& v : transparent_model.vertices) {
            centroid_local += v.position;
        }
        centroid_local /= static_cast<float>(transparent_model.vertices.size());

        mini_lamp.origin.x = transparent_model.origin.x + centroid_local.x * transparent_model.scale.x;
        mini_lamp.origin.z = transparent_model.origin.z + centroid_local.z * transparent_model.scale.z;
        mini_lamp.origin.y = transparent_model.origin.y + 0.01f;
    }
    else {
        mini_lamp.origin = transparent_model.origin;
    }

    // Place the plane.
    positionx = 5.0f;
    positionz = 5.0f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    plane.origin = glm::vec3(positionx, terrainYm + 0.5f, positionz);
    plane.scale = glm::vec3(0.5f);
    plane.orientation.z = glm::radians(30.0f);

    // Init projectile placement.
    projectile.origin = glm::vec3(0.0f, 0.5f, 0.0f);
    projectile.scale = glm::vec3(0.01f);

    // Enable collisions for selected objects.
    transparent_model.solid = true; transparent_model.computeAABB();
    Lamp.solid = true;              Lamp.computeAABB();
    mini_lamp.solid = true;         mini_lamp.computeAABB();
    my_model.solid = true;          my_model.computeAABB();

    // Register everything into the scene map.
    scene.insert({ "my_first_object", my_model });
    scene.insert({ "trasparent_block", transparent_model });
    scene.insert({ "Lamp", Lamp });
    scene.insert({ "Moving_model", plane });
    scene.insert({ "wooden_base", base });
    scene.insert({ "minilamp", mini_lamp });

    // Drop CPU-side mesh data for this local copy (scene has its own stored copy anyway).
    my_model.meshes.clear();
}

GLuint App::textureInit(const std::filesystem::path& file_name) {
    // Load an image via OpenCV and upload it as an OpenGL texture.

    cv::Mat image = cv::imread(file_name.string(), cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        throw std::runtime_error("No texture in file: " + file_name.string());
    }

    return gen_tex(image);
}

GLuint App::gen_tex(cv::Mat& image) {
    // Create a 2D GL texture from an OpenCV Mat (expects 3 or 4 channels, BGR/BGRA).

    if (image.empty()) {
        throw std::runtime_error("Image empty?\n");
    }

    GLuint ID = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &ID);
    glObjectLabel(GL_TEXTURE, ID, -1, "Mytexture");

    switch (image.channels()) {
    case 3:
        glTextureStorage2D(ID, 1, GL_RGB8, image.cols, image.rows);
        glTextureSubImage2D(ID, 0, 0, 0, image.cols, image.rows, GL_BGR, GL_UNSIGNED_BYTE, image.data);
        break;
    case 4:
        glTextureStorage2D(ID, 1, GL_RGBA8, image.cols, image.rows);
        glTextureSubImage2D(ID, 0, 0, 0, image.cols, image.rows, GL_BGRA, GL_UNSIGNED_BYTE, image.data);
        break;
    default:
        throw std::runtime_error("unsupported channel cnt. in texture:" + std::to_string(image.channels()));
    }

    // Filtering + mipmaps (nice quality when zoomed out).
    glTextureParameteri(ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(ID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(ID);

    // Repeat texture outside [0..1] UV range.
    glTextureParameteri(ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(ID, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return ID;
}
