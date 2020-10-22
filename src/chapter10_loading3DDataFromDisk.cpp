#include "error_handling.hpp"
#include "obj_loader.hpp"

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

    auto window = []() {
        if (!glfwInit()) {
            fmt::print("glfw didnt initialize!\n");
            std::exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);

        /* Create a windowed mode window and its OpenGL context */
        auto window = glfwCreateWindow(1280, 1280, "Chapter 8 - Loading Data from Disk", NULL, NULL);

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
            layout (location = 0) in vec3 position;
            layout (location = 1) in vec3 colours;
            layout (location = 2) in vec2 texCoord;

            out vec3 vertex_colour;
            uniform mat4 projection;


            void main(){
                vertex_colour = colours;
                gl_Position = projection * vec4(position, 1.0f);
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

    auto meshData = objLoader::readObjSplit("rubberToy.obj");

    const size_t sizeOfVertices = meshData.vertices.size();
    fmt::print("size {}", sizeOfVertices);
    // buffers
    {
        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, sizeOfVertices * sizeof(vertex3D), meshData.vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "position"), /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "colours"), /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, normal));
        glEnableVertexArrayAttrib(vao, 1);

        // dont HAVE to use texCoordData

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0, /*stride*/ sizeof(vertex3D));
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);


    std::array<GLfloat, 4> clearColour;
    clearColour = {0.f, 0.f, 0.f, 1.f};

    glUseProgram(program);

    glm::mat4 projection = glm::ortho( -1, 1, -1, 1, -1, 1 );

    int projectionLocation = glGetUniformLocation(program, "projection");
    glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, glm::value_ptr(projection));

    while (!glfwWindowShouldClose(window)) {
        // auto currentTime = duration<float>(system_clock::now() - startTime).count();
        // clearColour = {std::sin(currentTime) * 0.5f + 0.5f, std::cos(currentTime) * 0.5f + 0.5f, 0.2f, 1.0f};

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        // glClearBufferfv(GL_DEPTH, 0, depth);
        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLES, 0, sizeOfVertices);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}
