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
        auto window =
            glfwCreateWindow(1920, 960, "Chapter 19 - MultiDrawIndirect buffers", nullptr, nullptr);

        if (!window) {
            fmt::print("window doesn't exist\n");
            glfwTerminate();
            std::exit(EXIT_FAILURE);
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);

        glbinding::initialize(glfwGetProcAddress, false);
        return window;
    }();

    // debugging
    {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(errorHandler::MessageCallback, 0);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, false);
    }

    auto createShaderProgram = [](const char* vertexShaderSource,
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
            #version 460 core
            layout (location = 0) in vec3 aPosition;
            layout (location = 1) in vec3 aNormal;
            layout (location = 2) in vec2 aTexCoord;
            layout (location = 3) in float aTextureIndex;


            layout (location = 0) out vec3 normal;
            layout (location = 1) out vec2 uv;
            layout (location = 2) out vec3 position;
            layout (location = 3) out flat float textureIndex;


            uniform mat4 MVP;

            void main(){
                position = aPosition;
                normal = aNormal;
                uv = aTexCoord;
                textureIndex = aTextureIndex;

                gl_Position = MVP * vec4(aPosition, 1.0f);
            }
        )";

    // for bg
    const char* fragmentShaderSourceColour = R"(
            #version 460 core

            layout (location = 0) in vec3 normal;
            layout (location = 1) in vec2 uv;

            out vec4 finalColor;

            void main() {
                finalColor = vec4(normal, 1.0f);
            }
        )";

    // for texturing models
    const char* fragmentShaderSourceTexture = R"(
            #version 460 core

            layout (location = 0) in vec3 normal;
            layout (location = 1) in vec2 uv;
            layout (location = 2) in vec3 position;
            layout (location = 3) in flat float textureIndex;

            out vec4 finalColor;

            vec3 lightPosition = vec3(1,1,1);
            vec3 lightPosition2 = vec3(-2,0,0);

            uniform sampler2DArray Texture;

            void main() {
                vec3 lightDirection = normalize(lightPosition - position);
                vec3 lightDirection2 = normalize(lightPosition2 - position);

                float diffuseLighting = max(dot(normalize(normal), lightDirection), 0);
                float diffuseLighting2 = max(dot(normalize(normal), lightDirection2), 0);

                vec4 textureSample = texture(Texture, vec3(uv, textureIndex));
                finalColor = textureSample * (diffuseLighting + diffuseLighting2 * 0.5f);
            }
        )";

    auto vertexColourProgram = createShaderProgram(vertexShaderSource, fragmentShaderSourceColour);
    auto textureProgram = createShaderProgram(vertexShaderSource, fragmentShaderSourceTexture);

    // clang-format off
    const std::vector<vertex3D> backGroundVertices {{
        //   position   |           normal        |  texCoord
        {{-1.f, -1.f, 0.999999f},  {0.10f, 0.15f, 0.14f}, {0.f, 0.f}},
        {{ 3.f, -1.f, 0.999999f},  {0.10f, 0.15f, 0.14f}, {3.f, 0.f}},
        {{-1.f,  3.f, 0.999999f},  {0.80f, 0.82f, 0.80f}, {0.f, 3.f}}
    }};
    // clang-format on

    auto meshData = objLoader::readObjElements(
        "C:/Users/dokimacbookpro/Documents/Projects/OpenGLutorialOffline/testAsets/tommi.obj");

    for (const auto& group : meshData.groupInfos) {
        fmt::print("group name: {} with startOffset: {}, count: {}\n", group.name,
                   group.startOffset, group.count);
    }

    // buffers
    auto createBufferAndVao = [](const std::vector<vertex3D>& vertices,
                                 const std::vector<int>& indices, GLuint program) -> GLuint {
        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, vertices.size() * sizeof(vertex3D), vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "aPosition"),
                                   /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec3::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex3D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "aNormal"), /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex3D, normal));
        glEnableVertexArrayAttrib(vao, 1);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "aTexCoord"), /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 2, glm::vec2::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex3D, texCoord));
        glEnableVertexArrayAttrib(vao, 2);

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0,
                                  /*stride in bytes*/ sizeof(vertex3D));

        // NEW! element buffer
        if (indices.size() > 0) {
            GLuint elemementBufferObject;
            glCreateBuffers(1, &elemementBufferObject);
            glNamedBufferStorage(elemementBufferObject, indices.size() * sizeof(GLuint),
                                 indices.data(), GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
            glVertexArrayElementBuffer(vao, elemementBufferObject);
        }
        return vao;
    };

    auto backGroundVao = createBufferAndVao(backGroundVertices, {}, vertexColourProgram);
    auto meshVao = createBufferAndVao(meshData.vertices, meshData.indices, textureProgram);

    std::vector<GLfloat> textureIndices = {0.f, 0.f, 0.f, 1.f, 1.f};

    // setup texture indices
    {
        GLuint bufferObject2;

        // one buffer to be shared amonst vaos
        glCreateBuffers(1, &bufferObject2);
        glNamedBufferStorage(bufferObject2, textureIndices.size() * sizeof(GLfloat),
                             textureIndices.data(), GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribFormat(meshVao, 3, 1, GL_FLOAT, GL_FALSE, 0);
        glEnableVertexArrayAttrib(meshVao, 3);

        glVertexArrayAttribBinding(meshVao, glGetAttribLocation(textureProgram, "aTextureIndex"),
                                   /*buffer index*/ 1); // <-- new buffer
        glVertexArrayVertexBuffer(meshVao, 1, bufferObject2, /*offset*/ 0, sizeof(GLfloat));

        // IMPORTANT!!! 1) otherwise shader is per vertex. 2) second arg is the BINDING POINT!
        glVertexArrayBindingDivisor(meshVao, 1, 1);
    }

    // texture
    auto textureGenerator = [](const std::vector<std::string>& filePaths) -> GLuint {
        stbi_set_flip_vertically_on_load(true);

        // create gl texture array
        GLuint textureName;
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &textureName);

        const int hardCodedResolution = 1024;
        // can have possibly 2048+ texture slices (have to be same format)
        glTextureStorage3D(textureName, 1, GL_RGB8, hardCodedResolution, hardCodedResolution,
                           filePaths.size());

        // loop through all textures and put into a slice of the array
        for (auto i = 0u; i < filePaths.size(); ++i) {
            int texWidth, texHeight, texChannels;
            stbi_uc* pixels =
                stbi_load(filePaths[i].c_str(), &texWidth, &texHeight, &texChannels, 0);
            if (!pixels) {
                fmt::print(stderr, "texture {} failed to load\n", filePaths[0]);
                return -1;
            }

            glTextureSubImage3D(textureName, 0, 0, 0, i, hardCodedResolution, hardCodedResolution,
                                1, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            stbi_image_free(pixels);
        }

        glTextureParameteri(textureName, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(textureName, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(textureName, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(textureName, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenerateTextureMipmap(textureName);

        return textureName;
    };

    auto textureArrayName =
        textureGenerator({"body_diffuse.jpg", "tankTops_pants_boots_diffuse.jpg"});

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    std::array<GLfloat, 4> clearColour{0.f, 0.f, 0.f, 1.f};
    GLfloat clearDepth{1.0f};

    glm::mat4 model = glm::mat4(1.0f);

    glm::mat4 view;
    glm::mat4 ortho = glm::ortho(-1.f, 1.f, -1.f, 1.f, 1.f, -1.f);
    glm::mat4 projection;
    glm::mat4 mvp;

    projection = glm::perspective(glm::radians(40.0f), 1280.f / 640.f, 0.1f, 100.0f);

    int mvpLocationVertex = glGetUniformLocation(vertexColourProgram, "MVP");
    int mvpLocationTexture = glGetUniformLocation(textureProgram, "MVP");

    auto groups = meshData.groupInfos;

    // only do this once now
    glBindTextureUnit(0, textureArrayName);

    struct DrawElementsIndirectCommand {
        GLuint vertexCount;
        GLuint instanceCount;
        GLuint firstIndex;
        GLint baseVertex;
        GLuint baseInstance;
    };

    std::vector<DrawElementsIndirectCommand> allDraws = {
        {groups[0].count, 1, groups[0].startOffset, 0, 0},
        {groups[1].count, 1, groups[1].startOffset, 0, 1},
        {groups[2].count, 1, groups[2].startOffset, 0, 2},
        {groups[3].count, 1, groups[3].startOffset, 0, 3},
        {groups[4].count, 1, groups[4].startOffset, 0, 4}};

    auto createIndirectBuffer =
        [](const std::vector<DrawElementsIndirectCommand>& commandBuffer) -> GLuint {
        GLuint indirectBuffer;
        glCreateBuffers(1, &indirectBuffer);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        glNamedBufferStorage(indirectBuffer,
                             commandBuffer.size() * sizeof(DrawElementsIndirectCommand),
                             commandBuffer.data(), GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
        return indirectBuffer;
    };

    auto allCommands = createIndirectBuffer(allDraws);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, allCommands);

    while (!glfwWindowShouldClose(window)) {

        auto currentTime = duration<float>(system_clock::now() - startTime).count();

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        // bg
        glBindVertexArray(backGroundVao);
        glUseProgram(vertexColourProgram);

        glProgramUniformMatrix4fv(vertexColourProgram, mvpLocationVertex, 1, GL_FALSE,
                                  glm::value_ptr(ortho));

        glDrawArrays(GL_TRIANGLES, 0, backGroundVertices.size());

        // mesh
        glBindVertexArray(meshVao);
        glUseProgram(textureProgram);

        glm::mat4 view = glm::lookAt(
            glm::vec3(std::sin(currentTime * 0.5f) * 2.5f,
                      1.25f + ((std::sin(currentTime * 0.32f) + 1.0f) / 2.0f) * 0.3f,
                      std::cos(currentTime * 0.5f) * 2.5f), // Camera is at (4,3,3), in World Space
            glm::vec3(0.f, 1.f, 0.f),                       // and looks at the origin
            glm::vec3(0.f, 1.f, 0.f) // Head is up (set to 0,-1,0 to look upside-down)
        );
        mvp = projection * view * model;
        glProgramUniformMatrix4fv(textureProgram, mvpLocationTexture, 1, GL_FALSE,
                                  glm::value_ptr(mvp));

        // right before call bind buffer
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, allDraws.size(), 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}
