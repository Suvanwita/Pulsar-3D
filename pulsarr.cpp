// =============================================================================
//  Pulsar 3D — Native OpenGL 3.3 Core + Dear ImGui UI
//  Converted from: https://github.com/cometsinthesky/pulsar-3D
//
//  DEPENDENCIES (install via vcpkg or system package manager):
//    - GLFW3       (window + input)
//    - GLAD        (OpenGL function loader) — generate at glad.dav1d.de
//    - GLM         (math: vectors, matrices, quaternions)
//    - Dear ImGui  (UI overlay) — https://github.com/ocornut/imgui
//      Required ImGui files (copy into your project):
//        imgui.h / imgui.cpp
//        imgui_draw.cpp / imgui_widgets.cpp / imgui_tables.cpp
//        backends/imgui_impl_glfw.h/.cpp
//        backends/imgui_impl_opengl3.h/.cpp
//    - stb_image   (texture loading) — https://github.com/nothings/stb
//
//  BUILD (Linux / macOS):
//    g++ pulsar3d_opengl.cpp glad/src/glad.c \
//        imgui/imgui.cpp imgui/imgui_draw.cpp \
//        imgui/imgui_widgets.cpp imgui/imgui_tables.cpp \
//        imgui/backends/imgui_impl_glfw.cpp \
//        imgui/backends/imgui_impl_opengl3.cpp \
//        -o pulsar3d -lglfw -lGL -ldl -std=c++17 \
//        -Iimgui -Iimgui/backends -Iglad/include
//
//  BUILD (Windows with MinGW):
//    g++ pulsar3d_opengl.cpp glad/src/glad.c \
//        imgui/imgui.cpp imgui/imgui_draw.cpp \
//        imgui/imgui_widgets.cpp imgui/imgui_tables.cpp \
//        imgui/backends/imgui_impl_glfw.cpp \
//        imgui/backends/imgui_impl_opengl3.cpp \
//        -o pulsar3d.exe -lglfw3 -lopengl32 -lgdi32 -std=c++17 \
//        -Iimgui -Iimgui/backends -Iglad/include
//
//  TEXTURE PATHS:
//    Place map.jpg and skybox/{right,left,top,bottom,front,back}.jpg
//    in the same folder as the executable.
// =============================================================================

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <random>

// =============================================================================
//  GLSL SHADER SOURCE STRINGS
// =============================================================================

static const char* vertexShaderSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos     = vec3(model * vec4(aPos, 1.0));
    Normal      = mat3(transpose(inverse(model))) * aNormal;
    TexCoord    = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)glsl";

static const char* pulsarFragSrc = R"glsl(
#version 330 core
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform sampler2D starTexture;
uniform float ambientStrength;
uniform float specularStrength;
uniform float shininess;
uniform float emissiveStrength;
uniform float time;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main() {
    vec3 texColor = texture(starTexture, TexCoord).rgb;
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir  = normalize(viewPos - FragPos);
    vec3 halfway  = normalize(lightDir + viewDir);

    float diff = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(norm, halfway), 0.0), shininess);

    vec3 ambient = ambientStrength * texColor;
    vec3 diffuse = diff * texColor * lightColor;
    vec3 specular = specularStrength * spec * lightColor;

    // Layer a pulsing hotspot mask over the texture so the star keeps an energetic look.
    float hotspotNoise =
        sin(TexCoord.x * 34.0 + time * 1.6) *
        sin(TexCoord.y * 27.0 - time * 1.1) *
        sin((TexCoord.x + TexCoord.y) * 19.0 + time * 0.7);
    float hotspotMask = pow(max(hotspotNoise, 0.0), 5.0);
    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 2.5);
    float pulse = 0.72 + 0.28 * sin(time * 2.4);
    vec3 emissive = texColor * (0.45 + fresnel) * hotspotMask * emissiveStrength * pulse;

    vec3 color = ambient + diffuse + specular + emissive;
    FragColor = vec4(color, 1.0);
}
)glsl";

static const char* glowFragSrc = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec4 glowColor;
void main() {
    FragColor = glowColor;
}
)glsl";

static const char* lineFragSrc = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec4 lineColor;
void main() {
    FragColor = lineColor;
}
)glsl";

static const char* lineVertSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)glsl";

