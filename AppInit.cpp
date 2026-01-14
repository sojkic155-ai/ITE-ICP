#include "app.hpp"
#include "gl_err_callback.h"

#include <GL/glew.h>
#include <GL/wglew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

bool App::init()
{
    // Init the app runtime: GLFW window + OpenGL context + GLEW/DSA + debug output + assets + audio + camera tracker.

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        return false;

    // Request OpenGL 4.6 core profile.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(640, 480, "Prototype app", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return false;
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);

    // GLEW needs this for core profiles, otherwise you often miss modern entry points.
    glewExperimental = GL_TRUE;
    GLenum glew_ret = glewInit();

    std::cout << "glewInit() = " << glew_ret << " (" << (glew_ret == GLEW_OK ? "OK" : "ERROR") << ")\n";
    std::cout << "GLEW_VERSION: " << (const char*)glewGetString(GLEW_VERSION) << "\n";
    std::cout << "GL_VERSION: " << (const char*)glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL_VERSION: " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
    std::cout << "GLEW_ARB_direct_state_access: " << (GLEW_ARB_direct_state_access ? "yes" : "no") << "\n";
    std::cout << "glCreateTextures ptr: " << reinterpret_cast<void*>(glCreateTextures) << "\n";
    std::cout << "glTextureStorage2D ptr: " << reinterpret_cast<void*>(glTextureStorage2D) << "\n";

    // Init WGL/WGLEW-specific bits on Windows.
    glew_ret = glewInit();
    if (glew_ret != GLEW_OK) {
        throw std::runtime_error(std::string("WGLEW failed with error: ")
            + reinterpret_cast<const char*>(glewGetErrorString(glew_ret)));
    }
    else {
        std::cout << "WGLEW successfully initialized platform specific functions." << std::endl;
    }

    // Make sure we actually got the profile we asked for.
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

    // Enable OpenGL debug messages if supported.
    if (GLEW_ARB_debug_output) {
        glDebugMessageCallback(MessageCallback, 0);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        std::cout << "GL_DEBUG enabled." << std::endl;
    }
    else {
        std::cout << "GL_DEBUG NOT SUPPORTED!" << std::endl;
    }

    // This project relies on DSA calls (glCreateTextures, glNamedBufferData, ...).
    if (!GLEW_ARB_direct_state_access)
        throw std::runtime_error("No DSA :-(");

    // Print GLFW version info (runtime vs headers).
    int major, minor, revision;
    glfwGetVersion(&major, &minor, &revision);
    std::cout << "Running GLFW DLL " << major << '.' << minor << '.' << revision << std::endl;
    std::cout << "Compiled against GLFW "
        << GLFW_VERSION_MAJOR << '.' << GLFW_VERSION_MINOR << '.' << GLFW_VERSION_REVISION << std::endl;

    init_assets();

    // Enable alpha blending (needed for transparent models/textures).
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    engine = irrklang::createIrrKlangDevice();
    if (!engine)
        throw std::exception("Can not create 3D sound device");
    BackgroundEngine = engine;

    // Start face tracker camera capture (index 0).
    if (!tracker.init(0)) return false;

    return true;
}
