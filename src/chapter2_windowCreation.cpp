#include <array>
#include <chrono>     // current time
#include <cmath>      // sin & cos
#include <cstdlib>    // for std::exit()
#include <fmt/core.h> // for fmt::print(). implements c++20 std::format
// this is really important to make sure that glbindings does not
// clash with glfw's opengl includes. otherwise we get ambigous
// overloads.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>

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
            glfwCreateWindow(1600, 900, "Chapter 2 - Window Creation", nullptr, nullptr);

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

    std::array<GLfloat, 4> clearColour;

    while (!glfwWindowShouldClose(windowPtr)) {

        auto currentTime = duration<float>(system_clock::now() - startTime).count();
        clearColour = {std::sin(currentTime) * 0.5f + 0.5f, std::cos(currentTime) * 0.5f + 0.5f,
                       0.2f, 1.0f};

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glfwSwapBuffers(windowPtr);
        glfwPollEvents();
    }

    glfwTerminate();
}