static const char* skyboxVertSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main() {
    TexCoords   = aPos;
    vec4 pos    = projection * view * vec4(aPos * 4500.0, 1.0);
    gl_Position = pos.xyww;
}
)glsl";

static const char* skyboxFragSrc = R"glsl(
#version 330 core
in vec3 TexCoords;
out vec4 FragColor;
uniform samplerCube skybox;
void main() {
    FragColor = texture(skybox, TexCoords);
}
)glsl";

// =============================================================================
//  UTILITY: COMPILE + LINK SHADERS
// =============================================================================
static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "Shader error:\n" << log << "\n";
    }
    return s;
}

static GLuint buildProgram(const char* vert, const char* frag) {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// =============================================================================
//  UV SPHERE GEOMETRY
// =============================================================================
struct Vertex { glm::vec3 pos, normal; glm::vec2 uv; };

static void generateSphere(float radius, int stacks, int slices,
                            std::vector<Vertex>& verts,
                            std::vector<unsigned int>& idx) {
    for (int i = 0; i <= stacks; ++i) {
        float phi = (float)i / stacks * M_PI;
        for (int j = 0; j <= slices; ++j) {
            float theta = (float)j / slices * 2.0f * M_PI;
            Vertex v;
            v.pos    = { radius * sinf(phi) * cosf(theta),
                         radius * cosf(phi),
                         radius * sinf(phi) * sinf(theta) };
            v.normal = glm::normalize(v.pos);
            v.uv     = { (float)j / slices, (float)i / stacks };
            verts.push_back(v);
        }
    }
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            int a = i * (slices+1) + j;
            idx.insert(idx.end(), {(unsigned)a, (unsigned)(a+slices+1),
                                   (unsigned)(a+1),
                                   (unsigned)(a+1), (unsigned)(a+slices+1),
                                   (unsigned)(a+slices+2)});
        }
}

static GLuint uploadSphere(const std::vector<Vertex>& v,
                           const std::vector<unsigned int>& idx,
                           GLuint& vbo, GLuint& ebo) {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(Vertex), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*4, idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,uv));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,normal));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return vao;
}

// =============================================================================
//  TEXTURE LOADER
//  Sets stbi flip=true for 2D textures (OpenGL UV origin is bottom-left).
//  loadCubemap() will reset flip=false before loading cubemap faces.
// =============================================================================
static GLuint loadTexture(const std::string& path) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);  // 2D textures need flip
    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (data) {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        std::cout << "Loaded texture: " << path
                  << " (" << w << "x" << h << ", ch=" << ch << ")\n";
    } else {
        std::cerr << "Failed to load texture: " << path << "\n";
    }
    return tex;
}

// =============================================================================
//  CUBEMAP SKYBOX LOADER
//
//  TWO BUGS fixed vs previous version:
//
//  BUG 1 — stbi_set_flip_vertically_on_load(true) was left set from
//           loadTexture(). Cubemap faces use a different coordinate convention
//           and must NOT be flipped, or each face renders upside-down.
//           Fix: call stbi_set_flip_vertically_on_load(false) at the top.
//
//  BUG 2 — A stray std::cerr "[Texture stub]..." line remained in the previous
//           version after the stb_image code was uncommented, printing garbage
//           every frame and indicating the loader was half-enabled.
//           Fix: removed entirely.
// =============================================================================
static GLuint loadCubemap(const std::vector<std::string>& faces) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    // Cubemap faces must NOT be flipped — reset flag set by loadTexture()
    stbi_set_flip_vertically_on_load(false);

    for (int i = 0; i < 6; ++i) {
        int w, h, ch;
        unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &ch, 0);
        if (data) {
            GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
            std::cout << "Loaded cubemap face: " << faces[i]
                      << " (" << w << "x" << h << ", ch=" << ch << ")\n";
        } else {
            std::cerr << "Failed to load cubemap face: " << faces[i] << "\n";
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return tex;
}

// =============================================================================
//  PARTICLE JET SYSTEM
// =============================================================================
struct ParticleData {
    float altura;
    float theta;
    float raioNorm;
    float velocidade;
};

static constexpr int   NUM_PARTICLES     = 10000;
static constexpr float ALTURA_CONE       = 2000.0f;
static constexpr float RAIO_BASE_INICIAL = 0.015f;
static constexpr float RAIO_BASE_FINAL   = 40.0f;
static constexpr float COMP_SEGMENTO     = 10.0f;
static constexpr float VEL_JATO         = 70.0f;

