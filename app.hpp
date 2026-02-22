#pragma once

// Original author: JJ
// Modified by: Michal Sojka

#include <iostream>
#include <opencv2/opencv.hpp>
#include <unordered_map>
#include <chrono>
#include <irrKlang/irrKlang.h>

#include "assets.hpp"
#include "Model.hpp"
#include "camera.hpp"
#include "Heightmap.hpp"
#include "FaceTracker.hpp"

class App {
public:
    // Centralized project constants (static constexpr for compile-time usage).
    // Paths
    static constexpr const char* kMusicBirdsPath = "resources/music/birds.mp3";
    static constexpr const char* kBackgroundMusicPath = "resources/music/Dune_Official _Soundtrack _Pauls_Dream_Hans_Zimmer.mp3";
    static constexpr const char* kPlaneSoundPath = "resources/music/plane.mp3";
    static constexpr const char* kOuchPath = "resources/music/ouch.mp3";
    static constexpr const char* kGlassPath = "resources/music/wine-glass-hit.mp3";

    // Audio parameters
    static constexpr float kPlaneSoundMinDistance = 20.0f;
    static constexpr float kMusicMinDistance = 5.0f;
    static constexpr float kPlaneSoundVolume = 1.0f;        // irrKlang [0..1]
    static constexpr float kBackgroundMusicVolume = 0.3f;
    static constexpr float kMusicVolume = 0.8f;

    // Rendering / gameplay
    static constexpr float kTileSize = 1.0f / 16.0f;
    static constexpr float kDefaultBrightness = 10.0f;

    // Window & GL defaults
    static constexpr int kDefaultWindowWidth = 640;
    static constexpr int kDefaultWindowHeight = 480;
    static constexpr int kGLMajor = 4;
    static constexpr int kGLMinor = 6;

    // irrKlang engine options (centralized)
    static constexpr int kIrrKlangOptions = irrklang::ESEO_DEFAULT_OPTIONS;

    // AppAssets placement/configuration
    static constexpr int kNumPoints = 75;
    static constexpr int kMinCoordinate = -100;
    static constexpr int kMaxCoordinate = 100;
    static constexpr int kMinBorder = -15;
    static constexpr int kMaxBorder = 15;
    static constexpr int kNumRocks = 25;
    static constexpr int kNumRock3 = 20;
    static constexpr int kNumRock4 = 20;

    // Gameplay / projectile
    static constexpr float kProjectileSpeed = 10.0f;
    static constexpr float kProjectileSpawnOffset = 1.0f;

    // Sound cooldowns
    static constexpr double kOuchCooldown = 1.5;
    static constexpr double kGlassCooldown = 1.5;

    // App entry point object: constructor sets up default state, real heavy init belongs in init().
    App();

    //------ Public API ------
    // Set up window, GL state, callbacks, and core resources.
    bool init(void);

    // Main loop: input -> update -> render until the app exits.
    int run(void);

    // Load models/textures/sounds and prepare the scene.
    void init_assets(void);

    // Update FPS counter and (usually) update the window title/debug output.
    void updateFPS(void);

    // Recompute projection when fov / window size changes.
    void update_projection_matrix(void);

    // Toggle between windowed and fullscreen and keep last window placement.
    void switch_to_fullscreen(void);

    // Sample terrain height from the heightmap for world position (x,z).
    float getTerrainHeight(float x, float z, const std::vector<std::vector<float>>& heightmap) const;

    //------ Texture helpers ------
    // Load texture from disk and upload it to GPU.
    GLuint textureInit(const std::filesystem::path& file_name);

    // Create a GL texture from an OpenCV Mat.
    GLuint gen_tex(cv::Mat& image);

    // Clean up all resources (GL, audio, capture, etc.).
    ~App();

private:
    // Cascade classifier used by face tracker. Stored here so loader/trackers share the same trained data.
    cv::CascadeClassifier face_cascade = cv::CascadeClassifier("resources/haarcascade_frontalface_default.xml");

    // Clear color / UI color values (default white).
    GLfloat r{ 1.0f }, g{ 1.0f }, b{ 1.0f }, a{ 1.0f };

    // GLFW window handle.
    GLFWwindow* window = NULL;

    // Various runtime toggles / states.
    bool cursor_state = false;
    bool mute = false;
    bool night = false;
    bool flashlight = false;
    bool leftclick = false;
    float brightness = 0.0f;

    //------ GLFW callbacks ------
    // GLFW requires C-style static functions for callbacks; they retrieve the App instance via the window user pointer.
    static void error_callback(int error, const char* description);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

    //------ Model transform ------
    // Default transform used by quick-testing code (translate/rotate/scale applied to drawn models).
    glm::vec3 translate = glm::vec3(0.0f);
    glm::vec3 rotate = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    //------ Camera / projection ------
    int width, height;
    float fov;
    glm::mat4 projection_matrix;

    // Stored cursor position for mouse delta computation.
    double cursorLastX{ 0 };
    double cursorLastY{ 0 };

    // Ignore a single mouse-delta after mode switch to avoid camera jump.
    bool ignore_mouse_delta = false;

    //------ Fullscreen bookkeeping ------
    // Save last windowed placement so we can properly restore when exiting fullscreen.
    int last_window_xpos = 0;
    int last_window_ypos = 0;
    int last_window_height = 0;
    int last_window_width = 0;
    GLFWmonitor* last_window_monitor;
    bool fullscreen = false;

    //------ VSync ------
    // When true, swap buffers is synced to the monitor refresh rate.
    bool vsync_on = true;

    //------ FPS counter -----
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint last_time;
    int frame_count;
    int fps = 0;

    //------ 3D sound ------
    // irrKlang engines for positional audio and background music.
    irrklang::ISoundEngine* engine = nullptr;
    irrklang::ISoundEngine* BackgroundEngine = nullptr;

    // Projectile throw state (start + direction) and its model instance.
    glm::vec3 throw_start = glm::vec3(0.0f);
    glm::vec3 throw_dir = glm::vec3(0.0f);
    Model projectile;

    // Latest face tracking output mapped into world-space-ish direction/position.
    glm::vec3 FaceTracResult = glm::vec3(0.0f, 0.0f, 0.0f);

    //------ Face-control parameters ------
    // Optional camera/player control driven by detected face size/position.
    bool face_control_enabled = false;         // toggle face-based movement (default off)
    float face_control_target_px = 200.0f;     // desired face size in px (tunable)
    float face_control_deadzone_px = 15.0f;    // deadzone in px (no movement if within)
    float face_control_speed = 20.0f;          // movement gain (units per second)

    //------ Movement extras: crouch / walk ------
    // CTRL = crouch (lower eye height + slower speed), SHIFT = walk (slower speed)
    bool crouch_pressed = false;
    bool walk_pressed = false;
    float eyeHeightStanding = 1.8f;
    float eyeHeightCrouch = 1.0f;
    float eyeHeight = 1.8f;

protected:
    // Video capture device used by FaceTracker (kept protected for potential subclass access).
    cv::VideoCapture capture;

    Camera camera;
    ShaderProgram my_shader;

    // Main texture used by the scene (or UI) if only one is needed.
    GLuint my_texture;

    Heightmap Ground;

    // All scene objects addressable by a string key.
    std::unordered_map<std::string, Model> scene;

    FaceTracker tracker;
};
