#include "ParticleSystem.h"
#include <iostream>
#include <cstdlib>

static const char* particleVert = R"glsl(
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>
out vec2 TexCoords;
out vec4 ParticleColor;

uniform mat4 projection;
uniform mat4 view;
uniform vec3 offset;
uniform vec4 color;
uniform float scale;

void main() {
    TexCoords = vertex.zw;
    ParticleColor = color;

    // Billboard magic - extract right and up vectors from view matrix
    vec3 cameraRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 cameraUp = vec3(view[0][1], view[1][1], view[2][1]);
    
    vec3 vertexPos = offset 
        + cameraRight * vertex.x * scale 
        + cameraUp * vertex.y * scale;

    gl_Position = projection * view * vec4(vertexPos, 1.0);
}
)glsl";

static const char* particleFrag = R"glsl(
#version 330 core
in vec2 TexCoords;
in vec4 ParticleColor;
out vec4 FragColor;

uniform sampler2D sprite;

void main() {
    FragColor = texture(sprite, TexCoords) * ParticleColor;
}
)glsl";

ParticleSystem::ParticleSystem(unsigned int amount) : amount(amount) {
    this->init();
}

ParticleSystem::~ParticleSystem() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
}

void ParticleSystem::init() {
    unsigned int vs = compileShader(GL_VERTEX_SHADER, particleVert);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, particleFrag);
    
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // VBO data (quad)
    float particle_quad[] = {
        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f, 0.0f,

        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(particle_quad), particle_quad, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    for (unsigned int i = 0; i < this->amount; ++i)
        this->particles.push_back(Particle());
}

unsigned int ParticleSystem::firstUnusedParticle() {
    for (unsigned int i = 0; i < amount; ++i) {
        if (particles[i].Life <= 0.0f) {
            return i;
        }
    }
    return 0;
}

void ParticleSystem::respawnParticle(Particle &particle, glm::vec3 offset) {
    float rColor = 0.5f + ((rand() % 100) / 100.0f);
    particle.Position = offset + glm::vec3(((rand() % 100) - 50) / 100.0f, ((rand() % 100) - 50) / 100.0f, ((rand() % 100) - 50) / 100.0f);
    particle.Color = glm::vec4(rColor, rColor, rColor, 1.0f);
    particle.Life = 1.0f;
    particle.Velocity = glm::vec3(0.0f, 0.2f, 0.0f);
}

void ParticleSystem::Update(float dt, unsigned int newParticles, glm::vec3 offset) {
    for (unsigned int i = 0; i < newParticles; ++i) {
        int unusedParticle = firstUnusedParticle();
        respawnParticle(particles[unusedParticle], offset);
    }
    for (unsigned int i = 0; i < amount; ++i) {
        Particle &p = particles[i];
        p.Life -= dt;
        if (p.Life > 0.0f) {
            p.Position -= p.Velocity * dt;
            p.Color.a -= dt * 2.5f;
        }
    }
}

void ParticleSystem::Draw(const glm::mat4& view, const glm::mat4& projection, GLuint textureID) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE); // don't write to depth buffer

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "sprite"), 0);

    for (Particle particle : this->particles) {
        if (particle.Life > 0.0f) {
            glUniform3f(glGetUniformLocation(shaderProgram, "offset"), particle.Position.x, particle.Position.y, particle.Position.z);
            glUniform4f(glGetUniformLocation(shaderProgram, "color"), particle.Color.r, particle.Color.g, particle.Color.b, particle.Color.a);
            glUniform1f(glGetUniformLocation(shaderProgram, "scale"), 0.5f);
            
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

unsigned int ParticleSystem::compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(length);
        glGetShaderInfoLog(id, length, &length, message.data());
        std::cout << "Failed to compile particle shader: " << message.data() << std::endl;
        glDeleteShader(id);
        return 0;
    }
    return id;
}
