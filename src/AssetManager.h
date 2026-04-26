#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <string>
#include <map>
#include <memory>
#include <glad/gl.h>
#include "Model.h"
#include "TextureLoader.h"
#include "Maze.h"
#include "ParticleSystem.h"
#include <vector>
#include <glm/glm.hpp>

struct Transform {
    glm::vec3 position;
    glm::vec3 scale;
    glm::vec3 rotationAxis;
    float rotationAngle;
};

struct PropInstance {
    Model* model;
    Transform transform;
};

struct DecalInstance {
    GLuint texture;
    Transform transform;
};

class AssetManager {
public:
    static AssetManager& getInstance() {
        static AssetManager instance;
        return instance;
    }

    AssetManager(AssetManager const&) = delete;
    void operator=(AssetManager const&) = delete;

    GLuint getTexture(const std::string& name, const std::string& path, bool retroFilter = false);
    Model* getModel(const std::string& name, const std::string& path);

    void populateFromMaze(const MazeGenerator& maze);
    void renderProps(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& viewPos);
    void renderDecals(const glm::mat4& view, const glm::mat4& projection);
    void updateAndRenderParticles(float dt, const glm::mat4& view, const glm::mat4& projection);

private:
    AssetManager();

    std::map<std::string, GLuint> textures;
    std::map<std::string, std::unique_ptr<Model>> models;

    std::vector<PropInstance> props;
    std::vector<DecalInstance> decals;
    std::vector<glm::vec3> torchPositions;
    std::vector<std::unique_ptr<ParticleSystem>> particleSystems;

    unsigned int propShaderProgram;
    unsigned int decalShaderProgram;
    unsigned int decalVAO, decalVBO;
    bool decalInit;

    void initPropShader();
    void initDecals();
    unsigned int compileShader(unsigned int type, const char* source);
    unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource);
};

#endif
