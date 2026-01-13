#include <iostream>
#include <opencv2/opencv.hpp>// OpenGL Extension Wrangler: allow all multiplatform GL functions
#include <GL/glew.h> // WGLEW = Windows GL Extension Wrangler (change for different platform) platform specific functions (in this case Windows)
#include <GL/wglew.h> // GLFW toolkit. Uses GL calls to open GL context, i.e. GLEW must be first.
#include <GLFW/glfw3.h> // OpenGL math
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <unordered_map>
#include <chrono>
#include <irrKlang/irrKlang.h>
#include <cstdlib>  // For rand() and srand()
#include <ctime>    // For time()
#include <atomic>

#include "assets.hpp"
#include "app.hpp"
#include "gl_err_callback.h"    //Included for error checking of glew, wglew and glfw3
#include "ShaderProgram.hpp"    // compiles shaders, defines setUniform functions
#include "Mesh.hpp"             // Create and initialize VAO, VBO, EBO and parameters. 
#include "Model.hpp"            //creates model from on .obj file using given shaders and calls draw mesh function. The update of the model matrix (translation, rotation and schaling of the loaded model in view space) also happens here.
#include "camera.hpp"           // handles the movement of the camera (by updating he view matrix)
#include "Heightmap.hpp"
#include "FaceTracker.hpp"

//---------------------------------------------------------------------

static std::atomic<int> g_projectile_counter{ 0 };

App::App()
{
    //------ default constructor ------
    std::cout << "Constructed...\n";
}


bool App::init()
{
    //------ Set Error Callback ------ 
    glfwSetErrorCallback(error_callback);

    //------ Initialize the library ------
    if (!glfwInit())
        return -1;

    //------ Set the application to use core profile version 4.6 ------
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    //------ Create a windowed mode window and its OpenGL context ------
    window = glfwCreateWindow(640, 480, "Prototype app", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, key_callback);

    //------ Make the window's context current ------
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE; // required for core profiles
    GLenum glew_ret = glewInit();
    std::cout << "glewInit() = " << glew_ret << " (" << (glew_ret == GLEW_OK ? "OK" : "ERROR") << ")\n";
    std::cout << "GLEW_VERSION: " << (const char*)glewGetString(GLEW_VERSION) << "\n";
    std::cout << "GL_VERSION: " << (const char*)glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL_VERSION: " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
    std::cout << "GLEW_ARB_direct_state_access: " << (GLEW_ARB_direct_state_access ? "yes" : "no") << "\n";
    std::cout << "glCreateTextures ptr: " << reinterpret_cast<void*>(glCreateTextures) << "\n";
    std::cout << "glTextureStorage2D ptr: " << reinterpret_cast<void*>(glTextureStorage2D) << "\n";

    glew_ret = glewInit(); // Platform specific init
    if (glew_ret != GLEW_OK) {
        throw std::runtime_error(std::string("WGLEW failed with error: ")
            + reinterpret_cast<const char*>(glewGetErrorString(glew_ret)));
    }
    else {
        std::cout << "WGLEW successfully initialized platform specific functions." << std::endl;
    }

    //------ Check if we are in core or compatibility profile ------
    GLint myint;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &myint);

    if (myint & GL_CONTEXT_CORE_PROFILE_BIT) {
        std::cout << "We are using CORE profile\n";
    }
    else {
        if (myint & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {
            std::cout << "We are using COMPATIBILITY profile\n";
        }
        else {
            throw std::runtime_error("What??");
        }
    }

    //------ Enable debug output ------
    if (GLEW_ARB_debug_output) {
        glDebugMessageCallback(MessageCallback, 0);
        //glEnable(GL_DEBUG_OUTPUT);    //asynchronous debug output (default)
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        std::cout << "GL_DEBUG enabled." << std::endl;
    }
    else {
        std::cout << "GL_DEBUG NOT SUPPORTED!" << std::endl;
    }

    //------ Check if DSA have been initialised or not ------
    if (!GLEW_ARB_direct_state_access)




        throw std::runtime_error("No DSA :-(");

    //------ Get some glfw info ------
    int major, minor, revision;
    glfwGetVersion(&major, &minor, &revision);
    std::cout << "Running GLFW DLL " << major << '.' << minor << '.' << revision << std::endl;
    std::cout << "Compiled against GLFW " << 
        GLFW_VERSION_MAJOR << '.' << GLFW_VERSION_MINOR << '.' << GLFW_VERSION_REVISION << std::endl;

    init_assets();  // Initialise: shaders, textures, models

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    engine = irrklang::createIrrKlangDevice();
    if (!engine)
        throw std::exception("Can not create 3D sound device");
    BackgroundEngine = engine;
    
    if (!tracker.init(0)) return -1; // camera index 0

    return true;
}

