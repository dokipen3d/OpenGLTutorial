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

    auto window = []() {
        if (!glfwInit()) {
            fmt::print("glfw didnt initialize!\n");
            std::exit(EXIT_FAILURE);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

        /* Create a windowed mode window and its OpenGL context */
        auto window = glfwCreateWindow(1280, 720, "Chapter 5 - Vertex Data", NULL, NULL);

        if (!window) {
            fmt::print("window doesn't exist\n");
            glfwTerminate();
            std::exit(EXIT_FAILURE);
        }

        glfwMakeContextCurrent(window);
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
            #version 450 core
            layout (location = 0) in vec2 position;

            out vec3 vertex_colour;

            void main(){
                vertex_colour = vec3(1.0f, 0.0f, 0.0f);
                gl_Position = vec4(position, 0.0f, 1.0f);
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 450 core

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

    // buffers
    {
        // extra braces required in c++11.
        const std::array<glm::vec2, 3> vertices{{{-0.5f, -0.7f}, {0.5f, -0.7f}, {0.0f, 0.6888f}}};

        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);
        glNamedBufferStorage(bufferObject, vertices.size() * sizeof(glm::vec2), nullptr,
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferSubData(bufferObject, 0, vertices.size() * sizeof(glm::vec2), vertices.data());

        // attribute to index mapping. can query shader for its attributes
        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "position"), /*buffer index*/ 0);

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0, sizeof(glm::vec2));

        // data stride
        glVertexArrayAttribFormat(vao, 0, glm::vec2::length(), GL_FLOAT, GL_FALSE, 0);

        glEnableVertexArrayAttrib(vao, 0);
    }

    std::array<GLfloat, 4> clearColour;
    glUseProgram(program);
    while (!glfwWindowShouldClose(window)) {

        auto currentTime = duration<float>(system_clock::now() - startTime).count();
        clearColour = {std::sin(currentTime) * 0.5f + 0.5f, std::cos(currentTime) * 0.5f + 0.5f, 0.2f, 1.0f};

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}
