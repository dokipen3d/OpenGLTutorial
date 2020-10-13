#include "error_handling.hpp"
#include <chrono>     // current time
#include <cmath>      // sin & cos
#include <string>
#include <array>
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
        auto window = glfwCreateWindow(1280, 720, "Chapter 4 - Error Handling", NULL, NULL);

        if (!window) {
            fmt::print("window doesn't exist\n");
            glfwTerminate();
            std::exit(EXIT_FAILURE);
        }

        glfwMakeContextCurrent(window);
        glbinding::initialize(glfwGetProcAddress, false);
        return window;
    }();

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(errorHandler::MessageCallback, 0);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // useful for debugging as error
                                           // will occur as we step through
    //glbinding::aux::enableGetErrorCallback();

    const char* vertexShaderSource = R"VERTEX(
        #version 430 core
        out vec3 colour;

        const vec4 vertices[] = vec4[]( vec4(-0.5f, -0.7f,    0.0, 1.0), 
                                        vec4( 0.5f, -0.7f,    0.0, 1.0),    
                                        vec4( 0.0f,  0.6888f, 0.0, 1.0));   

        const vec3 colours[] = vec3[](  vec3( 1.0, 0.0, 0.0), 
                                        vec3( 0.0, 1.0, 0.0),    
                                        vec3( 0.0, 0.0, 1.0));   

        void main(){
            colour = colours[gl_VertexID];
            gl_Position = vertices[gl_VertexID];  
        }
    )VERTEX";

    const char* fragmentShaderSource = R"FRAGMENT(
        #version 430 core

        in vec3 colour;
        out vec4 finalColor;

        void main() {
            finalColor = vec4(colour.x, colour.y, colour.z, 1.0);
        }
    )FRAGMENT";

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
    glUseProgram(program);

    // in core profile, at least 1 vao is needed
    GLuint vao;
    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    std::array<GLfloat, 4> clearColour;

    while (!glfwWindowShouldClose(window)) {

        auto currentTime =
            duration<float>(system_clock::now() - startTime).count();
        clearColour = {std::sin(currentTime) * 0.5f + 0.5f,
                       std::cos(currentTime) * 0.5f + 0.5f, 0.2f, 1.0f};

        glClearBufferfv(GL_COLOR, 0, clearColour.data());

        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}
