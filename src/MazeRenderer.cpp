#include "MazeRenderer.h"
#include <glad/gl.h>
#include <iostream>
#include <cmath>

// Each maze cell becomes a 3x3 tile block:
//   walls = solid tiles, passages = open tiles
// Total tile map = (2*mazeW+1) x (2*mazeH+1)
static const float TILE_SIZE   = 1.0f;   // each tile is 1x1 world unit
static const float WALL_HEIGHT = 2.4f;   // low oppressive ceiling

// ---- Shaders ----

static const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)glsl";

static const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform float fogNear;
uniform float fogFar;

void main() {
    // Overhead fluorescent light that follows the player
    vec3 lightPos = viewPos + vec3(0.0, 0.8, 0.0);

    // Ambient — constant sickly glow
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Attenuation
    float dist_to_light = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.07 * dist_to_light + 0.017 * dist_to_light * dist_to_light);

    vec3 result = (ambient + diffuse * attenuation) * objectColor;

    // Fog — relative to camera position
    float dist = length(FragPos - viewPos);
    float fogFactor = clamp((fogFar - dist) / (fogFar - fogNear), 0.0, 1.0);
    vec3 fogColor = vec3(0.04, 0.04, 0.03);

    FragColor = vec4(mix(fogColor, result, fogFactor), 1.0);
}
)glsl";

// ---- Constructor / Destructor ----

MazeRenderer::MazeRenderer()
    : wallVAO(0), wallVBO(0), floorVAO(0), floorVBO(0),
      ceilingVAO(0), ceilingVBO(0), shaderProgram(0),
      wallVertexCount(0), floorVertexCount(0), ceilingVertexCount(0),
      shaderBuilt(false) {
}

MazeRenderer::~MazeRenderer() {
    cleanup();
}

void MazeRenderer::initShader() {
    if (shaderBuilt) return;
    shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    shaderBuilt = true;
}

// ---- Tile Map Approach ----
// Convert the cell-based maze into a 2D boolean grid where:
//   tileMap[tx][ty] = true  → solid (wall)
//   tileMap[tx][ty] = false → open (walkable)
//
// The tile map is (2*W+1) x (2*H+1):
//   Cell (cx,cy) interior   → tile (2*cx+1, 2*cy+1)  → always OPEN
//   Corner pillars          → tile (2*cx,   2*cy)     → always SOLID
//   North wall of (cx,cy)   → tile (2*cx+1, 2*cy)     → solid if wall[NORTH]
//   West wall of (cx,cy)    → tile (2*cx,   2*cy+1)   → solid if wall[WEST]
//   East boundary           → tile (2*W,    2*cy+1)   → solid if wall[EAST] of last col
//   South boundary          → tile (2*cx+1, 2*H)      → solid if wall[SOUTH] of last row