struct JetSystem {
    std::vector<ParticleData>  data;
    std::vector<float>         positions;
    GLuint vao = 0, vbo = 0;
    int direction = 1;

    void init(int dir, std::mt19937& rng) {
        direction = dir;
        std::uniform_real_distribution<float> randF(0.0f, 1.0f);
        data.resize(NUM_PARTICLES);
        positions.resize(NUM_PARTICLES * 6, 0.0f);
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            auto& p   = data[i];
            p.altura  = randF(rng) * ALTURA_CONE;
            p.theta   = randF(rng) * 2.0f * M_PI;
            p.raioNorm= sqrtf(randF(rng));
            p.velocidade = VEL_JATO * (0.65f + randF(rng) * 0.7f);
            fillSegment(i);
        }
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     positions.size() * sizeof(float),
                     positions.data(),
                     GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    void fillSegment(int i) {
        const auto& p = data[i];
        float t        = p.altura / ALTURA_CONE;
        float raioMax  = RAIO_BASE_INICIAL + t * (RAIO_BASE_FINAL - RAIO_BASE_INICIAL);
        float raio     = raioMax * p.raioNorm;
        float x        = raio * cosf(p.theta);
        float z        = raio * sinf(p.theta);
        float yStart   = (direction > 0) ?  p.altura : -p.altura;
        float yEnd     = yStart + COMP_SEGMENTO * direction;
        int base = i * 6;
        positions[base+0] = x; positions[base+1] = yStart; positions[base+2] = z;
        positions[base+3] = x; positions[base+4] = yEnd;   positions[base+5] = z;
    }

    void update(float deltaTime) {
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            auto& p = data[i];
            p.altura += p.velocidade * deltaTime;
            if (p.altura > ALTURA_CONE) p.altura = 0.0f;
            fillSegment(i);
        }
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        positions.size() * sizeof(float),
                        positions.data());
    }

    void draw(GLuint program, const glm::mat4& model,
              const glm::mat4& view, const glm::mat4& proj) {
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program,"model"),      1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(program,"view"),       1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program,"projection"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform4f(glGetUniformLocation(program,"lineColor"), 0.98f, 0.93f, 0.23f, 1.0f);
        glBindVertexArray(vao);
        glDrawArrays(GL_LINES, 0, NUM_PARTICLES * 2);
        glBindVertexArray(0);
    }
};

// =============================================================================
//  MAGNETIC FIELD LINES
// =============================================================================
static glm::vec3 magneticField(glm::vec3 r) {
    glm::vec3 m(0.0f, 1.0f, 0.0f);
    float rMag  = glm::length(r);
    if (rMag < 1e-5f) return glm::vec3(0.0f);
    float rDotM = glm::dot(r, m);
    float r5    = powf(rMag, 5.0f);
    float r3    = powf(rMag, 3.0f);
    return r * (3.0f * rDotM / r5) - m * (1.0f / r3);
}

struct FieldLines {
    std::vector<GLuint> vaos, vbos;
    std::vector<int>    counts;

    void build() {
        const int   NUM_LINES = 10;
        const float STEP      = 0.1f;
        const float MAX_DIST  = 1060.0f;
        const float INC_ANGLE = glm::radians(30.0f);

        for (int i = 0; i < NUM_LINES; ++i) {
            float theta = (float)i / NUM_LINES * 2.0f * M_PI;
            glm::vec3 start(
                sinf(INC_ANGLE) * cosf(theta),
                cosf(INC_ANGLE),
                sinf(INC_ANGLE) * sinf(theta)
            );
            start *= 0.5f;

            std::vector<glm::vec3> pts;
            glm::vec3 pos = start;
            for (int j = 0; j < 1061; ++j) {
                pts.push_back(pos);
                glm::vec3 B = glm::normalize(magneticField(pos));
                pos += B * STEP;
                if (glm::length(pos) > MAX_DIST) break;
            }

            GLuint vao, vbo;
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, pts.size()*sizeof(glm::vec3),
                         pts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);

            vaos.push_back(vao);
            vbos.push_back(vbo);
            counts.push_back((int)pts.size());
        }
    }

    void draw(GLuint program, const glm::mat4& model,
              const glm::mat4& view, const glm::mat4& proj) {
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program,"model"),      1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(program,"view"),       1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program,"projection"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform4f(glGetUniformLocation(program,"lineColor"), 0.49f, 0.84f, 1.0f, 0.8f);
        for (int i = 0; i < (int)vaos.size(); ++i) {
            glBindVertexArray(vaos[i]);
            glDrawArrays(GL_LINE_STRIP, 0, counts[i]);
        }
        glBindVertexArray(0);
    }
};

