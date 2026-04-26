#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "GameState.h"
#include "Maze.h"
#include "MazeRenderer.h"
#include "Collision.h"
#include "PostProcessor.h"

// ----- Globals -----
GameManager game;

// Camera
glm::vec3 cameraPos   = glm::vec3(1.5f, 1.2f, 1.5f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw   = 90.0f;
float pitch = 0.0f;
float lastX = 400.0f, lastY = 300.0f;
bool firstMouse = true;

const float EYE_HEIGHT = 1.2f;

// ----- Callbacks -----
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed: y goes bottom-to-top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    // Clamp pitch
    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void processInput(GLFWwindow* window, float deltaTime, MazeCollision& collision) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float speed = 3.0f * deltaTime;
    glm::vec3 desiredPos = cameraPos;

    // Flatten front direction to XZ plane so looking up/down doesn't move vertically
    glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 right = glm::normalize(glm::cross(flatFront, cameraUp));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        desiredPos += speed * flatFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        desiredPos -= speed * flatFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        desiredPos -= right * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        desiredPos += right * speed;

    // Resolve collision (keeps Y locked)
    cameraPos = collision.resolveMovement(cameraPos, desiredPos);
    cameraPos.y = EYE_HEIGHT;
}

// ----- Main -----
int main() {
    // Init GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "Backrooms Maze", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Init GLAD
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // ---- Generate Maze ----
    MazeGenerator maze(21, 21);
    maze.generate();
    maze.addLoops(0.05f);

    std::cout << "Maze generated: " << maze.width << "x" << maze.height << std::endl;

    // ---- Build Renderer (shader is created here, after GL context) ----
    MazeRenderer renderer;
    renderer.buildMesh(maze);

    // ---- Collision ----
    MazeCollision collision(maze, 1.0f, 0.2f);

    // ---- Post-Processing (paper-drawn effect) ----
    PostProcessor postFX;
    int initW, initH;
    glfwGetFramebufferSize(window, &initW, &initH);
    postFX.init(initW, initH);

    // Visual frame-rate limiting for flipbook feel
    const float VISUAL_FPS = 12.0f;
    const float VISUAL_FRAME_TIME = 1.0f / VISUAL_FPS;
    float visualTimer = 0.0f;
    bool shouldRenderScene = true;

    // ---- Game Loop ----
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Cap delta to avoid physics explosions on lag spikes
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Input always runs at full speed
        processInput(window, deltaTime, collision);
        game.update(deltaTime);

        // Visual frame rate limiting — only re-render the 3D scene at 12fps
        visualTimer += deltaTime;
        if (visualTimer >= VISUAL_FRAME_TIME) {
            visualTimer -= VISUAL_FRAME_TIME;
            shouldRenderScene = true;
        }

        // Dynamic aspect ratio + resize FBO if needed
        int winW, winH;
        glfwGetFramebufferSize(window, &winW, &winH);
        if (winH == 0) winH = 1;
        float aspect = static_cast<float>(winW) / static_cast<float>(winH);
        postFX.resize(winW, winH);

        if (shouldRenderScene) {
            shouldRenderScene = false;

            glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

            switch (game.currentState) {
                case GameState::MENU:
                    break;
                case GameState::PLAYING:
                    // Render scene to FBO
                    postFX.beginSceneRender();
                    renderer.render(view, projection, cameraPos);
                    postFX.endSceneRender();
                    break;
                case GameState::LOSE:
                    break;
            }
        }

        // Always apply post-processing and display (reads from last FBO content)
        glViewport(0, 0, winW, winH);
        postFX.applyEffects(currentFrame, VISUAL_FPS);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    renderer.cleanup();
    glfwTerminate();
    return 0;
}
