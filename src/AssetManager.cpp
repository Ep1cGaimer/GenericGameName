#include "AssetManager.h"
#include <iostream>
#include <cstdlib>

static const char* propVertexShader = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)glsl";

static const char* propFragmentShader = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;
uniform vec3 viewPos;
uniform vec3 lightColor;

// Fog
uniform float fogNear;
uniform float fogFar;

void main() {
    // Basic directional / point light from camera overhead
    vec3 lightPos = viewPos + vec3(0.0, 1.5, 0.0);
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    
    float diff = max(dot(norm, lightDir), 0.0);
    float ambient = 0.3;
    
    float dist_to_light = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.05 * dist_to_light + 0.01 * dist_to_light * dist_to_light);

    vec4 texColor = texture(texture_diffuse1, TexCoords);
    if(texColor.a < 0.1) discard; // Support basic alpha cutout

    vec3 result = (ambient + diff * attenuation) * lightColor * texColor.rgb;

    // Apply fog
    float dist = length(FragPos - viewPos);
    float fogFactor = clamp((fogFar - dist) / (fogFar - fogNear), 0.0, 1.0);
    vec3 fogColor = vec3(0.04, 0.04, 0.03);

    FragColor = vec4(mix(fogColor, result, fogFactor), 1.0);
}
)glsl";

static const char* decalFragmentShader = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
uniform sampler2D diffuseMap;
uniform vec3 viewPos;

uniform float fogNear;
uniform float fogFar;

void main() {
    vec4 texColor = texture(diffuseMap, TexCoords);
    if(texColor.a < 0.1) discard;

    vec3 lightPos = viewPos + vec3(0.0, 1.5, 0.0);
    float dist_to_light = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.05 * dist_to_light + 0.01 * dist_to_light * dist_to_light);
    
    vec3 result = texColor.rgb * (0.3 + 0.7 * attenuation);

    float dist = length(FragPos - viewPos);
    float fogFactor = clamp((fogFar - dist) / (fogFar - fogNear), 0.0, 1.0);
    vec3 fogColor = vec3(0.04, 0.04, 0.03);
    FragColor = vec4(mix(fogColor, result, fogFactor), texColor.a);
}
)glsl";

AssetManager::AssetManager() : propShaderProgram(0), decalShaderProgram(0), decalVAO(0), decalVBO(0), decalInit(false) {}

void AssetManager::initPropShader() {
    if (propShaderProgram == 0) {
        propShaderProgram = createShaderProgram(propVertexShader, propFragmentShader);
    }
}

void AssetManager::initDecals() {
    if (decalInit) return;
    decalShaderProgram = createShaderProgram(propVertexShader, decalFragmentShader);

    float quadVertices[] = { // facing up
        // positions          // normals          // texture coords
        -0.5f,  0.01f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.01f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.01f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.01f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.01f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.01f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
    };

    glGenVertexArrays(1, &decalVAO);
    glGenBuffers(1, &decalVBO);
    glBindVertexArray(decalVAO);
    glBindBuffer(GL_ARRAY_BUFFER, decalVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    decalInit = true;
}

unsigned int AssetManager::compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::string message(length, '\0');
        glGetShaderInfoLog(id, length, &length, &message[0]);
        std::cout << "AssetManager Shader compilation failed: " << message << std::endl;
        glDeleteShader(id);
        return 0;
    }
    return id;
}

unsigned int AssetManager::createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLuint AssetManager::getTexture(const std::string& name, const std::string& path, bool retroFilter) {
    if (textures.find(name) != textures.end()) {
        return textures[name];
    }
    
    GLuint texID = TextureLoader::loadTexture(path, retroFilter);
    if (texID != 0) {
        textures[name] = texID;
    }
    return texID;
}

Model* AssetManager::getModel(const std::string& name, const std::string& path) {
    if (models.find(name) != models.end()) {
        return models[name].get();
    }
    
    auto model = std::make_unique<Model>(path);
    Model* ptr = model.get();
    models[name] = std::move(model);
    return ptr;
}