// =============================================================================
//  ORBIT CAMERA
// =============================================================================
struct Camera {
    float yaw   = 0.0f;
    float pitch = 0.15f;
    float dist  = 5.0f;
    float fov   = 50.0f;
    glm::vec3 target{0.0f};

    double lastX = 0, lastY = 0;
    bool   dragging = false;

    glm::mat4 view() const {
        glm::vec3 pos = target + glm::vec3(
            dist * cosf(pitch) * sinf(yaw),
            dist * sinf(pitch),
            dist * cosf(pitch) * cosf(yaw)
        );
        return glm::lookAt(pos, target, {0,1,0});
    }

    glm::vec3 position() const {
        return target + glm::vec3(
            dist * cosf(pitch) * sinf(yaw),
            dist * sinf(pitch),
            dist * cosf(pitch) * cosf(yaw)
        );
    }

    glm::mat4 projection(float aspect) const {
        return glm::perspective(glm::radians(fov), aspect, 0.1f, 9100.0f);
    }
};

// =============================================================================
//  GRID
// =============================================================================
struct Grid {
    GLuint vao = 0, vbo = 0;
    int count = 0;

    void build(float size, int divs) {
        std::vector<float> pts;
        float half = size * 0.5f;
        float step = size / divs;
        for (int i = 0; i <= divs; ++i) {
            float t = -half + i * step;
            pts.insert(pts.end(), {t, 0.0f, -half, t, 0.0f,  half});
            pts.insert(pts.end(), {-half, 0.0f, t,  half, 0.0f, t});
        }
        count = (int)pts.size() / 3;
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, pts.size()*sizeof(float), pts.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    void draw(GLuint prog, const glm::mat4& view, const glm::mat4& proj) {
        glUseProgram(prog);
        glm::mat4 m(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(prog,"model"),      1,GL_FALSE,glm::value_ptr(m));
        glUniformMatrix4fv(glGetUniformLocation(prog,"view"),       1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog,"projection"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform4f(glGetUniformLocation(prog,"lineColor"), 0.94f, 0.94f, 0.94f, 0.4f);
        glBindVertexArray(vao);
        glDrawArrays(GL_LINES, 0, count);
        glBindVertexArray(0);
    }
};

// =============================================================================
//  AXIS ARROWS
// =============================================================================
struct AxisArrows {
    GLuint vao = 0, vbo = 0;

    void build() {
        float pts[] = {
            0.0f, -4.0f, 0.0f,
            0.0f,  4.0f, 0.0f
        };
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(pts), pts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    void draw(GLuint prog, const glm::mat4& model,
              const glm::mat4& view, const glm::mat4& proj) {
        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog,"model"),      1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(prog,"view"),       1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog,"projection"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform4f(glGetUniformLocation(prog,"lineColor"), 1.0f, 0.0f, 0.0f, 1.0f);
        glBindVertexArray(vao);
        glDrawArrays(GL_LINES, 0, 2);
        glBindVertexArray(0);
    }
};

// =============================================================================
//  GLOBAL STATE
// =============================================================================
static Camera g_cam;
static bool   g_rotating    = false;
static float  g_rotSpeed    = -0.1f;
static float  g_inclination = 0.0f;
static float  g_rotY        = glm::radians(180.0f);
static bool   g_showJets    = false;
static bool   g_showField   = false;
static bool   g_showGrid    = false;
static bool   g_showAxis    = false;
static float  g_ambientStr  = 1.25f;
static float  g_specularStr = 0.65f;
static float  g_shininess   = 48.0f;
static float  g_emissiveStr = 1.35f;

static float  g_ui_rotSpeedAbs = 0.1f;
static float  g_ui_inclDegrees = 0.0f;
static float  g_ui_ambient     = 1.25f;
static float  g_ui_specular    = 0.65f;
static float  g_ui_shininess   = 48.0f;
static float  g_ui_emissive    = 1.35f;

static int g_width = 1280, g_height = 600;

// =============================================================================
//  GLFW CALLBACKS
// =============================================================================
void framebufferSizeCB(GLFWwindow*, int w, int h) {
    g_width = w; g_height = h;
    glViewport(0, 0, w, h);
}

void mouseButtonCB(GLFWwindow* w, int button, int action, int /*mods*/) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_cam.dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &g_cam.lastX, &g_cam.lastY);
    }
}

