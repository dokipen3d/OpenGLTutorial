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
        auto window = glfwCreateWindow(1920, 960, "Chapter 13 - Element Buffers", nullptr, nullptr);

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
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                              false);
    }

    auto program = []() -> GLuint {
        const char* vertexShaderSource = R"(
            #version 460 core
            layout (location = 0) in vec3 position;
            layout (location = 1) in vec3 normal;

            out vec3 vertex_colour;

            uniform mat4 MVP;
            uniform float switcher;

            vec3 remappedColour = (normal + vec3(1.f)) / 2.f;


            void main(){
                vertex_colour = mix(normal, remappedColour, switcher);;
                gl_Position = MVP * vec4(position, 1.0f);
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 460 core

            in vec3 vertex_colour;
            out vec4 finalColor;


            void main() {
                finalColor = vec4(vertex_colour, 1.0);
            }
        )";

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
    }();
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
    auto createBufferAndVao = [&program](const std::vector<vertex3D>& vertices,
                                         const std::vector<int>& indices) -> GLuint {
        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, vertices.size() * sizeof(vertex3D), vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "position"), /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "normal"), /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, normal));
        glEnableVertexArrayAttrib(vao, 1);

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0,
                                  /*stride*/ sizeof(vertex3D));

        // NEW! element buffer
        if (indices.size() > 0) {
            GLuint elemementBufferObject;
            glCreateBuffers(1, &bufferObject);
            glCreateBuffers(1, &elemementBufferObject);
            glNamedBufferStorage(elemementBufferObject, indices.size() * sizeof(GLuint),
                                 indices.data(), GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
            glVertexArrayElementBuffer(vao, elemementBufferObject);
        }
        return vao;
    };

    auto backGroundVao = createBufferAndVao(backGroundVertices, {});
    auto meshVao = createBufferAndVao(meshData.vertices, meshData.indices);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    std::array<GLfloat, 4> clearColour{0.f, 0.f, 0.f, 1.f};
    GLfloat clearDepth{1.0f};

    glm::mat4 model = glm::mat4(1.0f);

    glm::mat4 ortho = glm::ortho(-1.f, 1.f, -1.f, 1.f, 1.f, -1.f);
    glm::mat4 projection;

    projection = glm::perspective(glm::radians(65.0f), 1280.f / 640.f, 0.1f, 100.0f);

    glm::mat4 mvp;

    int mvpLocation = glGetUniformLocation(program, "MVP");
    int remapUniformLocation = glGetUniformLocation(program, "switcher");

    glUseProgram(program);

    while (!glfwWindowShouldClose(window)) {
        auto currentTime = duration<float>(system_clock::now() - startTime).count();

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        // bg
        glBindVertexArray(backGroundVao);
        glProgramUniformMatrix4fv(program, mvpLocation, 1, GL_FALSE, glm::value_ptr(ortho));
        glProgramUniform1f(program, remapUniformLocation, 0);
        glDrawArrays(GL_TRIANGLES, 0, (gl::GLsizei)backGroundVertices.size());

        // mesh
        glBindVertexArray(meshVao);

        glm::mat4 view =
            glm::lookAt(glm::vec3(std::sin(currentTime * 0.5f) * 2, ((std::sin(currentTime * 0.32f) + 1.0f) / 2.0f) * 2,
                                  std::cos(currentTime * 0.5f) * 2), // Camera is at (4,3,3), in World Space
                        glm::vec3(0, .4, 0),                         // and looks at the origin
                        glm::vec3(0, 1, 0)                           // Head is up (set to 0,-1,0 to look upside-down)
            );

        mvp = projection * view * model;
        glProgramUniformMatrix4fv(program, mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
        glProgramUniform1f(program, remapUniformLocation, 1);

        glDrawElements(GL_TRIANGLES, (gl::GLsizei)meshData.indices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}