void App::init_assets(void) {
    // Initialize pipeline: compile, link and use shaders
    // 
    // -----shaders------: load, compile, link, initialize params (may be moved global variables - if all models used same shader)
    my_shader = ShaderProgram("lighting_shader.vert", "lighting_shader.frag");

    //------textures-----
    my_texture = textureInit("resources/textures/tex_2048.png");
    GLuint lamp = textureInit("resources/textures/Lamp_BaseColor.png");
    GLuint glass = textureInit("resources/textures/glass.jpg");
    GLuint stone = textureInit("resources/textures/Rock.jpg");
    GLuint stone_2 = textureInit("resources/textures/Rock-Texture-Surface.jpg");
    GLuint stone_3 = textureInit("resources/textures/rock_2.jpg");
    GLuint Cactus = textureInit("resources/textures/cactustextur.png");

    auto createSubTextureFromAtlas = [&](const std::filesystem::path& atlasPath, int tileX, int tileY, int tilesPerRow = 16) -> GLuint {
        cv::Mat atlas = cv::imread(atlasPath.string(), cv::IMREAD_UNCHANGED);
        if (atlas.empty()) {
            throw std::runtime_error("Cannot open atlas: " + atlasPath.string());
        }
        int tilePx = atlas.cols / tilesPerRow; // 2048/16 = 128
        int sx = tileX * tilePx;
        int sy = tileY * tilePx;
        // clamp rect into image
        sx = std::max(0, std::min(sx, atlas.cols - tilePx));
        sy = std::max(0, std::min(sy, atlas.rows - tilePx));
        cv::Rect roi(sx, sy, tilePx, tilePx);
        cv::Mat tile = atlas(roi).clone();
        // optionally upscale tile to full resolution (nepotřebné, gen_tex použije rozměry)
        return gen_tex(tile);
        };


    GLuint ground_tex = createSubTextureFromAtlas("resources/textures/tex_2048.png", 14, 7);
    GLuint cactus_tex = createSubTextureFromAtlas("resources/textures/tex_2048.png", 14, 1);
    GLuint plane_tex = createSubTextureFromAtlas("resources/textures/tex_2048.png", 1, 0);


    // ------Heightmap------
    //Ground = Heightmap("resources/heightmaps/iceland_heightmap.png", my_shader, my_texture);
    Ground = Heightmap("resources/heightmaps/ground_v1.png", my_shader, ground_tex);

    // ------ Models ------: load model file, assign shader used to draw a model

    // --- Random coordinate generation for the Cactuses ---
    const int numPoints = 75;
    const int minCoordinate = -100;
    const int maxCoordinate = 100;
    const int minborder = -15;
    const int maxborder = 15;
    glm::vec2 cactuscoords = glm::vec2(0.0f);
    std::srand(static_cast<unsigned int>(std::time(0)));
    // --- ---

    float positionx = 0.0f;
    float positionz = 0.0f;
    
    Model my_model = Model("resources/objects/Wooden_Crate.obj", my_shader, my_texture);
    Model base = my_model;
    Model transparent_model = my_model;
    Model Cactuses = Model("resources/objects/cactus.obj", my_shader , cactus_tex);
    Model Lamp = Model("resources/objects/lamp.obj", my_shader , lamp);
    Model mini_lamp =Lamp;
    Model plane = Model("resources/objects/plane.obj", my_shader , plane_tex);
    projectile = Model("resources/objects/Rock_1.OBJ", my_shader, stone);
    Model rockTemplate = Model("resources/objects/rock_2.obj", my_shader, stone);
    Model rock3Template = Model("resources/objects/rock_3.obj", my_shader, stone_2);
    Model rock4Template = Model("resources/objects/rock_4.obj", my_shader, stone_3);

    positionx = -5.0f;
    positionz = 15.0f;
    float terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    my_model.origin = glm::vec3(positionx, terrainYm + 0.10f, positionz);
    my_model.scale = glm::vec3(0.5f);
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
        // rock_2
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
        // rock_3
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

        // rock_4
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

    positionx = 2.0f;
    positionz = 10.0f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    Lamp.origin = glm::vec3(positionx, terrainYm, positionz);
    Lamp.scale = glm::vec3(3.0f);

    positionx = 0.0f;
    positionz = 3.0f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    base.origin = glm::vec3(-5.0f, getTerrainHeight(-5.0f, 5.0f, Ground.heightmap) + 0.5f, 10.0f);
    base.scale = glm::vec3(0.25f);
    positionx = -5.0f;
    positionz = 5.0f;
    float groundY = getTerrainHeight(positionx, positionz, Ground.heightmap);
    transparent_model.scale = glm::vec3(0.25f);
    transparent_model.origin.x = positionx;
    transparent_model.origin.z = positionz;
    transparent_model.transparent = true;
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
        // fallback: pokud model nemá vertexy, použijeme malý offset
        transparent_model.origin.y = groundY + 0.01f;
    }
    else {
        // nastavit origin.y tak, aby nejnižší vertex ležel přesně na terénu:
        // world_y = origin.y + vertex_y * scale.y  => chceme world_y(minY) == groundY
        transparent_model.origin.y = groundY - t_minY * transparent_model.scale.y;
    }

    // nyní nastavíme mini_lamp tak, aby stál na vrchu transparent_model
    // mini_lamp je založen na Lamp (kopie), upravíme měřítko a umístíme ho přesně do středu transparent_modelu (v XZ)
    mini_lamp.scale = glm::vec3(0.3f); // upravte dle potřeby

    // spočítat centroid modelu (lokální souřadnice) – lépe než pouhé použití origin.x/z
    glm::vec3 centroid_local(0.0f);
    if (!transparent_model.vertices.empty()) {
        for (auto const& v : transparent_model.vertices) {
            centroid_local += v.position;
        }
        centroid_local /= static_cast<float>(transparent_model.vertices.size());
        // převod centroidu do world-space: origin + centroid * scale (X/Z)
        mini_lamp.origin.x = transparent_model.origin.x + centroid_local.x * transparent_model.scale.x;
        mini_lamp.origin.z = transparent_model.origin.z + centroid_local.z * transparent_model.scale.z;
        // Y: zarovnat na stejnou výšku jako transparent_model.origin.y + drobný offset
        mini_lamp.origin.y = transparent_model.origin.y + 0.01f;
    }
    else {
        // fallback: pokud transparent_model nemá vertexy, umístíme mini_lamp na origin
        mini_lamp.origin = transparent_model.origin;
    }

    positionx = 0.0f;
    positionz = -3.5f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    positionx = 5.0f;
    positionz = 5.0f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    plane.origin = glm::vec3(positionx, terrainYm+0.5f, positionz);
    plane.scale = glm::vec3(0.5f);
    plane.orientation.z = glm::radians(30.0f);

    positionx = 0.0f;
    positionz = 0.0f;
    projectile.origin = glm::vec3(positionx, 0.5f, positionz);
    projectile.scale = glm::vec3(0.01f);


    transparent_model.solid = true;
    transparent_model.computeAABB();
    Lamp.solid = true;
    Lamp.computeAABB();
    mini_lamp.solid = true;
    mini_lamp.computeAABB();
    my_model.solid = true;
    my_model.computeAABB();

    // put model to scene
    scene.insert({ "my_first_object", my_model });
    scene.insert({ "trasparent_block", transparent_model });
    scene.insert({ "Lamp", Lamp });
    scene.insert({ "Moving_model", plane });
    scene.insert({ "wooden_base", base });
    scene.insert({ "minilamp", mini_lamp });

    

    my_model.meshes.clear();
}