void cursorPosCB(GLFWwindow*, double x, double y) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (!g_cam.dragging) return;
    double dx = x - g_cam.lastX;
    double dy = y - g_cam.lastY;
    g_cam.lastX = x; g_cam.lastY = y;
    g_cam.yaw   -= (float)(dx * 0.005f);
    g_cam.pitch += (float)(dy * 0.005f);
    g_cam.pitch  = glm::clamp(g_cam.pitch, -1.5f, 1.5f);
}

void scrollCB(GLFWwindow*, double /*dx*/, double dy) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    g_cam.dist -= (float)dy * 0.3f;
    g_cam.dist  = glm::clamp(g_cam.dist, 0.8f, 1400.0f);
}

void keyCB(GLFWwindow* window, int key, int /*scan*/, int action, int /*mods*/) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (action != GLFW_PRESS) return;
    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        case GLFW_KEY_SPACE:
            g_rotating = !g_rotating; break;
        case GLFW_KEY_P:
            g_showJets = !g_showJets; break;
        case GLFW_KEY_M:
            g_showField = !g_showField; break;
        case GLFW_KEY_G:
            g_showGrid = !g_showGrid; break;
        case GLFW_KEY_R:
            g_showAxis = !g_showAxis; break;
        case GLFW_KEY_EQUAL:
        case GLFW_KEY_KP_ADD:
            g_ui_rotSpeedAbs = glm::clamp(g_ui_rotSpeedAbs + 0.02f, 0.01f, 0.3f);
            g_rotSpeed = -g_ui_rotSpeedAbs; break;
        case GLFW_KEY_MINUS:
        case GLFW_KEY_KP_SUBTRACT:
            g_ui_rotSpeedAbs = glm::clamp(g_ui_rotSpeedAbs - 0.02f, 0.01f, 0.3f);
            g_rotSpeed = -g_ui_rotSpeedAbs; break;
        case GLFW_KEY_UP:
            g_ui_inclDegrees = glm::clamp(g_ui_inclDegrees + 5.0f, 0.0f, 100.0f);
            g_inclination = glm::radians(g_ui_inclDegrees); break;
        case GLFW_KEY_DOWN:
            g_ui_inclDegrees = glm::clamp(g_ui_inclDegrees - 5.0f, 0.0f, 100.0f);
            g_inclination = glm::radians(g_ui_inclDegrees); break;
        default: break;
    }
}

// =============================================================================
//  IMGUI STYLE
// =============================================================================
static void applySpaceStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding   = 6.0f;
    s.FrameRounding    = 4.0f;
    s.GrabRounding     = 4.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize  = 0.0f;
    s.WindowPadding    = ImVec2(12, 10);
    s.ItemSpacing      = ImVec2(8, 6);
    s.GrabMinSize      = 10.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = ImVec4(0.06f, 0.06f, 0.10f, 0.88f);
    c[ImGuiCol_TitleBg]          = ImVec4(0.08f, 0.08f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.10f, 0.10f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBg]          = ImVec4(0.14f, 0.14f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.20f, 0.20f, 0.32f, 1.00f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.24f, 0.24f, 0.38f, 1.00f);
    c[ImGuiCol_SliderGrab]       = ImVec4(0.27f, 0.75f, 0.40f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.35f, 0.90f, 0.50f, 1.00f);
    c[ImGuiCol_Button]           = ImVec4(0.15f, 0.45f, 0.80f, 1.00f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(0.25f, 0.58f, 0.95f, 1.00f);
    c[ImGuiCol_ButtonActive]     = ImVec4(0.10f, 0.35f, 0.70f, 1.00f);
    c[ImGuiCol_CheckMark]        = ImVec4(0.27f, 0.85f, 0.45f, 1.00f);
    c[ImGuiCol_Header]           = ImVec4(0.15f, 0.15f, 0.28f, 1.00f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.22f, 0.22f, 0.38f, 1.00f);
    c[ImGuiCol_Separator]        = ImVec4(0.30f, 0.30f, 0.45f, 0.60f);
    c[ImGuiCol_Text]             = ImVec4(0.90f, 0.92f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.45f, 0.48f, 0.58f, 1.00f);
    c[ImGuiCol_Border]           = ImVec4(0.28f, 0.30f, 0.48f, 0.70f);
}

