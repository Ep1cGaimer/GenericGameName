#ifndef OVERLAY_H
#define OVERLAY_H

#include <glad/gl.h>
#include <string>

class Overlay {
public:
    Overlay();
    ~Overlay();

    void render(GLuint textureID);

private:
    GLuint VAO, VBO;
    GLuint shaderProgram;
    bool initialized;

    void init();
    unsigned int compileShader(unsigned int type, const char* source);
};

#endif
