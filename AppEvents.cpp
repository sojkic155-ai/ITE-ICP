#include "AppEvents.hpp"

#include <iostream>
#include <string>
#include <GLFW/glfw3.h>

// Def global counter
std::atomic<int> g_projectile_counter{ 0 };


void App::error_callback(int error, const char* description) {  //General error callback function
    std::cerr << "Error: " << description << std::endl;
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
            else {
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
            // pouze toggle flagu — nezapojuj/nezastavuj worker
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
                app->my_shader.setUniform("lights[0].diffuseM", glm::vec3(
                    static_cast<GLfloat>(0.2f * app->brightness),
                    static_cast<GLfloat>(0.2f * app->brightness),
                    static_cast<GLfloat>(0.35f * app->brightness)));
                app->my_shader.setUniform("lights[0].specularM", glm::vec3(
                    static_cast<GLfloat>(0.3f * app->brightness),
                    static_cast<GLfloat>(0.3f * app->brightness),
                    static_cast<GLfloat>(0.5f * app->brightness)));
            }
            else {//set to day
                app->night = FALSE;
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

void App::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

    // Pøevod double -> GLfloat provádíme explicitnì, aby zmizelo varování C4244.
    GLfloat dx = static_cast<GLfloat>(xpos - app->cursorLastX);
    GLfloat dy = static_cast<GLfloat>((ypos - app->cursorLastY) * -1.0);

    app->camera.ProcessMouseMovement(dx, dy);
    app->cursorLastX = xpos;
    app->cursorLastY = ypos;

}

void App::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) { //General functionality to check if the callback is working or not
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        std::cout << "[DEBUG] Mouse left pressed." << std::endl;

        // Compute forward strictly from camera yaw/pitch to ensure projectile goes exactly in view direction.
        // This avoids accidental influence from any transient offsets (napø. head-tracking code).
        glm::vec3 forward;
        {
            // use camera yaw/pitch (in degrees) same formula as Camera::updateCameraVectors()
            float yaw = app->camera.Yaw;
            float pitch = app->camera.Pitch;
            forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            forward.y = sin(glm::radians(pitch));
            forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            forward = glm::normalize(forward);
        }

        // pøipravit novou instanci projektilu (kopie šablony)
        Model newProj = app->projectile;
        // umístit projektil trochu pøed kameru, aby se hned nesrazil se sebou
        newProj.origin = app->camera.Position + forward * 1.0f;
        // nastavit rychlost pøesnì smìrem pohledu
        newProj.velocity = forward * 10.0f;
        newProj.solid = false; // než dopadne, není pevný
        // unikátní klíè, aby se staré kameny nevymazaly
        int id = ++g_projectile_counter;
        std::string key = std::string("throwable_rock_") + std::to_string(id);
        app->scene.insert_or_assign(key, newProj);
        std::cout << "[DEBUG] Inserted " << key << " origin=("
            << newProj.origin.x << "," << newProj.origin.y << "," << newProj.origin.z
            << ") velocity=(" << newProj.velocity.x << "," << newProj.velocity.y << "," << newProj.velocity.z << ")\n";
    }
}