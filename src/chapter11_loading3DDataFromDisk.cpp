#include "error_handling.hpp"
#include "obj_loader_simple_split_cpp.hpp"

#include <array>
#include <chrono>     // current time
#include <cmath>      // sin & cos
#include <cstdlib>    // for std::exit()
#include <fmt/core.h> // for fmt::print(). implements c++20 std::format
#include <pystring.h>
#include <unordered_map>

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

    const int width = 900;
    const int height = 900;

    auto windowPtr = [&]()  {
        if (!glfwInit()) {
            fmt::print("glfw didnt initialize!\n");
            std::exit(EXIT_FAILURE);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

        /* Create a windowed mode window and its OpenGL context */
        auto windowPtr = glfwCreateWindow(width, height,
                                          "Chapter 11 - Loading Data from Disk",
                                          nullptr, nullptr);

        if (!windowPtr) {
            fmt::print("window doesn't exist\n");
            glfwTerminate();
            std::exit(EXIT_FAILURE);
        }
        glfwSetWindowPos(windowPtr, 480, 90);

        glfwMakeContextCurrent(windowPtr);
        glbinding::initialize(glfwGetProcAddress, false);
        return windowPtr;
    }();

    // debugging
    {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(errorHandler::MessageCallback, 0);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                              false);
    }

    auto createProgram = [](const char* vertexShaderSource,
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
            #version 450 core
            layout (location = 0) in vec3 position;
            layout (location = 1) in vec3 normal;

            out vec3 vertex_colour;

            uniform mat4 projection;
            uniform float switcher;

            vec3 remappedColour = (normal + vec3(1.f)) / 2.f;

            void main(){
                vertex_colour = mix(normal, remappedColour, switcher);
                gl_Position = vec4((position * vec3(1.0f, 1.0f, -1.0f)) +
                                   (vec3(0, -0.5, 0) * switcher), 1.0f);
            }
        )";

    const char* fragmentShaderSource = R"(
            #version 450 core

            in vec3 vertex_colour;
            out vec4 finalColor;


            void main() {
                finalColor = vec4(vertex_colour, 1.0);
            }
        )";

    auto program = createProgram(vertexShaderSource, fragmentShaderSource);

    // clang-format off
    const std::vector<vertex3D> backGroundVertices {{
        //   position   |           normal        |  texCoord
        {{-1.f, -1.f, -0.999999f},  {0.12f, 0.14f, 0.16f}, {0.f, 0.f}},
        {{ 3.f, -1.f, -0.999999f},  {0.12f, 0.14f, 0.16f}, {3.f, 0.f}},
        {{-1.f,  3.f, -0.999999f},  {0.80f, 0.80f, 0.82f}, {0.f, 3.f}}
    }};
    // clang-format on

    auto meshData = objLoader::readObjSplit("rubberToy.obj");

    fmt::print("size {}", meshData.vertices.size());

    // buffers
    auto createBufferAndVao =
        [&program](const std::vector<vertex3D>& vertices) -> GLuint {
        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, vertices.size() * sizeof(vertex3D),
                             vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(
            vao, glGetAttribLocation(program, "position"), /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec3::length(), GL_FLOAT,
                                  GL_FALSE, offsetof(vertex3D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "normal"),
                                   /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT,
                                  GL_FALSE, offsetof(vertex3D, normal));
        glEnableVertexArrayAttrib(vao, 1);

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0,
                                  /*stride*/ sizeof(vertex3D));

        return vao;
    };

    auto backGroundVao = createBufferAndVao(backGroundVertices);
    auto meshVao = createBufferAndVao(meshData.vertices);

    glEnable(GL_DEPTH_TEST);

    std::array<GLfloat, 4> clearColour{0.f, 0.f, 0.f, 1.f};
    GLfloat clearDepth{1.0f};

    int remapUniformLocation = glGetUniformLocation(program, "switcher");
    glUseProgram(program);

    while (!glfwWindowShouldClose(windowPtr)) {

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        glBindVertexArray(backGroundVao);
        glProgramUniform1f(program, remapUniformLocation, 0);
        glDrawArrays(GL_TRIANGLES, 0, (gl::GLsizei)backGroundVertices.size());

        glBindVertexArray(meshVao);
        glProgramUniform1f(program, remapUniformLocation, 1);
        glDrawArrays(GL_TRIANGLES, 0, (gl::GLsizei)meshData.vertices.size());

        glfwSwapBuffers(windowPtr);
        glfwPollEvents();
    }

    glfwTerminate();
}