void MazeRenderer::buildMesh(const MazeGenerator& maze) {
    initShader();
    cleanup();

    int tw = 2 * maze.width + 1;
    int th = 2 * maze.height + 1;
    std::vector<std::vector<bool>> tileMap(tw, std::vector<bool>(th, true)); // default solid

    // Mark cell interiors as open
    for (int cx = 0; cx < maze.width; cx++) {
        for (int cy = 0; cy < maze.height; cy++) {
            tileMap[2 * cx + 1][2 * cy + 1] = false; // cell interior

            // North wall of cell (cx,cy) → tile between cell row cy and cy-1
            if (!maze.grid[cx][cy].walls[NORTH]) {
                tileMap[2 * cx + 1][2 * cy] = false;
            }
            // West wall
            if (!maze.grid[cx][cy].walls[WEST]) {
                tileMap[2 * cx][2 * cy + 1] = false;
            }
            // South wall (only mark from the cell's perspective for the bottom boundary)
            if (cy == maze.height - 1 && !maze.grid[cx][cy].walls[SOUTH]) {
                tileMap[2 * cx + 1][2 * cy + 2] = false;
            }
            // East wall (only mark for the right boundary)
            if (cx == maze.width - 1 && !maze.grid[cx][cy].walls[EAST]) {
                tileMap[2 * cx + 2][2 * cy + 1] = false;
            }
        }
    }

    // Now generate geometry from the tile map
    std::vector<float> wallVerts;
    std::vector<float> floorVerts;
    std::vector<float> ceilingVerts;

    for (int tx = 0; tx < tw; tx++) {
        for (int ty = 0; ty < th; ty++) {
            float x0 = tx * TILE_SIZE;
            float z0 = ty * TILE_SIZE;
            float x1 = x0 + TILE_SIZE;
            float z1 = z0 + TILE_SIZE;

            if (tileMap[tx][ty]) {
                // SOLID tile — render exposed wall faces only where adjacent tile is open
                // This gives walls real thickness

                // Check -X neighbor
                if (tx == 0 || !tileMap[tx - 1][ty]) {
                    addQuad(wallVerts,
                        glm::vec3(x0, 0, z1), glm::vec3(x0, 0, z0),
                        glm::vec3(x0, WALL_HEIGHT, z0), glm::vec3(x0, WALL_HEIGHT, z1),
                        glm::vec3(-1, 0, 0));
                }
                // Check +X neighbor
                if (tx == tw - 1 || !tileMap[tx + 1][ty]) {
                    addQuad(wallVerts,
                        glm::vec3(x1, 0, z0), glm::vec3(x1, 0, z1),
                        glm::vec3(x1, WALL_HEIGHT, z1), glm::vec3(x1, WALL_HEIGHT, z0),
                        glm::vec3(1, 0, 0));
                }
                // Check -Z neighbor
                if (ty == 0 || !tileMap[tx][ty - 1]) {
                    addQuad(wallVerts,
                        glm::vec3(x0, 0, z0), glm::vec3(x1, 0, z0),
                        glm::vec3(x1, WALL_HEIGHT, z0), glm::vec3(x0, WALL_HEIGHT, z0),
                        glm::vec3(0, 0, -1));
                }
                // Check +Z neighbor
                if (ty == th - 1 || !tileMap[tx][ty + 1]) {
                    addQuad(wallVerts,
                        glm::vec3(x1, 0, z1), glm::vec3(x0, 0, z1),
                        glm::vec3(x0, WALL_HEIGHT, z1), glm::vec3(x1, WALL_HEIGHT, z1),
                        glm::vec3(0, 0, 1));
                }

                // Top of the wall (visible if player can somehow look down on it — unlikely but correct)
                // Skip for performance, ceiling covers it anyway

            } else {
                // OPEN tile — render floor and ceiling
                addQuad(floorVerts,
                    glm::vec3(x0, 0, z0), glm::vec3(x1, 0, z0),
                    glm::vec3(x1, 0, z1), glm::vec3(x0, 0, z1),
                    glm::vec3(0, 1, 0));

                addQuad(ceilingVerts,
                    glm::vec3(x0, WALL_HEIGHT, z1), glm::vec3(x1, WALL_HEIGHT, z1),
                    glm::vec3(x1, WALL_HEIGHT, z0), glm::vec3(x0, WALL_HEIGHT, z0),
                    glm::vec3(0, -1, 0));
            }
        }
    }

    // Upload Walls
    glGenVertexArrays(1, &wallVAO);
    glGenBuffers(1, &wallVBO);
    glBindVertexArray(wallVAO);
    glBindBuffer(GL_ARRAY_BUFFER, wallVBO);
    glBufferData(GL_ARRAY_BUFFER, wallVerts.size() * sizeof(float), wallVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    wallVertexCount = wallVerts.size() / 6;

    // Upload Floor
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, floorVerts.size() * sizeof(float), floorVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    floorVertexCount = floorVerts.size() / 6;

    // Upload Ceiling
    glGenVertexArrays(1, &ceilingVAO);
    glGenBuffers(1, &ceilingVBO);
    glBindVertexArray(ceilingVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ceilingVBO);
    glBufferData(GL_ARRAY_BUFFER, ceilingVerts.size() * sizeof(float), ceilingVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    ceilingVertexCount = ceilingVerts.size() / 6;

    glBindVertexArray(0);

    std::cout << "Tile map: " << tw << "x" << th << " tiles" << std::endl;
    std::cout << "Mesh built: " << wallVertexCount << " wall, "
              << floorVertexCount << " floor, "
              << ceilingVertexCount << " ceiling verts" << std::endl;
}

void MazeRenderer::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) {
    glUseProgram(shaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);

    // Light follows the camera
    glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 0.95f, 0.75f);

    // Fog
    glUniform1f(glGetUniformLocation(shaderProgram, "fogNear"), 4.0f);
    glUniform1f(glGetUniformLocation(shaderProgram, "fogFar"), 16.0f);

    // Walls (Sickly Yellowish Wallpaper)
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.78f, 0.72f, 0.52f);
    glBindVertexArray(wallVAO);
    glDrawArrays(GL_TRIANGLES, 0, wallVertexCount);

    // Floor (Dirty Brownish-Yellow Carpet)
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.45f, 0.38f, 0.25f);
    glBindVertexArray(floorVAO);
    glDrawArrays(GL_TRIANGLES, 0, floorVertexCount);

    // Ceiling (Stained Off-White Panels)
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.72f, 0.70f, 0.62f);
    glBindVertexArray(ceilingVAO);
    glDrawArrays(GL_TRIANGLES, 0, ceilingVertexCount);

    glBindVertexArray(0);
}