GLuint App::textureInit(const std::filesystem::path& file_name){
    // Initialise texture based on given image

    cv::Mat image = cv::imread(file_name.string(), cv::IMREAD_UNCHANGED);  // Read with (potential) Alpha
    if (image.empty()) {
        throw std::runtime_error("No texture in file: " + file_name.string());
    }
    
    // or print warning, and generate synthetic image with checkerboard pattern 
    // using OpenCV and use as a texture replacement 

    GLuint texture = gen_tex(image);

    return texture;
}

GLuint App::gen_tex(cv::Mat& image){
    // Generate texture from loaded image file

    GLuint ID = 0;

    if (image.empty()) {
        throw std::runtime_error("Image empty?\n");
    }

    // Generates an OpenGL texture object
    glCreateTextures(GL_TEXTURE_2D, 1, &ID);
    glObjectLabel(GL_TEXTURE, ID, -1, "Mytexture");

    switch (image.channels()) {
    case 3:
        // Create and clear space for data - immutable format
        glTextureStorage2D(ID, 1, GL_RGB8, image.cols, image.rows);
        // Assigns the image to the OpenGL Texture object
        glTextureSubImage2D(ID, 0, 0, 0, image.cols, image.rows, GL_BGR, GL_UNSIGNED_BYTE, image.data);
        break;
    case 4:
        glTextureStorage2D(ID, 1, GL_RGBA8, image.cols, image.rows);
        glTextureSubImage2D(ID, 0, 0, 0, image.cols, image.rows, GL_BGRA, GL_UNSIGNED_BYTE, image.data);
        break;
    default:
        throw std::runtime_error("unsupported channel cnt. in texture:" + std::to_string(image.channels()));
    }

    // MIPMAP filtering + automatic MIPMAP generation - nicest, needs more memory. Notice: MIPMAP is only for image minifying.
    glTextureParameteri(ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // bilinear magnifying
    glTextureParameteri(ID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // trilinear minifying
    glGenerateTextureMipmap(ID);  //Generate mipmaps now.

    // Configures the way the texture repeats
    glTextureParameteri(ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(ID, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return ID;
}

void App::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {   //Handles the callback events for all the key inputs
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window)); // Static cast needed to link key callbacks for the current active window (Filters out possible errors)

    if ((action == GLFW_PRESS) || (action == GLFW_REPEAT))
    {
        switch (key)
        {
        case GLFW_KEY_ESCAPE:   // Stop the application
            std::cout << "ESC has been pressed!\n";
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_V:    // Toggle VSync on-off by glfwSwapInterval(...)
            app->vsync_on = !app->vsync_on;  // Toggle the flag
            glfwSwapInterval(app->vsync_on ? 1 : 0);  // Apply new VSync setting
            break;
        case GLFW_KEY_TAB:  // Toggle between full screen and vindowed mode
            if (app->fullscreen == FALSE) {
                app->switch_to_fullscreen();
                //app->update_projection_matrix();
                app->fullscreen = TRUE;
            }
            else {
                glfwSetWindowMonitor(window, app->last_window_monitor, app->last_window_xpos,
                    app->last_window_ypos, app->last_window_width, app->last_window_height, GLFW_DONT_CARE);
               // app->update_projection_matrix();
                app->fullscreen = FALSE;
            }
            break; 
        case GLFW_KEY_C:    //Toggle the status of the cursor between locked and free
            if (app->cursor_state == FALSE) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                app->cursor_state = TRUE;
            }
            else{
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                app->cursor_state = FALSE;
            }
            break;
        case GLFW_KEY_M:    // Mute/unmute the audio
            if (app->mute == FALSE) {
                app->mute = TRUE;
            }
            else {
                app->mute = FALSE;
            }
            break;
        case GLFW_KEY_F:    // toggle flashlight
            if (app->flashlight == FALSE) {
                app->flashlight = TRUE;
                app->brightness = 10.0f;
                app->my_shader.setUniform("lights[1].ambientM", glm::vec3(0.05f, 0.05f, 0.05f));
                app->my_shader.setUniform("lights[1].diffuseM", glm::vec3(1.0f * app->brightness, 0.95f * app->brightness, 0.8f * app->brightness));
                app->my_shader.setUniform("lights[1].specularM", glm::vec3(1.0f * app->brightness, 0.95f * app->brightness, 0.9f * app->brightness));
            }
            else {
                app->flashlight = FALSE;
                app->my_shader.setUniform("lights[1].ambientM", glm::vec3(0.0f, 0.0f, 0.0f));
                app->my_shader.setUniform("lights[1].diffuseM", glm::vec3(0.0f, 0.0f, 0.0f));
                app->my_shader.setUniform("lights[1].specularM", glm::vec3(0.0f, 0.0f, 0.0f));
            }
            break;
        case GLFW_KEY_T:
            app->face_control_enabled = !app->face_control_enabled;
            std::cout << "[FaceControl] " << (app->face_control_enabled ? "ENABLED" : "DISABLED")
                << " (target_px=" << app->face_control_target_px << ")\n";
            break;
        case GLFW_KEY_N:    // Change day/night
            if (app->night == FALSE) {//set to night
                app->night = TRUE;
                app->brightness = 0.1f;
                app->my_shader.setUniform("fog_color", glm::vec4(glm::vec3(0.0f), 1.0f));
                app->my_shader.setUniform("lights[0].ambientM", glm::vec3(0.05f, 0.05f, 0.1f));
                app->my_shader.setUniform("lights[0].diffuseM", glm::vec3(0.2f * app->brightness, 0.2f * app->brightness, 0.35f * app->brightness));
                app->my_shader.setUniform("lights[0].specularM", glm::vec3(0.3f * app->brightness, 0.3f * app->brightness, 0.5f * app->brightness));
            }
            else {//set to day
                app->night = FALSE;
                app->my_shader.setUniform("fog_color", glm::vec4(glm::vec3(0.85f), 1.0f));
                app->my_shader.setUniform("lights[0].ambientM", glm::vec3(0.2f, 0.2f, 0.2f));
                app->my_shader.setUniform("lights[0].diffuseM", glm::vec3(1.0f, 0.95f, 0.8f));
                app->my_shader.setUniform("lights[0].specularM", glm::vec3(1.0f, 0.95f, 0.9f));
            }
            break;
        case GLFW_KEY_R:    // Reset player / camera to default safe position
        {
            glm::vec3 defaultPos = glm::vec3(0.0f, 15.0f, 0.0f);
            app->camera.Position = defaultPos;
            app->camera.Velocity = glm::vec3(0.0f);
            app->camera.onground = false;
            app->cursorLastX = 0.0;
            app->cursorLastY = 0.0;
            glfwSetCursorPos(window, app->cursorLastX, app->cursorLastY);
            std::cout << "Camera reset to default position\n";
        }
        break;

        default:
            break;
        }
    }
}

void App::update_projection_matrix() {  //Update the projection matrix
    glfwGetFramebufferSize(window, &width, &height);
    if (height <= 0) // avoid division by 0
        height = 1;

    float ratio = static_cast<float>(width) / height;

    projection_matrix = glm::perspective(
        glm::radians(45.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90deg (extra wide) and 30deg (quite zoomed in)
        ratio,			     // Aspect Ratio. Depends on the size of your window.
        0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
        300.0f               // Far clipping plane. Keep as little as possible.
    );
    my_shader.setUniform("uP_m", projection_matrix);
}

void App::updateFPS() { // Calculate the FPS of the application by counting the frames for 1 second
    frame_count++;

    TimePoint currentTime = Clock::now();
    float elapsed = std::chrono::duration<float>(currentTime - last_time).count();

    if (elapsed >= 1.0f) {
        fps = frame_count;
        //std::cout << "FPS: " << fps << std::endl;
        frame_count = 0;
        last_time = currentTime;
    }
}

void App::error_callback(int error, const char* description) {  //General error callback function
    std::cerr << "Error: " << description << std::endl;
}

void App::framebuffer_size_callback(GLFWwindow* window, int width, int height) {    // Update the projection matrix based on window size. Needed for window scaling
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));
    glViewport(0, 0, width, height);
    if (height <= 0) // avoid division by 0
        height = 1;

    float ratio = static_cast<float>(width) / height;

    app->projection_matrix = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 20000.0f);
    //app->my_shader.setUniform("uP_m", app->projection_matrix);
}

