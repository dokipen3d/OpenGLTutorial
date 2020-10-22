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


        /* Create a windowed mode window and its OpenGL context */
        auto window = glfwCreateWindow(1920, 960, "Chapter 9 - Shader Transforms", nullptr, nullptr);

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

            uniform mat4 MVP;

            out vec3 vertex_colour;

            void main(){
                vertex_colour = colours;
                gl_Position = MVP * vec4(position, 1.0f);
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

    auto meshData = objLoader::readObjElements("rubberToy.obj");

    fmt::print("size: {} indices", meshData.indices.size());

    // buffers
    auto vao = [&program, &meshData]() -> GLuint{
        GLuint vao;
        glCreateVertexArrays(1, &vao);

        GLuint bufferObject, elemementBufferObject;
        glCreateBuffers(1, &bufferObject);


        // upload immediately
        glNamedBufferStorage(bufferObject, meshData.vertices.size() * sizeof(vertex3D), meshData.vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "position"), /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "colours"), /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, normal));
        glEnableVertexArrayAttrib(vao, 1);

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0, /*stride*/ sizeof(vertex3D));

        // NEW! element buffer
        glCreateBuffers(1, &elemementBufferObject);
        glNamedBufferStorage(elemementBufferObject, meshData.indices.size() * sizeof(GLuint), meshData.indices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
        glVertexArrayElementBuffer(vao, elemementBufferObject);

        return vao;
    }();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    std::array<GLfloat, 4> clearColour;
    clearColour = {0.f, 0.f, 0.f, 1.f};
    // float depth[1] = { 1.0 };

    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;

    model = glm::mat4(1.0f);
    view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));
    projection = glm::perspective(glm::radians(35.0f), 1280.f / 640.f, 0.1f, 100.0f);

    glm::mat4 mvp = projection * view * model;

    int MVPLocation = glGetUniformLocation(program, "MVP");


    glBindVertexArray(vao);
    glUseProgram(program);

    while (!glfwWindowShouldClose(window)) {

        auto currentTime = duration<float>(system_clock::now() - startTime).count();

        glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.35f, -0.8f));
        view = glm::rotate(t, currentTime*0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        mvp = projection * view * model;
        glProgramUniformMatrix4fv(program, MVPLocation, 1, GL_FALSE, glm::value_ptr(mvp));

        // clearColour = {std::sin(currentTime) * 0.5f + 0.5f, std::cos(currentTime) * 0.5f + 0.5f, 0.2f, 1.0f};

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        // glClearBufferfv(GL_DEPTH, 0, depth);
        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, meshData.indices.size(), GL_UNSIGNED_INT, 0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}
