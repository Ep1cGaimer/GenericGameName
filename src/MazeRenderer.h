#ifndef MAZE_RENDERER_H
#define MAZE_RENDERER_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Maze.h"

class MazeRenderer {
public:
    MazeRenderer();
    ~MazeRenderer();

    void buildMesh(const MazeGenerator& maze);
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);
    void cleanup();

private:
    unsigned int wallVAO, wallVBO;
    unsigned int floorVAO, floorVBO;
    unsigned int ceilingVAO, ceilingVBO;
    unsigned int shaderProgram;
    int wallVertexCount;
    int floorVertexCount;
    int ceilingVertexCount;
    bool shaderBuilt;

    void initShader();
    unsigned int compileShader(unsigned int type, const char* source);
    unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource);
    void addQuad(std::vector<float>& vertices, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 p4, glm::vec3 normal);
};

#endif