void App::switch_to_fullscreen(void) {
    // First, save position, size and placement for position recovery
    last_window_monitor = glfwGetWindowMonitor(window);
    glfwGetWindowSize(window, &last_window_width, &last_window_height);
    glfwGetWindowPos(window, &last_window_xpos, &last_window_xpos);
    // Switch to fullscreen
    // Multimonitor support
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    // Get resolution of primary monitor
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    // Switch to full screen
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
}

float App::getTerrainHeight(float x, float z, const std::vector<std::vector<float>>& heightmap) {
    int terrainWidth = Ground.width;
    int terrainHeight = Ground.height;
    float terrainScale = Ground.scale.x;  // Only need to be changed if the scale of the heightmap is changed
    float fx = (x + terrainHeight / 2.0f) / terrainScale;
    float fz = (z + terrainWidth / 2.0f) / terrainScale;

    int ix = (int)fx;
    int iz = (int)fz;

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
    float h0 = h00 * (1 - fracx) + h10 * fracx;
    float h1 = h01 * (1 - fracx) + h11 * fracx;
    return h0 * (1 - fracz) + h1 * fracz;
}


void App::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

    app->camera.ProcessMouseMovement(xpos - app->cursorLastX, (ypos - app->cursorLastY) * -1.0);
    app->cursorLastX = xpos;
    app->cursorLastY = ypos;

}

void App::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) { //General functionality to check if the callback is working or not
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        std::cout << "[DEBUG] Mouse left pressed." << std::endl;

        // safe forward vector (avoid normalizing zero vector)
        glm::vec3 forward = app->camera.Front;
        float fwdLen = glm::length(forward);
        if (fwdLen < 1e-6f) {
            // fallback direction pokud front není inicializovaný
            forward = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        else {
            forward = forward / fwdLen;
        }

        // připravit novou instanci projektilu (kopie šablony)
        Model newProj = app->projectile;
        // umístit projektil trochu před kameru, aby se hned nesrazil se sebou
        newProj.origin = app->camera.Position + forward * 1.0f;
        newProj.velocity = forward * 10.0f;
        newProj.solid = false; // než dopadne, není pevný
        // unikátní klíč, aby se staré kameny nevymazaly
        int id = ++g_projectile_counter;
        std::string key = std::string("throwable_rock_") + std::to_string(id);
        app->scene.insert_or_assign(key, newProj);
        std::cout << "[DEBUG] Inserted " << key << " origin=("
            << newProj.origin.x << "," << newProj.origin.y << "," << newProj.origin.z
            << ") velocity=(" << newProj.velocity.x << "," << newProj.velocity.y << "," << newProj.velocity.z << ")\n";
    }
}

/*  General window close callback funstion. I did nothing with it yet...
void App::window_close_callback(GLFWwindow* window) {
    if (!time_to_close) // is it really time to quit?
        glfwSetWindowShouldClose(window, GLFW_FALSE); // You can cancel the request.
}*/

