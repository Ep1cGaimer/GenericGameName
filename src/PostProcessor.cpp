#include "PostProcessor.h"
#include <iostream>
#include <vector>

// ---- Fullscreen quad vertex shader ----
static const char* quadVertSrc = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main() {
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)glsl";

// ---- Combined post-process fragment shader ----
// All 4 layers in one pass: edges, hatching, boil, paper
static const char* postFragSrc = R"glsl(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform vec2 texelSize;
uniform float uTime;
uniform float uBoilFPS;

// ---- Noise / Hash functions ----
float hash21(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float hash11(float p) {
    return fract(sin(p * 127.1) * 43758.5453);
}

// Simple value noise
float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Fractal noise for paper texture
float paperNoise(vec2 uv) {
    float n = 0.0;
    n += 0.5    * valueNoise(uv * 8.0);
    n += 0.25   * valueNoise(uv * 16.0);
    n += 0.125  * valueNoise(uv * 32.0);
    n += 0.0625 * valueNoise(uv * 64.0);
    return n;
}

// ---- Sobel Edge Detection on depth ----
float sobelEdge(vec2 uv) {
    float tl = texture(sceneDepth, uv + vec2(-1, -1) * texelSize).r;
    float t  = texture(sceneDepth, uv + vec2( 0, -1) * texelSize).r;
    float tr = texture(sceneDepth, uv + vec2( 1, -1) * texelSize).r;
    float l  = texture(sceneDepth, uv + vec2(-1,  0) * texelSize).r;
    float r  = texture(sceneDepth, uv + vec2( 1,  0) * texelSize).r;
    float bl = texture(sceneDepth, uv + vec2(-1,  1) * texelSize).r;
    float b  = texture(sceneDepth, uv + vec2( 0,  1) * texelSize).r;
    float br = texture(sceneDepth, uv + vec2( 1,  1) * texelSize).r;

    float gx = -tl - 2.0*l - bl + tr + 2.0*r + br;
    float gy = -tl - 2.0*t - tr + bl + 2.0*b + br;

    return sqrt(gx*gx + gy*gy);
}

void main() {
    // ---- Step 0: Boiling UV wobble (stepped time) ----
    float steppedTime = floor(uTime * uBoilFPS);
    vec2 noiseInput = gl_FragCoord.xy * 0.05 + steppedTime * 7.13;
    vec2 boilOffset = vec2(
        hash21(noiseInput) - 0.5,
        hash21(noiseInput + vec2(37.0, 91.0)) - 0.5
    ) * 0.0018;

    vec2 uv = TexCoords + boilOffset;

    // ---- Step 1: Sample scene ----
    vec3 scene = texture(sceneColor, uv).rgb;

    // ---- Step 2: Edge detection ----
    float edge = sobelEdge(uv);
    // Linearize the edge response — depth is non-linear
    float edgeMask = smoothstep(0.001, 0.004, edge);

    // Also add a second edge pass with wider kernel for thicker outlines
    float edgeWide = sobelEdge(uv * 0.999);
    edgeMask = max(edgeMask, smoothstep(0.0008, 0.003, edgeWide) * 0.6);

    // ---- Step 3: Cross-hatching based on luminance ----
    float lum = dot(scene, vec3(0.2126, 0.7152, 0.0722));

    // Hatching coordinates with slight boil
    vec2 hatchUV = (gl_FragCoord.xy + boilOffset * 400.0) / 6.0;

    float hatch = 1.0;

    // Layer 1: light diagonal strokes (45°)
    if (lum < 0.85) {
        float line1 = abs(fract(hatchUV.x + hatchUV.y) - 0.5) * 2.0;
        hatch = min(hatch, smoothstep(0.0, 0.35, line1) * 0.85 + 0.15);
    }

    // Layer 2: cross-hatch (135°)
    if (lum < 0.55) {
        float line2 = abs(fract(hatchUV.x - hatchUV.y) - 0.5) * 2.0;
        float h2 = smoothstep(0.0, 0.3, line2) * 0.7 + 0.3;
        hatch = min(hatch, h2);
    }

    // Layer 3: vertical strokes
    if (lum < 0.3) {
        float line3 = abs(fract(hatchUV.x * 0.7) - 0.5) * 2.0;
        float h3 = smoothstep(0.0, 0.25, line3) * 0.5 + 0.15;
        hatch = min(hatch, h3);
    }

    // Layer 4: near-total fill
    if (lum < 0.1) {
        hatch = 0.08;
    }

    // Blend between original scene colors and hatching
    // Use scene color tinted toward warm paper for hatched areas
    vec3 warmTint = vec3(0.92, 0.87, 0.78);
    vec3 hatched = warmTint * hatch;

    // Mix: bright areas keep some scene color, dark areas go full hatch
    float hatchBlend = smoothstep(0.0, 0.8, 1.0 - lum);
    vec3 result = mix(scene * 0.9 + warmTint * 0.1, hatched, hatchBlend * 0.7);

    // ---- Step 4: Apply edge outlines ----
    // Edges drawn in dark pencil color with slight variation
    vec3 pencilColor = vec3(0.08, 0.06, 0.05);
    result = mix(result, pencilColor, edgeMask * 0.9);

    // ---- Step 5: Paper texture overlay ----
    vec2 paperUV = gl_FragCoord.xy / 200.0;
    // Add steppedTime so paper grain subtly shifts with boil
    float paper = paperNoise(paperUV + steppedTime * 0.01);
    paper = 0.7 + paper * 0.3; // remap to 0.7–1.0 range
    result *= paper;

    // ---- Step 6: Slight vignette for extra unease ----
    vec2 vigUV = TexCoords * 2.0 - 1.0;
    float vig = 1.0 - dot(vigUV * 0.5, vigUV * 0.5);
    vig = clamp(vig, 0.0, 1.0);
    result *= vig;

    FragColor = vec4(result, 1.0);
}
)glsl";