void MazeRenderer::cleanup() {
    if (wallVAO) { glDeleteVertexArrays(1, &wallVAO); wallVAO = 0; }
    if (wallVBO) { glDeleteBuffers(1, &wallVBO); wallVBO = 0; }
    if (floorVAO) { glDeleteVertexArrays(1, &floorVAO); floorVAO = 0; }
    if (floorVBO) { glDeleteBuffers(1, &floorVBO); floorVBO = 0; }
    if (ceilingVAO) { glDeleteVertexArrays(1, &ceilingVAO); ceilingVAO = 0; }
    if (ceilingVBO) { glDeleteBuffers(1, &ceilingVBO); ceilingVBO = 0; }
}

void MazeRenderer::addQuad(std::vector<float>& vertices, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 p4, glm::vec3 normal) {
    // Triangle 1: p1 p2 p3
    vertices.push_back(p1.x); vertices.push_back(p1.y); vertices.push_back(p1.z);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    vertices.push_back(p2.x); vertices.push_back(p2.y); vertices.push_back(p2.z);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    vertices.push_back(p3.x); vertices.push_back(p3.y); vertices.push_back(p3.z);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);

    // Triangle 2: p1 p3 p4
    vertices.push_back(p1.x); vertices.push_back(p1.y); vertices.push_back(p1.z);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    vertices.push_back(p3.x); vertices.push_back(p3.y); vertices.push_back(p3.z);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
    vertices.push_back(p4.x); vertices.push_back(p4.y); vertices.push_back(p4.z);
    vertices.push_back(normal.x); vertices.push_back(normal.y); vertices.push_back(normal.z);
}

unsigned int MazeRenderer::compileShader(unsigned int type, const char* source) {
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
        std::cout << "Failed to compile " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader!" << std::endl;
        std::cout << message.data() << std::endl;
        glDeleteShader(id);
        return 0;
    }
    return id;
}

unsigned int MazeRenderer::createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    if (vs == 0 || fs == 0) {
        std::cout << "Shader compilation failed!" << std::endl;
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        int length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(length);
        glGetProgramInfoLog(program, length, &length, message.data());
        std::cout << "Shader link error: " << message.data() << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}
