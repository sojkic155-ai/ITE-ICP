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
#include <algorithm>

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
    // Constructor: minimal runtime initialization only.
    // Heavy initialization should happen in init() to keep the constructor lightweight.
    std::cout << "Constructed...\n";

}


int App::run(void)
{
    // Main rendering loop entry point.
    // Set OpenGL global state that is required for 3D rendering.
    // Enable depth testing so fragments are drawn with correct occlusion.
    glEnable(GL_DEPTH_TEST);
    
    glCullFace(GL_BACK);  // The default culling face
    glEnable(GL_CULL_FACE); // assume most geometry is opaque and can be back-face culled
    
    // Hide the cursor and capture it inside the window for FPS-style camera control.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &cursorLastX, &cursorLastY);           // record initial mouse position

    update_projection_matrix();
    glViewport(0, 0, width, height);    // Set the GL viewport to the window size

    // Place the camera at the starting world position.
    camera.Position = glm::vec3(0.0, 10.0, 0.0);

    // Default color used for object tinting; alpha is adjusted for transparent pass later.
    glm::vec4 my_rgba = glm::vec4(r,g,b,a); // Creating the vector for the color input of the object
    a = 0.1f;
    glm::vec4 transparent_rgba = glm::vec4(r, g, b, a);
    float tile_size = App::kTileSize;            // Size of one tile on the texture atlas
    glm::vec2 tile_offset = glm::vec2(0.0f * tile_size, 0.0f * tile_size);   // Position of selected tile in atlas

    // Initialize timing for FPS calculation.
    double last_frame_time = glfwGetTime();
    last_time = Clock::now();
    frame_count = 0;

    // Activate shader program once; uniforms will be updated per-frame / per-object.
    my_shader.activate();

    // Configure lighting uniforms. The shader expects an array of light structs.
    const int maxlights = 4;
    my_shader.setUniform("N_matrix", Ground.normal_matrix); // Needed for per-object normal transform

    brightness = static_cast<int>(App::kDefaultBrightness);

    // Compute a fallback lamp top position using terrain height; override if Lamp model present.
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
            // place the light slightly below the top vertex to match model geometry
            lampTopWorldPos.y = lampModel.origin.y + maxY * lampModel.scale.y - 0.15f;
        }
    }

    // Set light parameters. Each block configures a different light slot.
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
            // Light bound to the camera (e.g. flashlight)
            my_shader.setUniform("lights[1].position", glm::vec4(camera.Position, 1.0f));
            my_shader.setUniform("lights[1].ambientM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].diffuseM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].specularM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].consAttenuation", 1.0f);
            my_shader.setUniform("lights[1].linAttenuation", 0.09f);
            my_shader.setUniform("lights[1].quadAttenuation", 0.032f);
            my_shader.setUniform("lights[1].cutoff", 20.0f);
            my_shader.setUniform("lights[1].direction", glm::normalize(camera.Front));
            my_shader.setUniform("lights[1].exponent", 20.0f);
        }
        else if (i == 2) {
            // Lamp light (placed at lampTopWorldPos)
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
            // Decorative point light that will be animated (blinked) later
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
    // Global light multipliers and specular shininess.
    my_shader.setUniform("ambient_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("diffuse_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("specular_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("specular_shinines", 80.0f);

    // Temporary container to collect pointers to transparent models for painter's algorithm.
    std::vector<Model*> transparent;
    transparent.reserve(scene.size());  // avoid reallocation during each frame
    
    // Audio handles: ambient 3D sounds and background stereo music.
    irrklang::ISound* music = nullptr;
    irrklang::ISound* BackgroundMusic = nullptr;
    irrklang::ISound* planeSound = nullptr;

    if (engine) {
        // Start a 3D bird sound, looped and tracked.
        music = engine->play3D(App::kMusicBirdsPath, irrklang::vec3df(0, 0, 0), false, true, true);
        if (music) {
            music->setMinDistance(App::kMusicMinDistance);
            music->setVolume(App::kMusicVolume);
            music->setIsPaused(false);
        }
        // Plane engine sound, looped.
        planeSound = engine->play3D(App::kPlaneSoundPath, irrklang::vec3df(0.0f, 0.0f, 0.0f), true, true, true);
        if (planeSound) {
            planeSound->setMinDistance(App::kPlaneSoundMinDistance);
            planeSound->setVolume(App::kPlaneSoundVolume);
            planeSound->setIsPaused(false);
        }
    }
    if (BackgroundEngine) {
        // Background 2D music (stereo), optional.
        BackgroundMusic = BackgroundEngine->play2D(App::kBackgroundMusicPath, true, true, false);
        if (BackgroundMusic) {
            // track==false above, so return may be nullptr; if non-null adjust volume and unpause
            BackgroundMusic->setVolume(App::kBackgroundMusicVolume);
            BackgroundMusic->setIsPaused(false);
        }
    }

    double last_ouch_time = -10.0;
    double last_glass_time = -10.0;

    // Start face-tracking background worker; if it fails, ensure audio resources are cleaned up.
    if (!tracker.startWorker()) {
        if (music) { music->stop(); music->drop(); music = nullptr; }
        if (BackgroundMusic) { BackgroundMusic->stop(); BackgroundMusic->drop(); BackgroundMusic = nullptr; }
        if (planeSound) { planeSound->stop(); planeSound->drop(); planeSound = nullptr; }
        return -1;
    }
    std::uint64_t last_seq = 0;

    while (!glfwWindowShouldClose(window)) {    // Main loop of the application
        
        // Ensure callbacks are registered once. Re-registering each frame is unnecessary.
        static bool callbacks_set = false;
        if (!callbacks_set) {
            glfwSetCursorPosCallback(window, cursor_position_callback);
            glfwSetMouseButtonCallback(window, mouse_button_callback);
            glfwSetWindowSizeCallback(window,framebuffer_size_callback);
            callbacks_set = true;
        }
        // Show FPS and vsync status in the window title.
        glfwSetWindowTitle(window, std::string("FPS: ").append(std::to_string(fps)).append(" Vsync: ").append(std::to_string(vsync_on)).c_str());

        // Clear color depends on day/night toggle.
        if (night) {
            glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
        }
        else { glClearColor(0.53f, 0.81f, 0.92f, 1.0f); }  // sky blue RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear frame buffer and depth buffer

        // Time since last frame (seconds)
        double current_frame_time = glfwGetTime();
        double delta_t = current_frame_time - last_frame_time; 
        last_frame_time = current_frame_time;

        // Smoothly interpolate FOV toward target if RMB is held (zoom).
        {
            float targetFov = right_mouse_down ? App::kZoomedFov : App::kDefaultFov;
            float diff = targetFov - fov;
            if (std::fabs(diff) > 0.001f) {
                float lerp = std::min(1.0f, App::kFovLerpSpeed * static_cast<float>(delta_t));
                fov += diff * lerp;
                // Recompute projection matrix using new fov
                update_projection_matrix();
            }
        }

        // Save previous camera position for collision rollback.
        glm::vec3 prevCameraPos = camera.Position;

        // Update crouch / walk state from keys (CTRL = crouch, SHIFT = walk)
        bool crouchPressed = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
        bool walkPressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
        this->crouch_pressed = crouchPressed;
        this->walk_pressed = walkPressed;

        // Smoothly interpolate eye height toward its target to avoid abrupt jumps when changing stance.
        float targetEye = crouchPressed ? this->eyeHeightCrouch : this->eyeHeightStanding;
        const float eyeInterpSpeed = 6.0f; // larger = faster transition
        float t = std::min(1.0f, eyeInterpSpeed * static_cast<float>(delta_t));
        this->eyeHeight += (targetEye - this->eyeHeight) * t;

        camera.ProcessInput(window, static_cast<float>(delta_t));

        // --- Ground collision handling (terrain height) ---
        float terrainY = getTerrainHeight(camera.Position.x, camera.Position.z, Ground.heightmap);
        float desiredMinEyeY = terrainY + this->eyeHeight;

        // If the camera is marked on-ground but slightly below desired eye height (e.g. released crouch),
        // gently raise the camera to avoid snapping.
        if (camera.onground && camera.Position.y < desiredMinEyeY) {
            camera.Position.y += (desiredMinEyeY - camera.Position.y) * t;
            camera.Velocity.y = 0.0f;
            camera.onground = true;
        }
        else {
            // Maintain grounded state if within epsilon of desired height.
            if (camera.Position.y <= desiredMinEyeY + 0.001f) {
                camera.onground = true;
            }
            else {
                camera.onground = false;
            }
        }

        // --- Simple sphere-vs-AABB collision detection for the camera ---
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
            // rollback camera to avoid penetrating solid geometry
            camera.Position = prevCameraPos;
            camera.Velocity = glm::vec3(0.0f);

            if (!collidedName.empty()) {
                double now = glfwGetTime();

                // Collision with cactus: play "ouch" sound with cooldown.
                if (collidedName.rfind("Cactus:", 0) == 0) {
                    const double ouchCooldown = 1.5; // seconds
                    if (!mute && (now - last_ouch_time) > ouchCooldown && engine) {
                        // Quick one-shot sound; no need to keep ISound* reference here.
                        engine->play3D(App::kOuchPath, irrklang::vec3df(collidedPos.x, collidedPos.y, collidedPos.z), false, false, false);
                        last_ouch_time = now;
                    }
                }
                // Collision with transparent block: play glass hit with cooldown.
                else if (collidedName == "trasparent_block") {
                    const double glassCooldown = 1.5; // seconds
                    if (!mute && (now - last_glass_time) > glassCooldown && engine) {
                        engine->play3D(App::kGlassPath, irrklang::vec3df(collidedPos.x, collidedPos.y, collidedPos.z), false, false, false);
                        last_glass_time = now;
                    }
                }
            }
        }

        // Update shader view/projection matrices for this frame.
        my_shader.setUniform("uV_m", camera.GetViewMatrix());   // view matrix from camera
        my_shader.setUniform("uP_m", projection_matrix);        

        // Set the color and texture tile (from texture atlas) of the object for this pass.
        tile_offset = glm::vec2(14.0f * tile_size, 4.0f * tile_size);
        my_shader.setUniform("my_color", my_rgba);
        my_shader.setUniform("tileSize", tile_size);
        my_shader.setUniform("tileOffset", tile_offset);  


        // Update camera-bound light position and direction.
        my_shader.setUniform("lights[1].position", glm::vec4(camera.Position, 1.0f));
        my_shader.setUniform("lights[1].direction", glm::vec3(camera.Front.x * delta_t, camera.Front.y * delta_t, camera.Front.z * delta_t));

        // --- Make lights[3] red and blinking: simple time-based blinking sequence. ---
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
        
        // --- 3D audio listener/source updates ---
        if (music) {
            // keep the 3D bird sound at a fixed point in world space for a natural ambient effect
            irrklang::vec3df newPosition(20.0, 10.0, 20.0);
            music->setPosition(newPosition);
        }
        if (engine) {
            // Update the listener to follow the camera for spatial audio.
            irrklang::vec3df position(camera.Position.x, camera.Position.y, camera.Position.z);
            irrklang::vec3df lookDirection(camera.Front.x, camera.Front.y, camera.Front.z);
            irrklang::vec3df velPerSecond(0, 0, 0); // doppler velocity (unused)
            irrklang::vec3df upVector(camera.Up.x, camera.Up.y, camera.Up.z);
            engine->setListenerPosition(position, lookDirection, velPerSecond, upVector);
        }

        // Pause/unpause sounds based on mute flag.
        if (mute) {
            if (music) music->setIsPaused(true);
            if (BackgroundMusic) BackgroundMusic->setIsPaused(true);
            if (planeSound) planeSound->setIsPaused(true);
        }
        else {
            if (music) music->setIsPaused(false);
            if (BackgroundMusic) BackgroundMusic->setIsPaused(false);
            if (planeSound) planeSound->setIsPaused(false);
        }
    
        // If a tracked sound finished, release its reference.
        if (music && music->isFinished()){
            music->drop();
            music = nullptr;
        }
                        
        // Draw terrain first; it uses opposite winding to other objects.
        glFrontFace(GL_CW);
        Ground.draw(translate, rotate, scale);
        glFrontFace(GL_CCW);


        // Optional face-control: use face detector to adjust camera forward/backward.
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
                        // Move camera depending on face size error with a deadzone.
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
        
        // Draw non-transparent models first; collect transparent ones for a separate pass.
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
                    // Animate a model along a circular path.
                    model.circlepath(static_cast<float>(delta_t), height, 90.0f, 0.2f);
                    my_shader.setUniform("lights[3].position", glm::vec4(model.origin, 1.0f));
                    if (planeSound) {
                        // Update plane sound to match the moving model's position and velocity.
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
                    // Projectiles: simple sub-stepped physics until they land on terrain.
                    const float velocityEps = 1e-4f;
                    bool inAir = glm::length(model.velocity) > velocityEps;

                    if (inAir) {
                        float remaining = static_cast<float>(delta_t);
                        const float maxStep = 0.02f; // 20 ms per physics substep
                        bool landed = false;

                        // Sub-step loop to avoid tunnelling and keep stable physics.
                        while (remaining > 0.0f && !landed) {
                            float step = std::min(remaining, maxStep);
                            model.flyghtpath(step, FaceTracResult);
                            remaining -= step;

                            // Check collision with terrain at current XY and stop when hitting ground.
                            float groundY = getTerrainHeight(model.origin.x, model.origin.z, Ground.heightmap);
                            const float groundEps = 0.01f;
                            if (model.origin.y <= groundY + groundEps) {
                                model.origin.y = groundY + groundEps;
                                model.velocity = glm::vec3(0.0f);
                                landed = true;
                                model.solid = true; // become solid after landing
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
                transparent.emplace_back(&model); // Save pointer for painter's algorithm pass.
        }

        if (!leftclick) {
            // scene.erase("throwable_rock"); // intentionally disabled: keep rock for debug
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

        // SECOND PART - draw only transparent objects.
        // Sort by distance from camera (far -> near) and render with blending enabled.
        std::sort(transparent.begin(), transparent.end(), [&](Model const* a, Model const* b) {
            glm::vec3 translation_a = glm::vec3(a->model_matrix[3]);  // extract translation from model matrix
            glm::vec3 translation_b = glm::vec3(b->model_matrix[3]);  // same for b
            return glm::distance(camera.Position, translation_a) < glm::distance(camera.Position, translation_b); // sort by distance from camera
            });

        // Configure GL for transparency: enable blending, preserve depth buffer for reads but disable writes.
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE); 
        glDisable(GL_CULL_FACE);
        // Draw sorted transparent geometry
        for (auto p : transparent) {
            my_shader.setUniform("N_matrix", p->normal_matrix);
            my_shader.setUniform("uM_m", p->model_matrix);
            p->draw();
        }
        // Restore GL state for opaque geometry rendering.
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);

        updateFPS();
        glfwSwapBuffers(window);        
        glfwPollEvents();
    }

    // Shutdown background worker if running.
    if (tracker.workerRunning()) tracker.stopWorker();

    // Release audio resources created in this scope to avoid leaks.
    if (music) {
        if (!music->isFinished()) music->stop();
        music->drop();
        music = nullptr;
    }
    if (planeSound) {
        if (!planeSound->isFinished()) planeSound->stop();
        planeSound->drop();
        planeSound = nullptr;
    }
    if (BackgroundMusic) {
        if (!BackgroundMusic->isFinished()) BackgroundMusic->stop();
        BackgroundMusic->drop();
        BackgroundMusic = nullptr;
    }

    // Close OpenGL window if opened and terminate GLFW
    if (window)
        glfwDestroyWindow(window);

    return EXIT_SUCCESS;

}

App::~App()
{
    // Destructor: cleanup of external libraries and GL resources.
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
    // Minimal main: initialize app and run. Return non-zero on init failure.
    if (!app.init()) {
        std::cerr << "App initialization failed.\n";
        return 3; 
    }
    return app.run();
}
