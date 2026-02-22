#include "AppEvents.hpp"

#include <iostream>
#include <string>
#include <GLFW/glfw3.h>

// Counter used to generate unique keys for spawned projectiles.
std::atomic<int> g_projectile_counter{ 0 };

void App::error_callback(int error, const char* description) {
    // GLFW error callback.
    std::cerr << "Error: " << description << std::endl;
}

void App::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Main keyboard input handler (toggles + quick actions).
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if ((action == GLFW_PRESS) || (action == GLFW_REPEAT))
    {
        switch (key)
        {
        case GLFW_KEY_ESCAPE: // quit
            std::cout << "ESC has been pressed!\n";
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;

        case GLFW_KEY_V: // toggle vsync
            app->vsync_on = !app->vsync_on;
            glfwSwapInterval(app->vsync_on ? 1 : 0);
            break;

        case GLFW_KEY_TAB: // toggle fullscreen/windowed
            if (!app->fullscreen) {
                app->switch_to_fullscreen();
                app->fullscreen = true;
            }
            else {
                // restore windowed mode using saved placement
                glfwSetWindowMonitor(window, app->last_window_monitor,
                                     app->last_window_xpos, app->last_window_ypos,
                                     app->last_window_width, app->last_window_height,
                                     GLFW_DONT_CARE);

                glfwPollEvents();

                int fbw = 0, fbh = 0;
                glfwGetFramebufferSize(window, &fbw, &fbh);
                int ww = 0, wh = 0;
                glfwGetWindowSize(window, &ww, &wh);

                app->width = fbw > 0 ? fbw : app->last_window_width;
                app->height = fbh > 0 ? fbh : app->last_window_height;
                glViewport(0, 0, app->width, app->height);
                app->update_projection_matrix();

                glfwSetInputMode(window, GLFW_CURSOR, app->cursor_state ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);

                double cx = static_cast<double>(ww) / 2.0;
                double cy = static_cast<double>(wh) / 2.0;
                glfwSetCursorPos(window, cx, cy);
                app->cursorLastX = cx;
                app->cursorLastY = cy;
                app->ignore_mouse_delta = true;

                glfwPollEvents();

                app->fullscreen = false;
            }
            break;

        case GLFW_KEY_C: // toggle cursor lock
        {
            app->cursor_state = !app->cursor_state;

            double cx = 0.0, cy = 0.0;

            if (app->cursor_state) {
                // enable cursor
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwGetCursorPos(window, &cx, &cy);
            }
            else {
                // disable cursor
                int w = 0, h = 0;
                glfwGetWindowSize(window, &w, &h);
                if (w <= 0 || h <= 0) {
                    glfwGetFramebufferSize(window, &w, &h);
                }
                cx = static_cast<double>(w) / 2.0;
                cy = static_cast<double>(h) / 2.0;
                glfwSetCursorPos(window, cx, cy);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }

            app->cursorLastX = cx;
            app->cursorLastY = cy;
            app->ignore_mouse_delta = true;

            glfwPollEvents();
        }
        break;

        case GLFW_KEY_M: // mute/unmute audio
            app->mute = !app->mute;
            break;

        case GLFW_KEY_F: // toggle flashlight
            if (!app->flashlight) {
                app->flashlight = true;
                app->brightness = App::kDefaultBrightness;
                app->my_shader.setUniform("lights[1].ambientM", glm::vec3(0.05f, 0.05f, 0.05f));
                app->my_shader.setUniform("lights[1].diffuseM", glm::vec3(1.0f * app->brightness, 0.95f * app->brightness, 0.8f * app->brightness));
                app->my_shader.setUniform("lights[1].specularM", glm::vec3(1.0f * app->brightness, 0.95f * app->brightness, 0.9f * app->brightness));
            }
            else {
                app->flashlight = false;
                app->my_shader.setUniform("lights[1].ambientM", glm::vec3(0.0f, 0.0f, 0.0f));
                app->my_shader.setUniform("lights[1].diffuseM", glm::vec3(0.0f, 0.0f, 0.0f));
                app->my_shader.setUniform("lights[1].specularM", glm::vec3(0.0f, 0.0f, 0.0f));
            }
            break;

        case GLFW_KEY_T: // toggle face-based control
            app->face_control_enabled = !app->face_control_enabled;
            std::cout << "[FaceControl] " << (app->face_control_enabled ? "ENABLED" : "DISABLED")
                << " (target_px=" << app->face_control_target_px << ")\n";
            break;

        case GLFW_KEY_N: // toggle day/night lighting
            if (!app->night) { // night
                app->night = true;
                app->brightness = 0.1f;
                app->my_shader.setUniform("fog_color", glm::vec4(glm::vec3(0.0f), 1.0f));
                app->my_shader.setUniform("lights[0].ambientM", glm::vec3(0.05f, 0.05f, 0.1f));
                app->my_shader.setUniform("lights[0].diffuseM", glm::vec3(
                    static_cast<GLfloat>(0.2f * app->brightness),
                    static_cast<GLfloat>(0.2f * app->brightness),
                    static_cast<GLfloat>(0.35f * app->brightness)));
                app->my_shader.setUniform("lights[0].specularM", glm::vec3(
                    static_cast<GLfloat>(0.3f * app->brightness),
                    static_cast<GLfloat>(0.3f * app->brightness),
                    static_cast<GLfloat>(0.5f * app->brightness)));
            }
            else { // day
                app->night = false;
                app->my_shader.setUniform("fog_color", glm::vec4(glm::vec3(0.85f), 1.0f));
                app->my_shader.setUniform("lights[0].ambientM", glm::vec3(0.2f, 0.2f, 0.2f));
                app->my_shader.setUniform("lights[0].diffuseM", glm::vec3(
                    static_cast<GLfloat>(1.0f),
                    static_cast<GLfloat>(0.95f),
                    static_cast<GLfloat>(0.8f)));
                app->my_shader.setUniform("lights[0].specularM", glm::vec3(
                    static_cast<GLfloat>(1.0f),
                    static_cast<GLfloat>(0.95f),
                    static_cast<GLfloat>(0.9f)));
            }
            break;

        case GLFW_KEY_R: // reset camera to a safe default
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

void App::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Keep viewport and projection in sync with window resizing.
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

    glViewport(0, 0, width, height);
    if (height <= 0)
        height = 1;

    float ratio = static_cast<float>(width) / height;
    app->projection_matrix = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 20000.0f);
}