// =============================================================================
//  DRAW IMGUI PANEL
// =============================================================================
static void drawImGuiPanel() {
    ImGuiIO& io = ImGui::GetIO();
    const float panelW = 210.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelW - 10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("##controls", nullptr, flags)) {

        ImGui::TextDisabled("Speed:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##speed", &g_ui_rotSpeedAbs, 0.01f, 0.30f, ""))
            g_rotSpeed = -g_ui_rotSpeedAbs;
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Ambient Light:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##ambient", &g_ui_ambient, 0.1f, 2.5f, ""))
            g_ambientStr = g_ui_ambient;
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Specular:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##specular", &g_ui_specular, 0.0f, 1.5f, ""))
            g_specularStr = g_ui_specular;
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Shininess:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##shininess", &g_ui_shininess, 4.0f, 96.0f, "%.0f"))
            g_shininess = g_ui_shininess;
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Emissive Hotspots:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##emissive", &g_ui_emissive, 0.0f, 3.0f, ""))
            g_emissiveStr = g_ui_emissive;
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Inclination:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##incl", &g_ui_inclDegrees, 0.0f, 100.0f, "%.0f deg"))
            g_inclination = glm::radians(g_ui_inclDegrees);
        ImGui::PopItemWidth();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.68f, 1.0f));
        ImGui::SetWindowFontScale(0.75f);
        ImGui::Text("0  15  30  45  60  75  90");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Radiation Jets  [P]", &g_showJets);
        ImGui::Checkbox("Rotation Axis   [R]", &g_showAxis);
        ImGui::Checkbox("Magnetic Field  [M]", &g_showField);
        ImGui::Checkbox("Reference Grid  [G]", &g_showGrid);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float btnW = (panelW - 36.0f) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.70f, 0.15f, 0.15f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.55f, 0.10f, 0.10f, 1.00f));
        if (ImGui::Button("Reset", ImVec2(btnW, 0))) {
            g_rotY           = glm::radians(180.0f);
            g_rotating       = false;
            g_ui_rotSpeedAbs = 0.1f;
            g_rotSpeed       = -0.1f;
            g_ui_inclDegrees = 0.0f;
            g_inclination    = 0.0f;
            g_ui_ambient     = 1.25f;
            g_ambientStr     = 1.25f;
            g_ui_specular    = 0.65f;
            g_specularStr    = 0.65f;
            g_ui_shininess   = 48.0f;
            g_shininess      = 48.0f;
            g_ui_emissive    = 1.35f;
            g_emissiveStr    = 1.35f;
            g_showJets       = false;
            g_showField      = false;
            g_showGrid       = false;
            g_showAxis       = false;
            g_cam.yaw        = 0.0f;
            g_cam.pitch      = 0.15f;
            g_cam.dist       = 5.0f;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.60f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.78f, 0.35f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.45f, 0.18f, 1.00f));
        const char* playLabel = g_rotating ? "Pause [Spc]" : "Play  [Spc]";
        if (ImGui::Button(playLabel, ImVec2(btnW, 0)))
            g_rotating = !g_rotating;
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.42f, 0.55f, 1.0f));
        ImGui::SetWindowFontScale(0.80f);
        ImGui::TextWrapped("+/- Speed  Arrows Inclination\nDrag Orbit  Scroll Zoom");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

