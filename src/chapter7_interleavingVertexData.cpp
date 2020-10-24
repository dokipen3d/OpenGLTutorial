#include "error_handling.hpp"
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
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

        auto windowPtr =
            glfwCreateWindow(1280, 720, "Chapter 7 - Interleaving Vertex Data", nullptr, nullptr);

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
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, false);
    }

    auto program = []() -> GLuint {
        const char* vertexShaderSource = R"(
            #version 460 core
            layout (location = 0) in vec2 position;
            layout (location = 1) in vec3 colours;

            out vec3 vertex_colour;

            void main(){
                vertex_colour = colours;
                gl_Position = vec4(position, 0.0f, 1.0f);
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 460 core

            in vec3 vertex_colour;
            out vec4 finalColor;

            void main() {
                finalColor = vec4(  vertex_colour.x,
                                    vertex_colour.y,
                                    vertex_colour.z,
                                    1.0);
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

    struct vertex2D {
        glm::vec2 position;
        glm::vec3 colour;
    };

    // clang-format off
    // interleaved data
    const std::array<vertex2D, 3> vertices {{
        //   position   |     colour
        {{-0.5f, -0.7f},  {1.f, 0.f, 0.f}},
        {{0.5f, -0.7f},   {0.f, 1.f, 0.f}},
        {{0.0f, 0.6888f}, {0.f, 0.f, 1.f}}
    }};

    const size_t sizeOfVertices = vertices.size() * sizeof(vertex2D);
    // clang-format on

    // buffers
    {
        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, sizeOfVertices, vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "position"),
                                   /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec2::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex2D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "colours"),
                                   /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex2D, colour));
        glEnableVertexArrayAttrib(vao, 1);

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0,
                                  /*stride*/ sizeof(vertex2D));
    }

    std::array<GLfloat, 4> clearColour;
    glUseProgram(program);

    while (!glfwWindowShouldClose(windowPtr)) {

        auto currentTime = duration<float>(system_clock::now() - startTime).count();

        clearColour = {std::sin(currentTime) * 0.5f + 0.5f, std::cos(currentTime) * 0.5f + 0.5f,
                       0.2f, 1.0f};

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(windowPtr);
        glfwPollEvents();
    }

    glfwTerminate();
}