void AssetManager::populateFromMaze(const MazeGenerator& maze) {
    props.clear();
    decals.clear();
    torchPositions.clear();
    particleSystems.clear();

    const float TILE_SIZE = 2.0f; // from MazeRenderer logic, assuming each full "cell" occupies a 2x2 grid in world. 
    // Wait, MazeRenderer says: TILE_SIZE = 1.0f. Each cell is 2 tiles wide. The center of cell (cx, cy) is tile (2*cx+1, 2*cy+1).
    // So world position x = (2*cx + 1) * TILE_SIZE, z = (2*cy + 1) * TILE_SIZE.
    // Let's use 1.0f for TILE_SIZE to match MazeRenderer.
    const float TS = 1.0f;

    Model* torchModel = getModel("torch", "../assets/torch.glb");
    Model* doorModel = getModel("door", "../assets/Door3dModel/Door.fbx");
    Model* crate1Model = getModel("crate1", "../assets/WoodenCrate1.glb");
    Model* crate2Model = getModel("crate2", "../assets/WoodenCrate2.glb");

    GLuint bloodTex1 = getTexture("blood1", "../assets/blood_decals/60_splatter_std.png", false);
    GLuint bloodTex2 = getTexture("blood2", "../assets/blood_decals/400_massacre_std.png", false);

    for (int cx = 0; cx < maze.width; cx++) {
        for (int cy = 0; cy < maze.height; cy++) {
            float px = (2 * cx + 1.5f) * TS; // +1.5f because the tile starts at 2*cx+1 and has width 1, center is +0.5
            float pz = (2 * cy + 1.5f) * TS;

            int wallCount = 0;
            if (maze.grid[cx][cy].walls[NORTH]) wallCount++;
            if (maze.grid[cx][cy].walls[EAST]) wallCount++;
            if (maze.grid[cx][cy].walls[SOUTH]) wallCount++;
            if (maze.grid[cx][cy].walls[WEST]) wallCount++;

            // Exit door
            if (cx == maze.width - 1 && cy == maze.height - 1) {
                Transform t;
                t.position = glm::vec3(px, 0.0f, pz + 1.0f); // place roughly at edge
                t.scale = glm::vec3(0.01f); // Adjust based on fbx size
                t.rotationAxis = glm::vec3(0, 1, 0);
                t.rotationAngle = glm::radians(0.0f);
                props.push_back({doorModel, t});
            }
            
            // Random blood splatters EVERYWHERE (40% chance per floor tile)
            if (rand() % 100 < 40) {
                Transform decalTrans;
                decalTrans.position = glm::vec3(px, 0.0f, pz);
                decalTrans.scale = glm::vec3(0.8f + (rand() % 100) / 50.0f); // Scale 0.8 - 2.8
                decalTrans.rotationAxis = glm::vec3(0, 1, 0);
                decalTrans.rotationAngle = glm::radians(static_cast<float>(rand() % 360));
                decals.push_back({(rand() % 2 == 0) ? bloodTex1 : bloodTex2, decalTrans});
            }

            // Wall Decals (20% chance)
            if (maze.grid[cx][cy].walls[NORTH] && rand() % 100 < 20) {
                Transform t;
                t.position = glm::vec3(px, 1.0f + (rand()%100)/250.0f, pz - 0.49f); 
                t.scale = glm::vec3(0.8f + (rand()%100)/50.0f);
                t.rotationAxis = glm::vec3(1, 0, 0);
                t.rotationAngle = glm::radians(90.0f);
                decals.push_back({(rand() % 2 == 0) ? bloodTex1 : bloodTex2, t});
            }
            if (maze.grid[cx][cy].walls[SOUTH] && rand() % 100 < 20) {
                Transform t;
                t.position = glm::vec3(px, 1.0f + (rand()%100)/250.0f, pz + 0.49f); 
                t.scale = glm::vec3(0.8f + (rand()%100)/50.0f);
                t.rotationAxis = glm::vec3(1, 0, 0);
                t.rotationAngle = glm::radians(-90.0f);
                decals.push_back({(rand() % 2 == 0) ? bloodTex1 : bloodTex2, t});
            }
            if (maze.grid[cx][cy].walls[EAST] && rand() % 100 < 20) {
                Transform t;
                t.position = glm::vec3(px + 0.49f, 1.0f + (rand()%100)/250.0f, pz); 
                t.scale = glm::vec3(0.8f + (rand()%100)/50.0f);
                t.rotationAxis = glm::vec3(0, 0, 1);
                t.rotationAngle = glm::radians(90.0f);
                decals.push_back({(rand() % 2 == 0) ? bloodTex1 : bloodTex2, t});
            }
            if (maze.grid[cx][cy].walls[WEST] && rand() % 100 < 20) {
                Transform t;
                t.position = glm::vec3(px - 0.49f, 1.0f + (rand()%100)/250.0f, pz); 
                t.scale = glm::vec3(0.8f + (rand()%100)/50.0f);
                t.rotationAxis = glm::vec3(0, 0, 1);
                t.rotationAngle = glm::radians(-90.0f);
                decals.push_back({(rand() % 2 == 0) ? bloodTex1 : bloodTex2, t});
            }

            // Torches at dead ends
            if (wallCount >= 3) {
                Transform t;
                t.position = glm::vec3(px, 1.2f, pz); // eye level
                t.scale = glm::vec3(0.005f); 
                t.rotationAxis = glm::vec3(0, 1, 0);
                t.rotationAngle = 0.0f;
                props.push_back({torchModel, t});

                // We moved blood splatters globally, so we skip adding them here
                
                torchPositions.push_back(glm::vec3(px, 1.3f, pz)); // little bit above torch base
                particleSystems.push_back(std::make_unique<ParticleSystem>(50));
            }
            // Render crates randomly in passable areas
            else if (wallCount <= 1 && (rand() % 10) == 0) {
                Transform t;
                t.position = glm::vec3(px, 0.0f, pz);
                t.scale = glm::vec3(0.3f);
                t.rotationAxis = glm::vec3(0, 1, 0);
                t.rotationAngle = glm::radians(static_cast<float>(rand() % 360));
                if (rand() % 2 == 0)
                    props.push_back({crate1Model, t});
                else
                    props.push_back({crate2Model, t});
            }
        }
    }
}