// =============================================================================
//  MAIN
// =============================================================================
int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(g_width, g_height,
                                          "Pulsar 3D — OpenGL", nullptr, nullptr);
    if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n"; return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebufferSizeCB);
    glfwSetMouseButtonCallback(window,    mouseButtonCB);
    glfwSetCursorPosCallback(window,      cursorPosCB);
    glfwSetScrollCallback(window,         scrollCB);
    glfwSetKeyCallback(window,            keyCB);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;
    applySpaceStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    GLuint pulsarProg = buildProgram(vertexShaderSrc, pulsarFragSrc);
    GLuint glowProg   = buildProgram(vertexShaderSrc, glowFragSrc);
    GLuint lineProg   = buildProgram(lineVertSrc,     lineFragSrc);
    GLuint skyboxProg = buildProgram(skyboxVertSrc,   skyboxFragSrc);

    // Pulsar sphere — loadTexture sets flip=true
    std::vector<Vertex> sv; std::vector<unsigned int> si;
    generateSphere(0.5f, 64, 64, sv, si);
    GLuint sVbo, sEbo;
    GLuint sphereVao = uploadSphere(sv, si, sVbo, sEbo);
    GLuint starTex   = loadTexture("images/map.jpg");

    // Glow sphere
    std::vector<Vertex> gv; std::vector<unsigned int> gi;
    generateSphere(0.52f, 32, 32, gv, gi);
    GLuint gVbo, gEbo;
    GLuint glowVao = uploadSphere(gv, gi, gVbo, gEbo);

    // Skybox — loadCubemap resets flip=false internally
    std::vector<std::string> skyFaces = {
        "skybox/right.jpg",  "skybox/left.jpg",
        "skybox/top.jpg",    "skybox/bottom.jpg",
        "skybox/front.jpg",  "skybox/back.jpg"
    };
    GLuint cubemapTex = loadCubemap(skyFaces);

    float skyVerts[] = {
        -1,  1, -1,  -1, -1, -1,   1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
        -1, -1,  1,  -1, -1, -1,  -1,  1, -1,  -1,  1, -1,  -1,  1,  1,  -1, -1,  1,
         1, -1, -1,   1, -1,  1,   1,  1,  1,   1,  1,  1,   1,  1, -1,   1, -1, -1,
        -1, -1,  1,  -1,  1,  1,   1,  1,  1,   1,  1,  1,   1, -1,  1,  -1, -1,  1,
        -1,  1, -1,   1,  1, -1,   1,  1,  1,   1,  1,  1,  -1,  1,  1,  -1,  1, -1,
        -1, -1, -1,  -1, -1,  1,   1, -1, -1,   1, -1, -1,  -1, -1,  1,   1, -1,  1
    };
    GLuint skyVao, skyVbo;
    glGenVertexArrays(1, &skyVao); glGenBuffers(1, &skyVbo);
    glBindVertexArray(skyVao);
    glBindBuffer(GL_ARRAY_BUFFER, skyVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyVerts), skyVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::mt19937 rng(42);
    JetSystem jet1, jet2;
    jet1.init( 1, rng);
    jet2.init(-1, rng);

    FieldLines field; field.build();
    Grid       grid;  grid.build(30.0f, 50);
    AxisArrows axis;  axis.build();

    double prevTime = glfwGetTime();

    // ==========================================================================
    //  RENDER LOOP
    // ==========================================================================
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float  dt  = (float)(now - prevTime);
        prevTime   = now;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (g_rotating)  g_rotY += g_rotSpeed * dt * 60.0f;
        if (g_showJets) { jet1.update(dt); jet2.update(dt); }

        float aspect = (float)g_width / (float)std::max(g_height, 1);
        glm::mat4 view = g_cam.view();
        glm::mat4 proj = g_cam.projection(aspect);
        glm::vec3 camPos = g_cam.position();
        glm::vec3 lightPos(3.2f, 2.3f, 2.4f);
        glm::vec3 lightColor(0.95f, 0.98f, 1.0f);

        glm::mat4 pulsarModel(1.0f);
        pulsarModel = glm::rotate(pulsarModel, g_rotY,        glm::vec3(0,1,0));
        pulsarModel = glm::rotate(pulsarModel, g_inclination, glm::vec3(0,0,1));

        glViewport(0, 0, g_width, g_height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

       // --- Skybox ----------------------------------------------------------
glDepthMask(GL_FALSE);
glDepthFunc(GL_LEQUAL); // Changed from GL_LESS to GL_LEQUAL
glUseProgram(skyboxProg);

// Remove translation from the view matrix so the skybox follows the camera
glm::mat4 skyView = glm::mat4(glm::mat3(view)); 

glUniformMatrix4fv(glGetUniformLocation(skyboxProg,"view"), 1, GL_FALSE, glm::value_ptr(skyView));
glUniformMatrix4fv(glGetUniformLocation(skyboxProg,"projection"), 1, GL_FALSE, glm::value_ptr(proj));
glUniform1i(glGetUniformLocation(skyboxProg,"skybox"), 0);

glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex);
glBindVertexArray(skyVao);
glDrawArrays(GL_TRIANGLES, 0, 36);
glBindVertexArray(0);

