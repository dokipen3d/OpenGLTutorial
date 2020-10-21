#include "error_handling.hpp"
#include "obj_loader.hpp"

#include <array>
#include <chrono>     // current time
#include <cmath>      // sin & cos
#include <cstdlib>    // for std::exit()
#include <fmt/core.h> // for fmt::print(). implements c++20 std::format
#include <pystring.h>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

    //    // debugging
    //    {
    //        glEnable(GL_DEBUG_OUTPUT);
    //        glDebugMessageCallback(errorHandler::MessageCallback, 0);
    //        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    //        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DEBUG_SEVERITY_NOTIFICATION, 0,
    //        nullptr,
    //                              false);
    //    }

    auto program = []() -> GLuint {
        const char* vertexShaderSource = R"(
            #version 450 core
            layout (location = 0) in vec3 position;
            layout (location = 1) in vec3 normal;
            layout (location = 2) in vec2 texCoord;

            uniform mat4 MVP;

            out vec2 uv;
            out vec3 colour;

            void main(){
                colour = normal;
                uv = texCoord;
                gl_Position = MVP * vec4(position, 1.0f);
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 450 core

            in vec2 uv;
            uniform sampler2DArray Texture;
            uniform int textureIndex;

            in vec3 colour;
            out vec4 finalColor;

            void main() {
                //finalColor = texture(Texture, vec3(uv, textureIndex));
                finalColor = vec4((colour+vec3(1.0f))/2.0f, 1.0f);
                //finalColor = vec4(colour, 1.0f);

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

    // auto meshData = objLoader::readObjElements("rubberToy.obj");
    //auto meshData = objLoader::readObjElements("rubberToy.obj");
            auto meshData =
                objLoader::readObjElements("C:/Users/dokimacbookpro/Documents/Projects/San_Miguel/san-miguel-low-poly.obj");

    fmt::print(stderr, "size: {} indices\n", meshData.indices.size());

    //    for (const auto& group : meshData.groupInfos) {
    //        fmt::print(stderr, "group name: {} with startOffset: {}, count: {}\n", group.name, group.startOffset,
    //        group.count);
    //    }

    // buffers
    auto vao = [&program, &meshData]() -> GLuint {
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

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "normal"), /*buffer idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, normal));
        glEnableVertexArrayAttrib(vao, 1);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "texCoord"), /*buffer idx*/ 0);
        glVertexArrayAttribFormat(vao, 2, glm::vec2::length(), GL_FLOAT, GL_FALSE, offsetof(vertex3D, texCoord));
        glEnableVertexArrayAttrib(vao, 2);

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

    // texture
    auto textureGenerator = [](const std::vector<std::string>& filePaths) -> GLuint {
        stbi_set_flip_vertically_on_load(true);

        // create gl texture array
        GLuint textureName;
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &textureName);

        const int hardCodedResolution = 512;
        // can have possibly 2048+ texture slices (have to be same format)
        glTextureStorage3D(textureName, 1, GL_RGB8, hardCodedResolution, hardCodedResolution, filePaths.size());

        // loop through all textures and put into a slice of the array
        for (auto i = 0u; i < filePaths.size(); ++i) {
            int texWidth, texHeight, texChannels;
            stbi_uc* pixels = stbi_load(filePaths[i].c_str(), &texWidth, &texHeight, &texChannels, 0);
            if (!pixels) {
                fmt::print(stderr, "texture {} failed to load\n", filePaths[0]);
                return -1;
            }

            glTextureSubImage3D(textureName, 0, 0, 0, i, hardCodedResolution, hardCodedResolution, 1, GL_RGB,
                                GL_UNSIGNED_BYTE, pixels);
            stbi_image_free(pixels);
        }

        glTextureParameteri(textureName, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(textureName, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(textureName, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(textureName, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenerateTextureMipmap(textureName);

        return textureName;
    };

    auto textureArrayName = textureGenerator({"toylowres.jpg"});
    // auto textureArrayName = textureGenerator({"body_diffuse.jpg", "tankTops_pants_boots_diffuse.jpg"});

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 mvp;
    projection = glm::perspective(glm::radians(35.0f), 1280.f / 640.f, 0.1f, 100.0f);
    int MVPLocation = glGetUniformLocation(program, "MVP");
    fmt::print(stderr, "mvp loc {} \n", MVPLocation);

    // use our stuff!
    glBindVertexArray(vao);
    glUseProgram(program);
    glBindTextureUnit(0, textureArrayName); // only do this once now
    fmt::print(stderr, "texture array id  {} \n", textureArrayName);

    int textureSliceLocation = glGetUniformLocation(program, "textureIndex");
    fmt::print(stderr, "texture loc {} \n", textureSliceLocation);
    auto& groups = meshData.groupInfos;

    //    view =
    //        glm::lookAt(glm::vec3(16.f , 2.f, 10.f), // Camera is at (4,3,3), in World Space
    //                    glm::vec3(12.27f, 0.9f, 4.83f),                          // and looks at the origin
    //                    glm::vec3(0, 1, 0)                           // Head is up (set to 0,-1,0 to look upside-down)
    //        );

    while (!glfwWindowShouldClose(window)) {

        auto currentTime = duration<float>(system_clock::now() - startTime).count();

        view = glm::lookAt(glm::vec3(std::sin(2 * 0.5f) * 4, ((std::sin(3.2 * 0.5f) + 1.0f) / 2.0f) * 0.5,
                                     std::cos(2 * 0.5f) * 4), // Camera is at (4,3,3), in World Space
                           glm::vec3(0, 0.7, 0),              // and looks at the origin
                           glm::vec3(0, 1, 0)                 // Head is up (set to 0,-1,0 to lookupside-down)
        );

                view = glm::lookAt(
                    glm::vec3(12.f, 2.f, 8.f) +
                        glm::vec3(std::sin(currentTime * 0.5f) * 4, ((std::sin(currentTime * 0.5f) + 1.0f) / 2.0f) *
                        0.5,
                                  std::cos(currentTime * 0.5f) * 4), // Camera is at (4,3,3), in World Space
                    glm::vec3(10.27f, 2.f, 4.83f),                   // and looks at the origin
                    glm::vec3(0, 1, 0)                               // Head is up (set to 0,-1,0 to look upside-down)
                );

        mvp = projection * view * model;
        glProgramUniformMatrix4fv(program, MVPLocation, 1, GL_FALSE, glm::value_ptr(mvp));

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        // glClearBufferfv(GL_DEPTH, 0, depth);
        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        glProgramUniform1i(program, textureSliceLocation, 0); // much cheaper than binding texture
        //        glDrawElements(GL_TRIANGLES, groups[0].count, GL_UNSIGNED_INT, (void*)(sizeof(GLuint) *
        //        groups[0].startOffset)); glDrawElements(GL_TRIANGLES, groups[1].count, GL_UNSIGNED_INT,
        //        (void*)(sizeof(GLuint) * groups[1].startOffset)); glDrawElements(GL_TRIANGLES, groups[2].count,
        //        GL_UNSIGNED_INT, (void*)(sizeof(GLuint) * groups[2].startOffset));

        // glProgramUniform1i(program, textureSliceLocation, 1); // much cheaper than binding texture
        //        glDrawElements(GL_TRIANGLES, groups[3].count, GL_UNSIGNED_INT, (void*)(sizeof(GLuint) *
        //        groups[3].startOffset)); glDrawElements(GL_TRIANGLES, groups[4].count, GL_UNSIGNED_INT,
        //        (void*)(sizeof(GLuint) * groups[4].startOffset));
        glDrawElements(GL_TRIANGLES, meshData.indices.size(), GL_UNSIGNED_INT, (void*)0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}