void App::switch_to_fullscreen(void) {
    // Switch to fullscreen
    glfwGetWindowPos(window, &last_window_xpos, &last_window_ypos);
    glfwGetWindowSize(window, &last_window_width, &last_window_height);

    // Record current monitor
    last_window_monitor = glfwGetWindowMonitor(window);

    // Monitor - window center (multi-monitor).
    int wx, wy, ww, wh;
    glfwGetWindowPos(window, &wx, &wy);
    glfwGetWindowSize(window, &ww, &wh);
    int centerX = wx + ww / 2;
    int centerY = wy + wh / 2;

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    GLFWmonitor* target = glfwGetPrimaryMonitor(); // fallback

    for (int i = 0; i < count; ++i) {
        int mx = 0, my = 0;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const GLFWvidmode* vm = glfwGetVideoMode(monitors[i]);
        if (!vm) continue;
        int mw = vm->width;
        int mh = vm->height;
        if (centerX >= mx && centerX < mx + mw && centerY >= my && centerY < my + mh) {
            target = monitors[i];
            break;
        }
    }

    // Switch to fullscreen on chosen monitor.
    const GLFWvidmode* mode = glfwGetVideoMode(target);
    if (!mode) {
        // fallback
        return;
    }

    glfwSetWindowMonitor(window, target, 0, 0, mode->width, mode->height, mode->refreshRate);

    glfwPollEvents();

    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    width = fbw > 0 ? fbw : mode->width;
    height = fbh > 0 ? fbh : mode->height;

    int ww_after = 0, wh_after = 0;
    glfwGetWindowSize(window, &ww_after, &wh_after);
    double cx = static_cast<double>(ww_after) / 2.0;
    double cy = static_cast<double>(wh_after) / 2.0;
    glfwSetCursorPos(window, cx, cy);
    cursorLastX = cx;
    cursorLastY = cy;
    ignore_mouse_delta = true;

    glViewport(0, 0, width, height);
    update_projection_matrix();
}

void App::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    // Mouse look: convert cursor delta into yaw/pitch input.
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if (app->ignore_mouse_delta) {
        app->cursorLastX = xpos;
        app->cursorLastY = ypos;
        app->ignore_mouse_delta = false;
        return;
    }

    // Explicit cast (double -> GLfloat) to avoid precision-loss warnings.
    GLfloat dx = static_cast<GLfloat>(xpos - app->cursorLastX);
    GLfloat dy = static_cast<GLfloat>((ypos - app->cursorLastY) * -1.0);

    app->camera.ProcessMouseMovement(dx, dy);
    app->cursorLastX = xpos;
    app->cursorLastY = ypos;
}

void App::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    // Mouse input handler (currently used for spawning a projectile on left click).
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        std::cout << "[DEBUG] Mouse left pressed." << std::endl;

        // Build forward direction from yaw/pitch to match the view direction exactly.
        glm::vec3 forward;
        {
            float yaw = app->camera.Yaw;
            float pitch = app->camera.Pitch;
            forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            forward.y = sin(glm::radians(pitch));
            forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            forward = glm::normalize(forward);
        }

        Model newProj = app->projectile;
        newProj.origin = app->camera.Position + forward * App::kProjectileSpawnOffset; // spawn slightly in front of the camera
        newProj.velocity = forward * App::kProjectileSpeed;
        newProj.solid = false;

        int id = ++g_projectile_counter;
        std::string key = std::string("throwable_rock_") + std::to_string(id);
        app->scene.insert_or_assign(key, newProj);

        std::cout << "[DEBUG] Inserted " << key << " origin=("
            << newProj.origin.x << "," << newProj.origin.y << "," << newProj.origin.z
            << ") velocity=(" << newProj.velocity.x << "," << newProj.velocity.y << "," << newProj.velocity.z << ")\n";
    }
}