glDepthMask(GL_TRUE);
glDepthFunc(GL_LESS); // Reset back to default

        // --- Pulsar sphere ---------------------------------------------------
        glUseProgram(pulsarProg);
        glUniformMatrix4fv(glGetUniformLocation(pulsarProg,"model"),      1,GL_FALSE,glm::value_ptr(pulsarModel));
        glUniformMatrix4fv(glGetUniformLocation(pulsarProg,"view"),       1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(pulsarProg,"projection"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform1f(glGetUniformLocation(pulsarProg,"ambientStrength"), g_ambientStr);
        glUniform1f(glGetUniformLocation(pulsarProg,"specularStrength"), g_specularStr);
        glUniform1f(glGetUniformLocation(pulsarProg,"shininess"), g_shininess);
        glUniform1f(glGetUniformLocation(pulsarProg,"emissiveStrength"), g_emissiveStr);
        glUniform1f(glGetUniformLocation(pulsarProg,"time"), now);
        glUniform3fv(glGetUniformLocation(pulsarProg,"viewPos"), 1, glm::value_ptr(camPos));
        glUniform3fv(glGetUniformLocation(pulsarProg,"lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(pulsarProg,"lightColor"), 1, glm::value_ptr(lightColor));
        glUniform1i(glGetUniformLocation(pulsarProg,"starTexture"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, starTex);
        glBindVertexArray(sphereVao);
        glDrawElements(GL_TRIANGLES, (GLsizei)si.size(), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        // --- Glow sphere -----------------------------------------------------
        glUseProgram(glowProg);
        glUniformMatrix4fv(glGetUniformLocation(glowProg,"model"),      1,GL_FALSE,glm::value_ptr(pulsarModel));
        glUniformMatrix4fv(glGetUniformLocation(glowProg,"view"),       1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(glowProg,"projection"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform4f(glGetUniformLocation(glowProg,"glowColor"), 0.66f, 0.89f, 1.0f, 0.25f);
        glBindVertexArray(glowVao);
        glDrawElements(GL_TRIANGLES, (GLsizei)gi.size(), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (g_showJets)  { jet1.draw(lineProg, pulsarModel, view, proj);
                           jet2.draw(lineProg, pulsarModel, view, proj); }
        if (g_showField)   field.draw(lineProg, pulsarModel, view, proj);
        if (g_showGrid)    grid.draw(lineProg, view, proj);
        if (g_showAxis)    axis.draw(lineProg, pulsarModel, view, proj);

        drawImGuiPanel();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- Cleanup -------------------------------------------------------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &sphereVao); glDeleteBuffers(1, &sVbo); glDeleteBuffers(1, &sEbo);
    glDeleteVertexArrays(1, &glowVao);  glDeleteBuffers(1, &gVbo); glDeleteBuffers(1, &gEbo);
    glDeleteVertexArrays(1, &skyVao);   glDeleteBuffers(1, &skyVbo);
    glDeleteTextures(1, &starTex);
    glDeleteTextures(1, &cubemapTex);
    glDeleteProgram(pulsarProg);
    glDeleteProgram(glowProg);
    glDeleteProgram(lineProg);
    glDeleteProgram(skyboxProg);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// =============================================================================
//  KEYBOARD REFERENCE
//
//  Space           — Play / Pause rotation
//  P               — Toggle radiation jets
//  M               — Toggle magnetic field lines
//  G               — Toggle reference grid
//  R               — Toggle rotation axis arrows
//  + / =           — Increase rotation speed
//  -               — Decrease rotation speed
//  Arrow Up        — Increase pulsar inclination
//  Arrow Down      — Decrease pulsar inclination
//  Escape          — Quit
//
//  Mouse left drag — Orbit camera
//  Mouse scroll    — Zoom in / out
// =============================================================================