// ---- Constructor / Destructor ----

PostProcessor::PostProcessor()
    : width(0), height(0), sceneFBO(0), sceneColorTex(0), sceneDepthTex(0),
      quadVAO(0), quadVBO(0), postShader(0) {}

PostProcessor::~PostProcessor() {
    destroyFBO();
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (postShader) glDeleteProgram(postShader);
}

void PostProcessor::init(int screenWidth, int screenHeight) {
    width = screenWidth;
    height = screenHeight;

    postShader = createShaderProgram(quadVertSrc, postFragSrc);
    createScreenQuad();
    createFBO();

    initialized = true;
    std::cout << "PostProcessor initialized (" << width << "x" << height << ")" << std::endl;
}

void PostProcessor::resize(int screenWidth, int screenHeight) {
    if (screenWidth == width && screenHeight == height) return;
    width = screenWidth;
    height = screenHeight;
    destroyFBO();
    createFBO();
}

void PostProcessor::createFBO() {
    // Create FBO
    glGenFramebuffers(1, &sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

    // Color attachment
    glGenTextures(1, &sceneColorTex);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTex, 0);

    // Depth attachment (as texture so we can read it in the shader)
    glGenTextures(1, &sceneDepthTex);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTex, 0);

    // Check completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR: Scene FBO is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::destroyFBO() {
    if (sceneFBO) { glDeleteFramebuffers(1, &sceneFBO); sceneFBO = 0; }
    if (sceneColorTex) { glDeleteTextures(1, &sceneColorTex); sceneColorTex = 0; }
    if (sceneDepthTex) { glDeleteTextures(1, &sceneDepthTex); sceneDepthTex = 0; }
}

void PostProcessor::createScreenQuad() {
    float quadVertices[] = {
        // pos        // texcoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void PostProcessor::beginSceneRender() {
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glViewport(0, 0, width, height);
    glClearColor(0.04f, 0.04f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void PostProcessor::endSceneRender() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::applyEffects(float time, float boilFPS) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(postShader);

    // Bind scene color to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    glUniform1i(glGetUniformLocation(postShader, "sceneColor"), 0);

    // Bind scene depth to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    glUniform1i(glGetUniformLocation(postShader, "sceneDepth"), 1);

    // Uniforms
    glUniform2f(glGetUniformLocation(postShader, "texelSize"), 1.0f / width, 1.0f / height);
    glUniform1f(glGetUniformLocation(postShader, "uTime"), time);
    glUniform1f(glGetUniformLocation(postShader, "uBoilFPS"), boilFPS);

    // Draw fullscreen quad
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

// ---- Shader utils ----

unsigned int PostProcessor::compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> msg(length);
        glGetShaderInfoLog(id, length, &length, msg.data());
        std::cout << "PostProcessor shader compile error (" 
                  << (type == GL_VERTEX_SHADER ? "vert" : "frag") << "):" << std::endl;
        std::cout << msg.data() << std::endl;
        glDeleteShader(id);
        return 0;
    }
    return id;
}

unsigned int PostProcessor::createShaderProgram(const char* vert, const char* frag) {
    unsigned int prog = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vert);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, frag);

    if (vs == 0 || fs == 0) {
        std::cout << "PostProcessor shader compilation failed!" << std::endl;
        return 0;
    }

    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    int linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        int length;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> msg(length);
        glGetProgramInfoLog(prog, length, &length, msg.data());
        std::cout << "PostProcessor shader link error: " << msg.data() << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}
