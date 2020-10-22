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

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"

using namespace gl;
using namespace std::chrono;

int main() {

    auto meshData =
        objLoader::readObjRaw("C:/Users/dokimacbookpro/Documents/Projects/San_Miguel/san-miguel-low-poly.obj");
    // auto meshData =
    // objLoader::readObjRaw("C:/Users/dokimacbookpro/Documents/Projects/OpenGLutorialOffline/testAsets/box.obj");

    // mem
    auto startLoad = system_clock::now();

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                          "C:/Users/dokimacbookpro/Documents/Projects/San_Miguel/san-miguel-low-poly.obj")) {
        // if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        // "C:/Users/dokimacbookpro/Documents/Projects/OpenGLutorialOffline/testAsets/box.obj")) {
        throw std::runtime_error(warn + err);
    }

    int test = 1;
    auto endLoad = system_clock::now();
    fmt::print(stderr, "tiny obj time took {}\n", duration<float>(endLoad - startLoad).count());

    startLoad = system_clock::now();
    fastObjMesh* mesh = fast_obj_read("C:/Users/dokimacbookpro/Documents/Projects/San_Miguel/san-miguel-low-poly.obj");
    endLoad = system_clock::now();
    fmt::print(stderr, "fast obj time took {}\n", duration<float>(endLoad - startLoad).count());
    for (unsigned int ii = 0; ii < mesh->group_count; ii++) {

        const fastObjGroup& grp = mesh->groups[ii];

        fmt::print(stderr, "fast obj group: {}, face count: {}\n", grp.name, grp.face_count);

    }
}
