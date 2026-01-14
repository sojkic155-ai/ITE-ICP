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
#include "AppEvents.hpp"
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
    : fov(45.0f),
      width(0),
      height(0),
      projection_matrix(1.0f),
      last_window_monitor(nullptr),
      frame_count(0),
      my_texture(0)
{
    //------ default constructor ------
    std::cout << "Constructed...\n";

}


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

    float terrainY = getTerrainHeight(13.5f, 17.5f, Ground.heightmap);

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
            lampTopWorldPos.y = lampModel.origin.y + maxY * lampModel.scale.y - 0.15f;
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
            my_shader.setUniform("lights[4].diffuseM", glm::vec3(1.0f, 1.0f, 0.95f));
            my_shader.setUniform("lights[4].specularM", glm::vec3(1.0f, 1.0f, 0.95f));
            my_shader.setUniform("lights[4].consAttenuation", 1.0f);
            my_shader.setUniform("lights[4].linAttenuation", 0.02f);
            my_shader.setUniform("lights[4].quadAttenuation", 0.005f);
            my_shader.setUniform("lights[4].cutoff", 25.0f);
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
        planeSound->setMinDistance(20.0f);
        planeSound->setVolume(10.0f);
        planeSound->setIsPaused(false);
    }
    // The minimum distance is the distance in which the sound gets played at maximum volume.
    music->setMinDistance(5.0f); // Make sound source bigger. (Default = 1.0 = "small" sound source.)
    music->setIsPaused(false); // Start playing
    
    if (BackgroundMusic) {
        std::cout << "Current volume:" << BackgroundMusic->getVolume(); // float, [0.0 to 1.0]
            // Prepare sound parameters: set volume, effects, etc.
        BackgroundMusic->setVolume(0.3f);
        // Unpause
        BackgroundMusic->setIsPaused(false);
    }

    if (music) {
        std::cout << "Current volume:" << music->getVolume(); // float, [0.0 to 1.0]
        // Prepare sound parameters: set volume, effects, etc.
        music->setVolume(0.8f);
        // Unpause
        music->setIsPaused(false);
    }

    float eyeHeight = 1.8f;
    // Start background worker (restore original behavior)
    if (!tracker.startWorker()) return -1;
    std::uint64_t last_seq = 0;

    while (!glfwWindowShouldClose(window)) {    //Main loop of the application
        
        // Set all the callback functions we want to be active during the runtime of the application (Only the set functions with declaration will be active, just declaring a callback function is not enough)
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetWindowTitle(window, std::string("FPS: ").append(std::to_string(fps)).append(" Vsync: ").append(std::to_string(vsync_on)).c_str());   //Set the window title to show current FPS of the application and if Vsync is active or not
        glfwSetWindowSizeCallback(window,framebuffer_size_callback);

        if (night) {
            glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
        }
        else { glClearColor(0.53f, 0.81f, 0.92f, 1.0f); }  // sky blue RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear canvas

        double current_frame_time = glfwGetTime(); //Needed for FPS calculation

        double delta_t = current_frame_time - last_frame_time; 
        last_frame_time = current_frame_time;

        glm::vec3 prevCameraPos = camera.Position;

        camera.ProcessInput(window, static_cast<float>(delta_t)); 

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

        // ---  (sphere-AABB) ---
        const float cameraRadius = 0.75f;
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
            // camera rollback
            camera.Position = prevCameraPos;
            camera.Velocity = glm::vec3(0.0f);

            if (!collidedName.empty()) {
                double now = glfwGetTime();

                // collision with cactus -> ouch
                if (collidedName.rfind("Cactus:", 0) == 0) {
                    const double ouchCooldown = 1.5; // s
                    if (!mute && (now - last_ouch_time) > ouchCooldown) {
                        engine->play3D("resources/music/ouch.mp3",
                            irrklang::vec3df(collidedPos.x, collidedPos.y, collidedPos.z),
                            false, false, false);
                        last_ouch_time = now;
                    }
                }
                // collision with transparent model -> glass hit
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
            const double blinkOn = 0.12;   
            const double blinkGap = 0.12;
            const double offDuration = 3.0;
            const double seqDuration = 2.0 * (blinkOn + blinkGap);
            const double cycle = seqDuration + offDuration;

            double t = glfwGetTime();
            double phase = std::fmod(t, cycle);

            bool on = false;

            if (phase < blinkOn) {
                on = true;
            }

            else if (phase >= (blinkOn + blinkGap) && phase < (2.0 * blinkOn + blinkGap)) {
                on = true;
            }

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
                        
        // Terrain draw uses opposite winding.
        glFrontFace(GL_CW);
        Ground.draw(translate, rotate, scale);
        glFrontFace(GL_CCW);


        // Optional face-control: uses detected face size to move camera forward/backward.
        if (face_control_enabled && tracker.workerRunning()) {
            if (auto res = tracker.getLatest(last_seq)) {
                if (res->face_found) {
                    std::cout << "Face at px: " << res->center_px
                        << " norm: " << res->center_norm << " size_px: " << res->face_size_px << '\n';
                    float ndcX = -(res->center_norm.x * 2.0f - 1.0f);
                    float ndcY = 1.0f - res->center_norm.y * 2.0f;
                    glm::vec3 targetPos(ndcX, ndcY, 0.0f);
                    float alpha = 0.1f; // smoothing factor
                    FaceTracResult = alpha * targetPos + (1.0f - alpha) * FaceTracResult;

                    if (face_control_enabled) {
                        float error = res->face_size_px - face_control_target_px;
                        if (std::abs(error) > face_control_deadzone_px) {
                            float dirSign = (error > 0.0f) ? 1.0f : -1.0f;
                            glm::vec3 front_xz = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
                            camera.Position += front_xz * (face_control_speed * dirSign) * static_cast<float>(delta_t);
                        }
                    }
                }
            }
        }
        
        // Draw non-transparent models first; collect transparent ones for later sorting.
        transparent.clear();

        for (auto& [name, model] : scene) {
            my_shader.setUniform("N_matrix", model.normal_matrix);
            if (!model.transparent) {
                if (name == "my_first_object") {
                    tile_offset = glm::vec2(4.0f * tile_size, 0.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }else if (name == "Moving_model") {
                    tile_offset = glm::vec2(0.0f * tile_size, 3.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    float height = getTerrainHeight(model.origin.x, model.origin.z, Ground.heightmap);
                    model.circlepath(static_cast<float>(delta_t), height, 90.0f, 0.2f);
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
                    // Projectiles get simple physics until they "land" on terrain.
                    const float velocityEps = 1e-4f;
                    bool inAir = glm::length(model.velocity) > velocityEps;

                    if (inAir) {
                        float remaining = static_cast<float>(delta_t);
                        const float maxStep = 0.02f; // 20 ms per physics substep
                        bool landed = false;

                        while (remaining > 0.0f && !landed) {
                            float step = std::min(remaining, maxStep);
                            model.flyghtpath(step, FaceTracResult);
                            remaining -= step;

                            // check collision with terrain at current XY
                            float groundY = getTerrainHeight(model.origin.x, model.origin.z, Ground.heightmap);
                            const float groundEps = 0.01f;
                            if (model.origin.y <= groundY + groundEps) {
                                model.origin.y = groundY + groundEps;
                                model.velocity = glm::vec3(0.0f);
                                landed = true;
                                model.solid = true;
                                model.computeAABB();
                            }
                        }

                        model.draw(translate, rotate, scale);
                    }
                    else {
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

    // Shutdown worker and window resources.
    if (tracker.workerRunning()) tracker.stopWorker();

    // Close OpenGL window if opened and terminate GLFW
    if (window)
        glfwDestroyWindow(window);

    return EXIT_SUCCESS;

}

App::~App()
{
    // Cleanup: windowing, GL objects, and audio.
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
