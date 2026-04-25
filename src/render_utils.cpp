#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "pulsar/render_utils.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace pulsar {

const char* kVertexShaderSrc = R"glsl(
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

const char* kPulsarFragSrc = R"glsl(
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

const char* kGlowFragSrc = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec4 glowColor;
void main() {
    FragColor = glowColor;
}
)glsl";

const char* kLineFragSrc = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec4 lineColor;
void main() {
    FragColor = lineColor;
}
)glsl";

const char* kLineVertSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)glsl";

const char* kSkyboxVertSrc = R"glsl(
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

const char* kSkyboxFragSrc = R"glsl(
#version 330 core
in vec3 TexCoords;
out vec4 FragColor;
uniform samplerCube skybox;
void main() {
    FragColor = texture(skybox, TexCoords);
}
)glsl";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader error:\n" << log << "\n";
    }
    return shader;
}

GLuint buildProgram(const char* vert, const char* frag) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void generateSphere(
    float radius,
    int stacks,
    int slices,
    std::vector<Vertex>& verts,
    std::vector<unsigned int>& idx
) {
    const float pi = glm::pi<float>();
    const float tau = glm::two_pi<float>();

    for (int i = 0; i <= stacks; ++i) {
        float phi = static_cast<float>(i) / static_cast<float>(stacks) * pi;
        for (int j = 0; j <= slices; ++j) {
            float theta = static_cast<float>(j) / static_cast<float>(slices) * tau;
            Vertex v;
            v.pos = {
                radius * sinf(phi) * cosf(theta),
                radius * cosf(phi),
                radius * sinf(phi) * sinf(theta)
            };
            v.normal = glm::normalize(v.pos);
            v.uv = {
                static_cast<float>(j) / static_cast<float>(slices),
                static_cast<float>(i) / static_cast<float>(stacks)
            };
            verts.push_back(v);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int a = i * (slices + 1) + j;
            idx.insert(idx.end(), {
                static_cast<unsigned int>(a),
                static_cast<unsigned int>(a + slices + 1),
                static_cast<unsigned int>(a + 1),
                static_cast<unsigned int>(a + 1),
                static_cast<unsigned int>(a + slices + 1),
                static_cast<unsigned int>(a + slices + 2)
            });
        }
    }
}

GLuint uploadSphere(
    const std::vector<Vertex>& verts,
    const std::vector<unsigned int>& idx,
    GLuint& vbo,
    GLuint& ebo
) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return vao;
}

GLuint loadTexture(const std::string& path) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);

    int w = 0;
    int h = 0;
    int ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (data) {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        std::cout << "Loaded texture: " << path << " (" << w << "x" << h << ", ch=" << ch << ")\n";
    } else {
        std::cerr << "Failed to load texture: " << path << "\n";
    }
    return tex;
}

GLuint loadCubemap(const std::vector<std::string>& faces) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    stbi_set_flip_vertically_on_load(false);

    for (int i = 0; i < 6; ++i) {
        int w = 0;
        int h = 0;
        int ch = 0;
        unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &ch, 0);
        if (data) {
            GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                fmt,
                w,
                h,
                0,
                fmt,
                GL_UNSIGNED_BYTE,
                data
            );
            stbi_image_free(data);
            std::cout << "Loaded cubemap face: " << faces[i] << " (" << w << "x" << h << ", ch=" << ch << ")\n";
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

}  // namespace pulsar
