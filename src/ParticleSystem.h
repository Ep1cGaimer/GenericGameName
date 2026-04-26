#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <vector>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Particle {
    glm::vec3 Position, Velocity;
    glm::vec4 Color;
    float Life;
    Particle() : Position(0.0f), Velocity(0.0f), Color(1.0f), Life(0.0f) {}
};

class ParticleSystem {
public:
    ParticleSystem(unsigned int amount);
    ~ParticleSystem();

    void Update(float dt, unsigned int newParticles, glm::vec3 offset = glm::vec3(0.0f));
    void Draw(const glm::mat4& view, const glm::mat4& projection, GLuint textureID);

private:
    std::vector<Particle> particles;
    unsigned int amount;
    unsigned int shaderProgram;
    unsigned int VAO, VBO;

    void init();
    unsigned int firstUnusedParticle();
    void respawnParticle(Particle &particle, glm::vec3 offset = glm::vec3(0.0f));
    unsigned int compileShader(unsigned int type, const char* source);
};

#endif
