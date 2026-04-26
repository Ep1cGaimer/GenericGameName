#ifndef TEXTURE_LOADER_H
#define TEXTURE_LOADER_H

#include <string>
#include <glad/gl.h>

class TextureLoader {
public:
    // Loads an image file and returns an OpenGL texture ID.
    // Returns 0 on failure. Optionally enable retro filtering (GL_NEAREST)
    static GLuint loadTexture(const std::string& path, bool retroFilter = false);
};

#endif
