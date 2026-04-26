#ifndef POST_PROCESSOR_H
#define POST_PROCESSOR_H

#include <glad/gl.h>
#include <glm/glm.hpp>

class PostProcessor {
public:
    PostProcessor();
    ~PostProcessor();

    void init(int screenWidth, int screenHeight);
    void resize(int screenWidth, int screenHeight);

    // Call before rendering the 3D scene
    void beginSceneRender();
    // Call after rendering the 3D scene
    void endSceneRender();

    // Apply all post-processing effects and render to screen
    void applyEffects(float time, float boilFPS);

    bool initialized = false;

private:
    int width, height;

    // Scene FBO (render 3D scene here)
    unsigned int sceneFBO;
    unsigned int sceneColorTex;
    unsigned int sceneDepthTex;

    // Screen quad for post-processing
    unsigned int quadVAO, quadVBO;

    // Combined post-process shader
    unsigned int postShader;

    void createFBO();
    void destroyFBO();
    void createScreenQuad();

    unsigned int compileShader(unsigned int type, const char* source);
    unsigned int createShaderProgram(const char* vert, const char* frag);
};

#endif