void AssetManager::renderProps(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& viewPos) {
    initPropShader();
    glUseProgram(propShaderProgram);
    
    glUniformMatrix4fv(glGetUniformLocation(propShaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(propShaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniform3f(glGetUniformLocation(propShaderProgram, "viewPos"), viewPos.x, viewPos.y, viewPos.z);
    glUniform3f(glGetUniformLocation(propShaderProgram, "lightColor"), 1.0f, 0.95f, 0.75f);
    glUniform1f(glGetUniformLocation(propShaderProgram, "fogNear"), 8.0f);
    glUniform1f(glGetUniformLocation(propShaderProgram, "fogFar"), 30.0f);

    for (const auto& prop : props) {
        if (!prop.model) continue;

        glm::mat4 modelMat = glm::mat4(1.0f);
        modelMat = glm::translate(modelMat, prop.transform.position);
        if (prop.transform.rotationAngle != 0.0f) {
            modelMat = glm::rotate(modelMat, prop.transform.rotationAngle, prop.transform.rotationAxis);
        }
        modelMat = glm::scale(modelMat, prop.transform.scale);

        glUniformMatrix4fv(glGetUniformLocation(propShaderProgram, "model"), 1, GL_FALSE, &modelMat[0][0]);
        prop.model->Draw();
    }
}

void AssetManager::renderDecals(const glm::mat4& view, const glm::mat4& projection) {
    if (!decalInit) initDecals();
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(decalShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(decalShaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(decalShaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniform1f(glGetUniformLocation(decalShaderProgram, "fogNear"), 8.0f);
    glUniform1f(glGetUniformLocation(decalShaderProgram, "fogFar"), 30.0f);
    
    glBindVertexArray(decalVAO);
    for (const auto& decal : decals) {
        glm::mat4 modelMat = glm::mat4(1.0f);
        modelMat = glm::translate(modelMat, decal.transform.position);
        if (decal.transform.rotationAngle != 0.0f) {
            modelMat = glm::rotate(modelMat, decal.transform.rotationAngle, decal.transform.rotationAxis);
        }
        modelMat = glm::scale(modelMat, decal.transform.scale);

        glUniformMatrix4fv(glGetUniformLocation(decalShaderProgram, "model"), 1, GL_FALSE, &modelMat[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, decal.texture);
        glUniform1i(glGetUniformLocation(decalShaderProgram, "diffuseMap"), 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void AssetManager::updateAndRenderParticles(float dt, const glm::mat4& view, const glm::mat4& projection) {
    // Temporarily disabled due to making the scene too dusty/smoky
    return;
    
    GLuint smokeTex = getTexture("smoke", "../assets/smoke1.png", true);
    
    for (size_t i = 0; i < particleSystems.size(); ++i) {
        // spawn 2 particles per frame
        particleSystems[i]->Update(dt, 2, torchPositions[i]);
        particleSystems[i]->Draw(view, projection, smokeTex);
    }
}