int App::run(void)
{
    //glfwSetWindowCloseCallback(window, window_close_callback); // Setting the window close callback function so it will be active during runtime

    // ------ Enabling the depth test and faceculling (only for the back faces) ------
    glEnable(GL_DEPTH_TEST);
    
    glCullFace(GL_BACK);  // The default
    glEnable(GL_CULL_FACE); // assume ALL objects are non-transparent 
    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);    // Disable cursor, so that it can not leave window, and we can process movement
    glfwGetCursorPos(window, &cursorLastX, &cursorLastY);           // get first position of mouse cursor

    update_projection_matrix();
    glViewport(0, 0, width, height);    //Set viewport

    camera.Position = glm::vec3(0.0, 10.0, 0.0);    // Setting the camera starting position

    glm::vec4 my_rgba = glm::vec4(r,g,b,a); // Creatiing the vector for the color input of the object
    a = 0.1f;
    glm::vec4 transparent_rgba = glm::vec4(r, g, b, a);
    float tile_size = 1.0f / 16;            // Size of one tile on the texture atlas
    glm::vec2 tile_offset = glm::vec2(0.0f* tile_size, 0.0f * tile_size);   // Setting the position of the desired tile of the texture atlas

    // Setting variables for the FPS calculations
    double last_frame_time = glfwGetTime();
    last_time = Clock::now();
    frame_count = 0;

    my_shader.activate();   // Because we only have one shader

    // ----- Setting the parameters of the desired lights. (All parameters needs to be set from the s_lights struct for it to work >.<)------
    // Currently these parameters generatte a green and a blue pointlight at the top and bottom of the loaded in textured cube
    const int maxlights = 4;
    my_shader.setUniform("N_matrix", Ground.normal_matrix); //Needed for light calculations

    brightness = 10;
    // fallback pozice pokud Lamp není v scene
    float terrainY = getTerrainHeight(13.5f, 17.5f, Ground.heightmap);

    // spočítat lampTopWorldPos v scope run()
    glm::vec3 lampTopWorldPos(13.5f, terrainY + 19.0f, 20.5f); // fallback
    auto itLamp = scene.find("Lamp");
    if (itLamp != scene.end()) {
        const Model& lampModel = itLamp->second;
        float maxY = -std::numeric_limits<float>::infinity();
        for (auto const& v : lampModel.vertices) {
            maxY = std::max(maxY, v.position.y);
        }
        if (maxY != -std::numeric_limits<float>::infinity()) {
            lampTopWorldPos.x = lampModel.origin.x;
            lampTopWorldPos.z = lampModel.origin.z;
            lampTopWorldPos.y = lampModel.origin.y + maxY * lampModel.scale.y - 0.15f; // malý offset pod vrcholem
        }
    }

    for (int i = 0; i < maxlights; ++i) {
        if (i == 0) {
            my_shader.setUniform("lights[0].position", glm::vec4(0.0f, 100.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[0].ambientM", glm::vec3(0.2f, 0.2f, 0.2f));
            my_shader.setUniform("lights[0].diffuseM", glm::vec3(1.0f, 0.95f, 0.8f));
            my_shader.setUniform("lights[0].specularM", glm::vec3(1.0f, 0.95f, 0.9f));
            my_shader.setUniform("lights[0].consAttenuation", 1.0f);
            my_shader.setUniform("lights[0].linAttenuation", 1.0f);
            my_shader.setUniform("lights[0].quadAttenuation", 1.0f);
            my_shader.setUniform("lights[0].cutoff", 180.0f);
            my_shader.setUniform("lights[0].direction", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[0].exponent", 0);
        }
        else if (i == 1) {
            my_shader.setUniform("lights[1].position", glm::vec4(camera.Position, 1.0f));
            my_shader.setUniform("lights[1].ambientM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].diffuseM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].specularM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].consAttenuation", 1.0f);
            my_shader.setUniform("lights[1].linAttenuation", 0.09f);
            my_shader.setUniform("lights[1].quadAttenuation", 0.032f);
            my_shader.setUniform("lights[1].cutoff", 20.0f);
            my_shader.setUniform("lights[1].direction", camera.Front);
            my_shader.setUniform("lights[1].exponent", 20.0f);
        }
        else if (i == 2) {
            my_shader.setUniform("lights[2].position", glm::vec4(lampTopWorldPos, 1.0f));
            my_shader.setUniform("lights[2].ambientM", glm::vec3(0.15f, 0.08f, 0.03f));
            my_shader.setUniform("lights[2].diffuseM", glm::vec3(1.0f * brightness, 0.4f * brightness, 0.0f * brightness));
            my_shader.setUniform("lights[2].specularM", glm::vec3(1.0f * brightness, 0.6f * brightness, 0.2f * brightness));
            my_shader.setUniform("lights[2].consAttenuation", 1.0f);
            my_shader.setUniform("lights[2].linAttenuation", 0.09f);
            my_shader.setUniform("lights[2].quadAttenuation", 0.032f);
            my_shader.setUniform("lights[2].cutoff", 180.0f);
            my_shader.setUniform("lights[2].direction", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[2].exponent", 20.0f);
        }
        else if (i == 3) {
            my_shader.setUniform("lights[3].position", glm::vec4(5.0f, 0.5f, 5.0f, 1.0f));
            my_shader.setUniform("lights[3].ambientM", glm::vec3(0.08f, 0.18f, 0.06f));
            my_shader.setUniform("lights[3].diffuseM", glm::vec3(0.3f * brightness, 0.95f * brightness, 0.3f * brightness));
            my_shader.setUniform("lights[3].specularM", glm::vec3(0.6f * brightness, 1.0f * brightness, 0.6f * brightness));
            my_shader.setUniform("lights[3].consAttenuation", 1.0f);
            my_shader.setUniform("lights[3].linAttenuation", 0.09f);
            my_shader.setUniform("lights[3].quadAttenuation", 0.032f);
            my_shader.setUniform("lights[3].cutoff", 180.0f);
            my_shader.setUniform("lights[3].direction", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[3].exponent", 20.0f);
        }
        else if (i == 4) {
            my_shader.setUniform("lights[4].position", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            my_shader.setUniform("lights[4].ambientM", glm::vec3(0.0f));
            my_shader.setUniform("lights[4].diffuseM", glm::vec3(1.0f, 1.0f, 0.95f)); // teplé bílé
            my_shader.setUniform("lights[4].specularM", glm::vec3(1.0f, 1.0f, 0.95f));
            my_shader.setUniform("lights[4].consAttenuation", 1.0f);
            my_shader.setUniform("lights[4].linAttenuation", 0.02f);
            my_shader.setUniform("lights[4].quadAttenuation", 0.005f);
            my_shader.setUniform("lights[4].cutoff", 25.0f); // úhel kužele v stupních
            my_shader.setUniform("lights[4].direction", glm::vec3(0.0f, -1.0f, 0.0f));
            my_shader.setUniform("lights[4].exponent", 40.0f);
        }
    }
    // --- Set general parameters for all lights ---
    my_shader.setUniform("ambient_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("diffuse_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("specular_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("specular_shinines", 80.0f);
    //------ ------

    std::vector<Model*> transparent;    // temporary, vector of pointers to transparent objects
    transparent.reserve(scene.size());  // reserve size for all objects to avoid reallocation
    
    //----- 2D & 3D audio -----    
    // position, playLooped = true, startPaused = true, track = true
    irrklang::ISound* music = engine->play3D("resources/music/birds.mp3", irrklang::vec3df(0, 0, 0), false, true, true); // loop, start paused, enable 3D sound
    irrklang::ISound* BackgroundMusic = BackgroundEngine -> play2D("resources/music/Dune_Official _Soundtrack _Pauls_Dream_Hans_Zimmer.mp3", true, true, false); // loop, start paused, enable 3D sound
    irrklang::ISound* planeSound = engine->play3D("resources/music/plane.mp3",
    irrklang::vec3df(0.0f, 0.0f, 0.0f), /*looped=*/true, /*startPaused=*/true, /*track=*/true);
    double last_ouch_time = -10.0;
    double last_glass_time = -10.0;
    if (planeSound) {
        planeSound->setMinDistance(20.0f);   // vzdálenost pro plné slyšení
        planeSound->setVolume(10.0f);
        planeSound->setIsPaused(false);
    }
    // The minimum distance is the distance in which the sound gets played at maximum volume.
    music->setMinDistance(5.0f); // Make sound source bigger. (Default = 1.0 = "small" sound source.)
    music->setIsPaused(false); // Start playing
    
    if (BackgroundMusic) {
        std::cout << "Current volume:" << BackgroundMusic->getVolume(); // float, [0.0 to 1.0]
            // Prepare sound parameters: set volume, effects, etc.
        BackgroundMusic->setVolume(0.3);
        // Unpause
        BackgroundMusic->setIsPaused(false);
    }

    if (music) {
        std::cout << "Current volume:" << music->getVolume(); // float, [0.0 to 1.0]
        // Prepare sound parameters: set volume, effects, etc.
        music->setVolume(0.8);
        // Unpause
        music->setIsPaused(false);
    }

    float eyeHeight = 1.8f;
    // Start background worker
    if (!tracker.startWorker()) return -1;
    std::uint64_t last_seq = 0;

    while (!glfwWindowShouldClose(window)) {    //Main loop of the application
        
        // Set all the callback functions we want to be active during the runtime of the application (Only the set functions with declaration will be active, just declaring a callback function is not enough)
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetWindowTitle(window, std::string("FPS: ").append(std::to_string(fps)).append(" Vsync: ").append(std::to_string(vsync_on)).c_str());   //Set the window title to show current FPS of the application and if Vsync is active or not
        glfwSetWindowSizeCallback(window,framebuffer_size_callback);

        //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // Set background color
        if (night) {
            glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
        }
        else { glClearColor(0.53f, 0.81f, 0.92f, 1.0f); }  // sky blue RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear canvas

        double current_frame_time = glfwGetTime(); //Needed for FPS calculation

        double delta_t = current_frame_time - last_frame_time; // render time of the last frame 
        last_frame_time = current_frame_time;

        // uložit předchozí pozici kamery pro rollback pokud dojde ke kolizi
        glm::vec3 prevCameraPos = camera.Position;

        camera.ProcessInput(window, delta_t); // process keys etc.

        // --- Process ground colision ---
        float terrainY = getTerrainHeight(camera.Position.x, camera.Position.z, Ground.heightmap);
        float minEyeY = terrainY + eyeHeight;
        if (camera.Position.y < minEyeY) {
            camera.Position.y = minEyeY;
            camera.Velocity.y = 0.0f;
            camera.onground = true;
        }
        else {
            camera.onground = false;
        }

        // --- jednoduchá kolize kamera vs modely (sphere-AABB) ---
        const float cameraRadius = 0.75f; // upravte podle velikosti "hlavy"
        bool collision = false;
        std::string collidedName;
        glm::vec3 collidedPos(0.0f);

        for (auto const& [name, model] : scene) {
            if (!model.solid) continue;
            if (model.intersectsSphere(camera.Position, cameraRadius)) {
                collision = true;
                collidedName = name;
                collidedPos = model.origin;
                break;
            }
        }

        if (collision) {
            // rollback kamery
            camera.Position = prevCameraPos;
            camera.Velocity = glm::vec3(0.0f);

            if (!collidedName.empty()) {
                double now = glfwGetTime();

                // kolize s kaktusem -> ouch
                if (collidedName.rfind("Cactus:", 0) == 0) {
                    const double ouchCooldown = 1.5; // s
                    if (!mute && (now - last_ouch_time) > ouchCooldown) {
                        engine->play3D("resources/music/ouch.mp3",
                            irrklang::vec3df(collidedPos.x, collidedPos.y, collidedPos.z),
                            false, false, false);
                        last_ouch_time = now;
                    }
                }
                // kolize s transparentním modelem -> glass hit
                else if (collidedName == "trasparent_block") {
                    const double glassCooldown = 1.5; // s
                    if (!mute && (now - last_glass_time) > glassCooldown) {
                        engine->play3D("resources/music/wine-glass-hit.mp3",
                            irrklang::vec3df(collidedPos.x, collidedPos.y, collidedPos.z),
                            false, false, false);
                        last_glass_time = now;
                    }
                }
            }
        }

        my_shader.setUniform("uV_m", camera.GetViewMatrix());   // Update the view matrix based on the viewmatrix of the camera
        my_shader.setUniform("uP_m", projection_matrix);        
        
        // --- Set the color and texture tile (from texture atlas) of the object ---     
        tile_offset = glm::vec2(14.0f * tile_size, 4.0f * tile_size);
        my_shader.setUniform("my_color", my_rgba);
        my_shader.setUniform("tileSize", tile_size);
        my_shader.setUniform("tileOffset", tile_offset);  


        my_shader.setUniform("lights[1].position", glm::vec4(camera.Position, 1.0f));
        my_shader.setUniform("lights[1].direction", glm::vec3(camera.Front.x * delta_t, camera.Front.y * delta_t, camera.Front.z * delta_t));

        // --- make lights[3] red and blinking ---
        {
            // parametry vzoru
            const double blinkOn = 0.12;   // délka jednoho "on" v sekundách
            const double blinkGap = 0.12;  // mezera mezi bliknutími
            const double offDuration = 3.0; // pauza mezi dvojicemi bliknutí
            const double seqDuration = 2.0 * (blinkOn + blinkGap); // délka sekvence 2 bliknutí (včetně mezer)
            const double cycle = seqDuration + offDuration; // celkový cyklus

            double t = glfwGetTime();
            double phase = std::fmod(t, cycle);

            bool on = false;
            // první bliknutí: [0, blinkOn)
            if (phase < blinkOn) {
                on = true;
            }
            // mezera po prvním bliknutí: [blinkOn, blinkOn+blinkGap) -> off
            // druhé bliknutí: [blinkOn+blinkGap, blinkOn+blinkGap+blinkOn)
            else if (phase >= (blinkOn + blinkGap) && phase < (2.0 * blinkOn + blinkGap)) {
                on = true;
            }
            // jinak off (včetně offDuration)

            float blinkFactor = on ? 1.0f : 0.0f;

            const float intensityMul = 10.0f; 
            glm::vec3 redAmbient = glm::vec3(0.04f * blinkFactor, 0.0f, 0.0f);
            glm::vec3 redDiffuse = glm::vec3(intensityMul * brightness * blinkFactor, 0.0f, 0.0f);
            glm::vec3 redSpecular = glm::vec3(1.2f * brightness * blinkFactor, 0.2f * blinkFactor, 0.2f * blinkFactor);


            my_shader.setUniform("lights[3].ambientM", redAmbient);
            my_shader.setUniform("lights[3].diffuseM", redDiffuse);
            my_shader.setUniform("lights[3].specularM", redSpecular);
        }
        
        // --- set the 3D audio ---
        // move sound source
        irrklang::vec3df newPosition(20.0, 10.0, 20.0);
        music->setPosition(newPosition);
        // move Listener (similar to Camera)
        irrklang::vec3df position(camera.Position.x, camera.Position.y, camera.Position.z); // position of the listener
        irrklang::vec3df lookDirection(camera.Front.x, camera.Front.y, camera.Front.z); // the direction the listener looks into
        irrklang::vec3df velPerSecond(0, 0, 0); // only relevant for doppler effects
        irrklang::vec3df upVector(camera.Up.x, camera.Up.y, camera.Up.z); // where 'up' is in your 3D scene
        engine->setListenerPosition(position, lookDirection, velPerSecond, upVector);

        if (mute) {
            music->setIsPaused(true);
            BackgroundMusic->setIsPaused(true);
            if (planeSound) planeSound->setIsPaused(true);
        }
        else {
            music->setIsPaused(false);
            BackgroundMusic->setIsPaused(false);
            if (planeSound) planeSound->setIsPaused(false);
        }
    
        if (music && music->isFinished()){
            music->drop();
            music = nullptr;
        }
                        
        // draw all models in the scene
        glFrontFace(GL_CW);
        Ground.draw(translate, rotate, scale);
        glFrontFace(GL_CCW);
        if (auto res = tracker.getLatest(last_seq)) {
            if (res->face_found) {
                std::cout << "Face at px: " << res->center_px
                    << " norm: " << res->center_norm << " size_px: " << res->face_size_px << '\n';
                float ndcX = -(res->center_norm.x * 2.0f - 1.0f);
                float ndcY = 1.0f - res->center_norm.y * 2.0f;
                glm::vec3 targetPos(ndcX, ndcY, 0.0f); // new position from face tracker
                float alpha = 0.1f; // smoothing factor: smaller = smoother, slower
                FaceTracResult = alpha * targetPos + (1.0f - alpha) * FaceTracResult;

                // --- Updated: face-size-based forward/back control (INVERTED) ---
                // Now: larger face (closer) -> move FORWARD; smaller face (farther) -> move BACKWARD.
                if (face_control_enabled) {
                    float faceSize = res->face_size_px;
                    if (faceSize > 0.0f) {
                        float target = face_control_target_px;
                        float dead = face_control_deadzone_px;
                        // inverted error: positive when face is larger/closer than target
                        float err = faceSize - target; // positive => closer -> move forward
                        if (std::fabs(err) > dead) {
                            float norm = err / target; // normalized error
                            // increased maximum per-frame movement (clamp wider because speed increased)
                            float moveAmount = glm::clamp(norm * face_control_speed * static_cast<float>(delta_t), -4.0f, 4.0f);
                            // apply horizontal movement along camera front; Y handled by ground/collision code
                            glm::vec3 forwardXZ = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
                            camera.Position += forwardXZ * moveAmount;
                        }
                    }
                }
            }
        }
        else {
            std::cout << "No face detected\n";
            //FaceTracResult = glm::vec3(0.0f, 0.0f, 0.0f);
        }
                
        transparent.clear();
        // FIRST PART - draw all non-transparent in any order
        for (auto& [name, model] : scene) {
            my_shader.setUniform("N_matrix", model.normal_matrix); //Needed for light calculations
            if (!model.transparent) {
                if (name == "my_first_object") {
                    tile_offset = glm::vec2(4.0f * tile_size, 0.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }else if (name == "Moving_model") {
                    tile_offset = glm::vec2(0.0f * tile_size, 3.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    float height = getTerrainHeight(model.origin.x, model.origin.z, Ground.heightmap);
                    model.circlepath(delta_t, height, 90.0f, 0.2f);
                    my_shader.setUniform("lights[3].position", glm::vec4(model.origin, 1.0f));
                    if (planeSound) {
                        planeSound->setPosition(irrklang::vec3df(model.origin.x, model.origin.y, model.origin.z));
                        planeSound->setVelocity(irrklang::vec3df(model.velocity.x, model.velocity.y, model.velocity.z));
                    }
                    model.draw(translate, rotate, scale);
                }
                else if (name == "wooden_base") {
                    tile_offset = glm::vec2(8.0f * tile_size, 1.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }
                else if (name == "light_2") {
                    tile_offset = glm::vec2(1.0f * tile_size, 1.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }
                else if (name.rfind("throwable_rock", 0) == 0) {
                    // Pokud má projektil nenulovou rychlost, považujeme ho za "ve vzduchu" a aktualizujeme fyziku
                    const float velocityEps = 1e-4f;
                    bool inAir = glm::length(model.velocity) > velocityEps;

                    if (inAir) {
                        // Integrace ve více sub-krocích, aby se zabránilo "tunnelingu"
                        float remaining = static_cast<float>(delta_t);
                        const float maxStep = 0.02f; // 20 ms per physics substep
                        bool landed = false;

                        while (remaining > 0.0f && !landed) {
                            float step = std::min(remaining, maxStep);
                            // flyghtpath musí akceptovat malý časový krok (step)
                            model.flyghtpath(step, FaceTracResult);
                            remaining -= step;

                            // check collision with terrain at current XY
                            float groundY = getTerrainHeight(model.origin.x, model.origin.z, Ground.heightmap);
                            const float groundEps = 0.01f; // drobný offset nad terénem
                            if (model.origin.y <= groundY + groundEps) {
                                // projektil dopadl -> posaď ho přesně na terén, zastav a označ jako pevný objekt
                                model.origin.y = groundY + groundEps;
                                model.velocity = glm::vec3(0.0f);
                                landed = true;

                                // nechme kámen zůstat ve scéně jako pevný objekt (může kolidovat s hráčem)
                                model.solid = true;
                                model.computeAABB();
                            }
                        }

                        model.draw(translate, rotate, scale);
                    }
                    else {
                        // projektil už dopadl — vykreslovat staticky (bez další fyziky)
                        model.draw(translate, rotate, scale);
                    }
                }
                else{
                    tile_offset = glm::vec2(5.0f * tile_size, 8.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }
                
            }
            else
                transparent.emplace_back(&model); // save pointer for painters algorithm
        }

        if (!leftclick) {
            //scene.erase("throwable_rock");
        }

        auto itRock = scene.find("throwable_rock");
        if (itRock != scene.end()) {
            const Model& r = itRock->second;
            std::cout << "[DEBUG] throwable_rock present; origin=("
                << r.origin.x << "," << r.origin.y << "," << r.origin.z << ") leftclick=" << leftclick << std::endl;
        }

        tile_offset = glm::vec2(3.0f * tile_size, 4.0f * tile_size);
        my_shader.setUniform("tileOffset", tile_offset);
        my_shader.setUniform("my_color", transparent_rgba);

        // SECOND PART - draw only transparent - painter's algorithm (sort by distance from camera, from far to near)
        std::sort(transparent.begin(), transparent.end(), [&](Model const* a, Model const* b) {
            glm::vec3 translation_a = glm::vec3(a->model_matrix[3]);  // get 3 values from last column of model matrix = translation
            glm::vec3 translation_b = glm::vec3(b->model_matrix[3]);  // dtto for model B
            return glm::distance(camera.Position, translation_a) < glm::distance(camera.Position, translation_b); // sort by distance from camera
            });

        // set GL for transparent objects // TODO: from lectures
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE); 
        glDisable(GL_CULL_FACE);
        // draw sorted transparent
        for (auto p : transparent) {
            my_shader.setUniform("N_matrix", p->normal_matrix);
            my_shader.setUniform("uM_m", p->model_matrix);
            p->draw();
        }
        // restore GL properties for non-transparent objects // TODO: from lectures
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);

        updateFPS();
        glfwSwapBuffers(window);        
        glfwPollEvents();
    }

    tracker.stopWorker();
    // Close OpenGL window if opened and terminate GLFW
    if (window)
        glfwDestroyWindow(window);

    return EXIT_SUCCESS;

}

App::~App()
{
    // clean-up
    cv::destroyAllWindows();
    glfwTerminate();
    my_shader.clear();
    glDeleteTextures(1, &my_texture);
    if (engine) {
        engine->drop();
    }
    engine = nullptr;
    BackgroundEngine = nullptr;
    std::cout << "Bye...\n";

}

App app;


int main()
{
    if (!app.init()) {
        std::cerr << "App initialization failed.\n";
        return 3; 
    }
    return app.run();
}
