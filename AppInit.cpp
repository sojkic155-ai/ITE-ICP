#include "app.hpp"
#include "gl_err_callback.h"

#include <GL/glew.h>
#include <GL/wglew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

bool App::init()
{
    // Initialize runtime: create GLFW window + GL context, initialize GLEW/DSA,
    // enable debug callbacks (if available), load assets and start audio/capture subsystems.

    // Register a simple GLFW error callback so we get visible messages from the library.
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        return false;

    // Request the OpenGL core profile and version defined in centralized constants.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, App::kGLMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, App::kGLMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(App::kDefaultWindowWidth, App::kDefaultWindowHeight, "Prototype app", NULL, NULL);
    if (!window)
    {
        // If window creation fails, make sure to cleanup GLFW.
        glfwTerminate();
        return false;
    }

    // Store `this` pointer in the GLFW window so static callbacks can access App instance.
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);

    // GLEW needs experimental flag for core profiles to expose modern entry points.
    glewExperimental = GL_TRUE;
    GLenum glew_ret = glewInit();

    std::cout << "glewInit() = " << glew_ret << " (" << (glew_ret == GLEW_OK ? "OK" : "ERROR") << ")\n";
    std::cout << "GLEW_VERSION: " << (const char*)glewGetString(GLEW_VERSION) << "\n";
    std::cout << "GL_VERSION: " << (const char*)glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL_VERSION: " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
    std::cout << "GLEW_ARB_direct_state_access: " << (GLEW_ARB_direct_state_access ? "yes" : "no") << "\n";
    std::cout << "glCreateTextures ptr: " << reinterpret_cast<void*>(glCreateTextures) << "\n";
    std::cout << "glTextureStorage2D ptr: " << reinterpret_cast<void*>(glTextureStorage2D) << "\n";

    // Initialize platform-specific WGL/GLEW extensions on Windows.
    glew_ret = glewInit();
    if (glew_ret != GLEW_OK) {
        throw std::runtime_error(std::string("WGLEW failed with error: ")
            + reinterpret_cast<const char*>(glewGetErrorString(glew_ret)));
    }
    else {
        std::cout << "WGLEW successfully initialized platform specific functions." << std::endl;
    }

    // Verify that the created context matches the requested profile.
    GLint myint;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &myint);

    if (myint & GL_CONTEXT_CORE_PROFILE_BIT) {
        std::cout << "We are using CORE profile\n";
    }
    else if (myint & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {
        std::cout << "We are using COMPATIBILITY profile\n";
    }
    else {
        throw std::runtime_error("Unknown GL profile mask.");
    }

    // Enable synchronous debug callback when supported to get runtime GL diagnostics.
    if (GLEW_ARB_debug_output) {
        glDebugMessageCallback(MessageCallback, 0);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        std::cout << "GL_DEBUG enabled." << std::endl;
    }
    else {
        std::cout << "GL_DEBUG NOT SUPPORTED!" << std::endl;
    }

    // This project relies on Direct State Access (DSA) API; fail early if missing.
    if (!GLEW_ARB_direct_state_access)
        throw std::runtime_error("No DSA :-(");

    // Print GLFW runtime version info vs headers for quick diagnostics.
    int major, minor, revision;
    glfwGetVersion(&major, &minor, &revision);
    std::cout << "Running GLFW DLL " << major << '.' << minor << '.' << revision << std::endl;
    std::cout << "Compiled against GLFW "
        << GLFW_VERSION_MAJOR << '.' << GLFW_VERSION_MINOR << '.' << GLFW_VERSION_REVISION << std::endl;

    // Load shaders, textures, models and place objects in the scene.
    init_assets();

    // Configure blending for alpha-transparent objects.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create sound engine (irrKlang). Use centralized option flags from App constants.
    engine = irrklang::createIrrKlangDevice(irrklang::ESOD_AUTO_DETECT, App::kIrrKlangOptions);
    if (!engine)
        throw std::exception("Can not create 3D sound device");
    BackgroundEngine = engine;

    // Initialize face tracker (camera capture). Fail initialization if capture cannot be opened.
    if (!tracker.init(0)) return false;

    return true;
}
