#include "error_handling.hpp"
#include <array>
#include <chrono>     // current time
#include <cmath>      // sin & cos
#include <cstdlib>    // for std::exit()
#include <fmt/core.h> // for fmt::print(). implements c++20 std::format
#include <unordered_map>

// this is really important to make sure that glbindings does not clash with
// glfw's opengl includes. otherwise we get ambigous overloads.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>

#include <glbinding-aux/debug.h>

#include "glm/glm.hpp"

using namespace gl;
using namespace std::chrono;

int main() {

    auto startTime = system_clock::now();

    const auto windowPtr = []() {
        if (!glfwInit()) {
            fmt::print("glfw didnt initialize!\n");
            std::exit(EXIT_FAILURE);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

        auto windowPtr = glfwCreateWindow(1280, 720, "Chapter 8 - Multiple Vertex Array Objects", nullptr, nullptr);

        if (!windowPtr) {
            fmt::print("window doesn't exist\n");
            glfwTerminate();
            std::exit(EXIT_FAILURE);
        }
        glfwSetWindowPos(windowPtr, 520, 180);

        glfwMakeContextCurrent(windowPtr);
        glbinding::initialize(glfwGetProcAddress, false);
        return windowPtr;
    }();

    // debugging
    {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(errorHandler::MessageCallback, 0);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                              false);
    }

    auto createProgram = [](const char* vertexShaderSource, const char* fragmentShaderSource) -> GLuint {
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

    auto program = createProgram(R"(
        #version 450 core
        layout (location = 0) in vec3 position;
        layout (location = 1) in vec3 colours;

        out vec3 vertex_colour;

        void main(){
            vertex_colour = colours;
            gl_Position = vec4(position, 1.0f);
        }
    )",
                                 R"(
        #version 450 core

        in vec3 vertex_colour;
        out vec4 finalColor;

        void main() {
            finalColor = vec4(  vertex_colour.x,
                                vertex_colour.y,
                                vertex_colour.z,
                                1.0);
        }
    )");

    struct vertex3D {
        glm::vec3 position;
        glm::vec3 colour;
    };

    // clang-format off
    // interleaved data
    // make 3d to see clipping
    const std::array<vertex3D, 3> backGroundVertices {{
        //   position   |     colour
        {{-1.f, -1.f, 0.f},  {0.12f, 0.14f, 0.16f}},
        {{ 3.f, -1.f, 0.f},  {0.12f, 0.14f, 0.16f}},
        {{-1.f,  3.f, 0.f},  {0.80f, 0.80f, 0.82f}}
    }};

    const std::array<vertex3D, 3> foregroundVertices {{
        //   position   |     colour
        {{-0.5f, -0.7f,  0.01f},  {1.f, 0.f, 0.f}},
        {{0.5f, -0.7f,  -0.01f},  {0.f, 1.f, 0.f}},
        {{0.0f, 0.6888f, 0.01f}, {0.f, 0.f, 1.f}}
    }};
    // clang-format on

    // buffers
    auto createBufferAndVao = [&program](const std::array<vertex3D, 3>& vertices) -> GLuint {
        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, vertices.size() * sizeof(vertex3D), vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "position"),
                                   /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "colours"),
                                   /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, colour));
        glEnableVertexArrayAttrib(vao, 1);

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0,
                                  /*stride*/ sizeof(vertex3D));

        return vao;
    };

    auto backGroundVao = createBufferAndVao(backGroundVertices);
    auto foreGroundVao = createBufferAndVao(foregroundVertices);

    std::array<GLfloat, 4> clearColour{0.f, 0.f, 0.f, 0.f};
    std::array<GLfloat, 1> clearDepth{1.0};

    glUseProgram(program);

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(windowPtr)) {

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glClearBufferfv(GL_DEPTH, 0, clearDepth.data());

        // draw bg
        glBindVertexArray(backGroundVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // draw fg
        glBindVertexArray(foreGroundVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(windowPtr);
        glfwPollEvents();
    }

    glfwTerminate();
}
