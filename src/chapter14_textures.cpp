#include "error_handling.hpp"
#include "obj_loader.hpp"

#include <array>
#include <chrono>     // current time
#include <cmath>      // sin & cos
#include <cstdlib>    // for std::exit()
#include <fmt/core.h> // for fmt::print(). implements c++20 std::format

// this is really important to make sure that glbindings does not clash with
// glfw's opengl includes. otherwise we get ambigous overloads.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>

#include <glbinding-aux/debug.h>

#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace gl;
using namespace std::chrono;

int main() {

    auto startTime = system_clock::now();

    auto window = []() {
        if (!glfwInit()) {
            fmt::print("glfw didnt initialize!\n");
            std::exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

        /* Create a windowed mode window and its OpenGL context */
        auto window =
            glfwCreateWindow(1920, 960, "Chapter 14 - Textures", nullptr, nullptr);

        if (!window) {
            fmt::print("window doesn't exist\n");
            glfwTerminate();
            std::exit(EXIT_FAILURE);
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);

        glbinding::initialize(glfwGetProcAddress, false);
        return window;
    }();

    // debugging
    {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(errorHandler::MessageCallback, 0);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, false);
    }

    auto createShaderProgram = [](const char* vertexShaderSource,
                                  const char* fragmentShaderSource) -> GLuint {
        auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);
        errorHandler::checkShader(vertexShader, "Vertex");

        auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);
        errorHandler::checkShader(fragmentShader, "Fragment");

        auto program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);

        glLinkProgram(program);
        return program;
    };

    const char* vertexShaderSource = R"(
            #version 460 core
            layout (location = 0) in vec3 position;
            layout (location = 1) in vec3 normal;
            layout (location = 2) in vec2 texCoord;

            layout (location = 0) out vec3 colour;
            layout (location = 1) out vec2 uv;

            uniform mat4 MVP;

            void main(){
                colour = normal;
                uv = texCoord;
                gl_Position = MVP * vec4(position, 1.0f);
            }
        )";

    // for bg
    const char* fragmentShaderSourceColour = R"(
            #version 460 core

            layout (location = 0) in vec3 colour;
            layout (location = 1) in vec2 uv;
            out vec4 finalColor;

            void main() {
                finalColor = vec4(colour, 1.0f);
            }
        )";

    // for texturing models
    const char* fragmentShaderSourceTexture = R"(
            #version 460 core

            layout (location = 0) in vec3 colour;
            layout (location = 1) in vec2 uv;

            out vec4 finalColor;

            uniform sampler2D Texture;

            void main() {
                finalColor = texture(Texture, uv);
            }
        )";

    auto vertexColourProgram = createShaderProgram(vertexShaderSource, fragmentShaderSourceColour);
    auto textureProgram = createShaderProgram(vertexShaderSource, fragmentShaderSourceTexture);

    // clang-format off
    const std::vector<vertex3D> backGroundVertices {{
        //   position   |           normal        |  texCoord
        {{-1.f, -1.f, 0.999999f},  {0.12f, 0.14f, 0.16f}, {0.f, 0.f}},
        {{ 3.f, -1.f, 0.999999f},  {0.12f, 0.14f, 0.16f}, {3.f, 0.f}},
        {{-1.f,  3.f, 0.999999f},  {0.80f, 0.80f, 0.82f}, {0.f, 3.f}}
    }};
    // clang-format on

    auto meshData = objLoader::readObjElements("rubberToy.obj");

    // buffers
    auto createBufferAndVao = [](const std::vector<vertex3D>& vertices,
                                 const std::vector<int>& indices, GLuint program,
                                 bool enableTexCoord = true) -> GLuint {
        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, vertices.size() * sizeof(vertex3D), vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "aPosition"),
                                   /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec3::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex3D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "aNormal"), /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex3D, normal));
        glEnableVertexArrayAttrib(vao, 1);

        if(enableTexCoord){
            glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "aTexCoord"), /*buffs idx*/ 0);
            glVertexArrayAttribFormat(vao, 2, glm::vec2::length(), GL_FLOAT, GL_FALSE,
                                    offsetof(vertex3D, texCoord));
            glEnableVertexArrayAttrib(vao, 2);
        }

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0,
                                  /*stride*/ sizeof(vertex3D));

        // NEW! element buffer
        if (indices.size() > 0) {
            GLuint elemementBufferObject;
            glCreateBuffers(1, &elemementBufferObject);
            glNamedBufferStorage(elemementBufferObject, indices.size() * sizeof(GLuint),
                                 indices.data(), GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
            glVertexArrayElementBuffer(vao, elemementBufferObject);
        }
        return vao;
    };

    auto backGroundVao = createBufferAndVao(backGroundVertices, {}, vertexColourProgram, false);
    auto meshVao = createBufferAndVao(meshData.vertices, meshData.indices, textureProgram);

    // texture
    auto textureName = []() -> GLuint {
        stbi_set_flip_vertically_on_load(true);
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("toylowres.jpg", &texWidth, &texHeight, &texChannels, 0);

        // create gl texture
        GLuint textureName;
        glCreateTextures(GL_TEXTURE_2D, 1, &textureName);

        glTextureParameteri(textureName, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(textureName, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(textureName, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(textureName, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTextureStorage2D(textureName, 1, GL_RGB8, texWidth, texHeight);
        glTextureSubImage2D(textureName, 0, 0, 0, texWidth, texHeight, GL_RGB, GL_UNSIGNED_BYTE,
                            pixels);
        glGenerateTextureMipmap(textureName);

        stbi_image_free(pixels);
        return textureName;
    }();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    std::array<GLfloat, 4> clearColour{0.f, 0.f, 0.f, 1.f};
    GLfloat clearDepth{1.0f};

    glm::mat4 model = glm::mat4(1.0f);

    glm::mat4 view;
    glm::mat4 ortho = glm::ortho(-1.f, 1.f, -1.f, 1.f, 1.f, -1.f);
    glm::mat4 projection;
    glm::mat4 mvp;

    projection = glm::perspective(glm::radians(65.0f), 1280.f / 640.f, 0.1f, 100.0f);

    int mvpLocationVertex = glGetUniformLocation(vertexColourProgram, "MVP");
    int mvpLocationTexture = glGetUniformLocation(textureProgram, "MVP");

    glBindTextureUnit(
        0,
        textureName); // bind once. we will be using texture arrays in the future. maybe bindless?

    while (!glfwWindowShouldClose(window)) {
        auto currentTime = duration<float>(system_clock::now() - startTime).count();

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        // bg
        glBindVertexArray(backGroundVao);
        glUseProgram(vertexColourProgram);

        glProgramUniformMatrix4fv(vertexColourProgram, mvpLocationVertex, 1, GL_FALSE,
                                  glm::value_ptr(ortho));

        glDrawArrays(GL_TRIANGLES, 0, backGroundVertices.size());

        // mesh
        glBindVertexArray(meshVao);
        glUseProgram(textureProgram);

        view = glm::lookAt(glm::vec3(std::sin(currentTime * 0.5f) * 2,
                                     ((std::sin(currentTime * 0.32f) + 1.0f) / 2.0f) * 2,
                                     std::cos(currentTime * 0.5f) * 2), // lookAt Vector
                           glm::vec3(0, 0.4, 0),                        // and looks at the origin
                           glm::vec3(0, 1, 0) // Head is up (set to 0,-1,0 to look upside-down)
        );

        mvp = projection * view * model;
        glProgramUniformMatrix4fv(textureProgram, mvpLocationTexture, 1, GL_FALSE,
                                  glm::value_ptr(mvp));

        glDrawElements(GL_TRIANGLES, meshData.indices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}
