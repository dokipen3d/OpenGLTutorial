#include "error_handling.hpp"
#include "obj_loader_simple_split_cpp.hpp"

#include <array>
#include <chrono>     // current time
#include <cmath>      // sin & cos
#include <cstdlib>    // for std::exit()
#include <fmt/core.h> // for fmt::print(). implements c++20 std::format

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

    const int width = 1600;
    const int height = 900;

    auto windowPtr = [&]() {
        if (!glfwInit()) {
            fmt::print("glfw didnt initialize!\n");
            std::exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, true);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

        /* Create a windowed mode window and its OpenGL context */
        auto windowPtr = glfwCreateWindow(
            width, height, "Chapter 12 - Shader Transforms", nullptr, nullptr);

        if (!windowPtr) {
            fmt::print("window doesn't exist\n");
            glfwTerminate();
            std::exit(EXIT_FAILURE);
        }

        glfwSetWindowPos(windowPtr, 160, 90);
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
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                              false);
    }

    auto createProgram = [](const char* vertexShaderSource,
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

    const char* fragmentShaderSourceGrid = R"(
            #version 450 core

            in vec3 colour;
            out vec4 finalColor;

            uniform mat4 invModelViewProjection;
            uniform mat4 modelViewProjection;

            float intersectplane( vec3 ro, vec3 rd, out vec3 pos )
            {

                float tmin = 10000.0;

                // raytrace-plane

                float h = (0.0-ro.y)/rd.y;   
                //h = max(0.0, h);
                if( h>0.0) 
                { 
                    tmin = h; 
                    pos = ro + h*rd;
                }

                return tmin;
            }
            //https://stackoverflow.com/questions/42633685/glsl-how-to-calculate-a-ray-direction-using-the-projection-matrix
            struct Ray {
                vec3 O; // Origin
                vec3 V; // Direction vector
            };

            // Computes the ray that passes through the current fragment
            // The ray is in world space.
            Ray glup_primary_ray(mat4 invmodelViewProj, vec2 ssuv) {
                vec4 near = vec4(
                    ssuv,
                    0.0,
                    1.0
                );

                near = invmodelViewProj * near ;
                vec4 far = near + invmodelViewProj[2] ;
                near.xyz /= near.w ;
                far.xyz /= far.w ;
                return Ray(near.xyz, far.xyz-near.xyz) ;
            }

            const float N = 20.0; // grid ratio
            float gridTexture( in vec2 p )
            {
                // coordinates
                vec2 i = step( fract(p), vec2(1.0/N) );
                //pattern
                //return (1.0-i.x)*(1.0-i.y);   // grid (N=10)
                
                // other possible patterns are these
                //return 1.0-i.x*i.y;           // squares (N=4)
                return 1.0-i.x-i.y+2.0*i.x*i.y; // checker (N=2)
            }

            void main() {

                // make uvs go from -1 to 1
                vec2 uv = (colour.xy - vec2(0.5f)) * 2.0;

                Ray R = glup_primary_ray(invModelViewProjection, uv);

                // rayDir = modelViewProjection * vec4(colour,1.f);
                vec3 pos;
                float t = intersectplane( R.O, R.V, pos);
                

                float aboveOrBelowZero = (float(R.O.y < pos.y)*2.0)-1.0;
                float facingDown = dot( normalize(R.V), vec3(0.0 ,aboveOrBelowZero, 0.0));
                float splitScreen = 1.0-step(facingDown, 0.0); 
                float gt = clamp(1.0-gridTexture(pos.xz),0.0,1.0)  * splitScreen;// 1.0-* float(R.O.y < pos.x);
                //float gt = clamp(1.0-gridTexture(pos.xz),0.0,1.0);
                // vec4 v_clip_coord = modelViewProjection * vec4(pos, 1.0);
                // float f_ndc_depth = clamp(v_clip_coord.z / v_clip_coord.w, 0.1, 2.0);
                // gl_FragDepth = ((f_ndc_depth + gt) * 0.5);


                float far = gl_DepthRange.far;
                float near = gl_DepthRange.near;
                //float t = -near / (far-near);

                float spotlight = min(1.0, 1.5 - 0.6*length(pos.xz));
                vec3 finalColour = vec3(gt*spotlight);
                finalColor = vec4(finalColour, 1.0);

                vec4 clip_space_pos = modelViewProjection* vec4(pos.xyz, 1.0);

                // get the depth value in normalized device coordinates
                float clip_space_depth = clip_space_pos.z / clip_space_pos.w;

                // and compute the range based on gl_DepthRange settings (not necessary with default settings, but left for completeness)
                

                float depth2 = (((far-near) * clip_space_depth) + near + far) / 2.0;

                // and return the result
                gl_FragDepth = max(depth2,0.01);


            }
        )";

    const char* fragmentShaderSource = R"(
        #version 450 core

        in vec3 colour;
        out vec4 finalColor;

        void main() {
            finalColor = vec4(colour, 1.0);
        }
        )";

    auto programBG = createProgram(R"(
        #version 450 core
        out vec3 colour;

        const vec4 vertices[] = vec4[]( vec4(-1.f, -1.f, 0.9999, 1.0),
                                        vec4( 3.f, -1.f, 0.9999, 1.0),    
                                        vec4(-1.f,  3.f, 0.9999, 1.0));   
        const vec3 colours[]   = vec3[](vec3(0.0f, 0.0f, 0.0f),
                                        vec3(2.f,  0.0f, 0.0f),
                                        vec3(0.0f, 2.0f, 0.0f));
        

        void main(){
            colour = colours[gl_VertexID];
            gl_Position = vertices[gl_VertexID];  
        }
    )",
                                   fragmentShaderSourceGrid);

    auto program = createProgram(R"(
            #version 450 core
            layout (location = 0) in vec3 position;
            layout (location = 1) in vec3 normal;

            out vec3 colour;

            uniform mat4 modelViewProjection;

            vec3 remappedColour = (normal + vec3(1.f)) / 2.f;

            void main(){
                colour = remappedColour;
                gl_Position = modelViewProjection * vec4(position, 1.0f);
            }
        )",
                                 fragmentShaderSource);

    auto meshData = objLoader::readObjSplit("tommy.obj");

    auto createBuffer =
        [&program](const std::vector<vertex3D>& vertices) -> GLuint {
        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, vertices.size() * sizeof(vertex3D),
                             vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        return bufferObject;
    };

    auto backGroundBuffer = createBuffer(meshData.vertices);

    auto createVertexArrayObject = [](GLuint program) -> GLuint {
        GLuint vao;
        glCreateVertexArrays(1, &vao);

        glEnableVertexArrayAttrib(vao, 0);
        glEnableVertexArrayAttrib(vao, 1);

        glVertexArrayAttribBinding(vao,
                                   glGetAttribLocation(program, "position"),
                                   /*buffer index*/ 0);
        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "normal"),
                                   /*buffs idx*/ 0);

        glVertexArrayAttribFormat(vao, 0, glm::vec3::length(), GL_FLOAT,
                                  GL_FALSE, offsetof(vertex3D, position));
        glVertexArrayAttribFormat(vao, 1, glm::vec3::length(), GL_FLOAT,
                                  GL_FALSE, offsetof(vertex3D, normal));

        return vao;
    };

    auto meshVao = createVertexArrayObject(program);
    glVertexArrayVertexBuffer(meshVao, 0, backGroundBuffer,
                              /*offset*/ 0,
                              /*stride*/ sizeof(vertex3D));
    glBindVertexArray(meshVao);

    glEnable(GL_DEPTH_TEST);

    std::array<GLfloat, 4> clearColour{0.f, 0.f, 0.f, 1.f};
    GLfloat clearDepth{1.0f};

    const glm::mat4 projection = glm::perspective(
        glm::radians(65.0f),
        static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

    const glm::mat4 model = glm::mat4(1.0f);

    glm::mat4 mvp;
    glm::mat4 mvpInv;
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    int mvpLocation = glGetUniformLocation(program, "modelViewProjection");
    int invMvpLocationBG = glGetUniformLocation(programBG, "invModelViewProjection");
    int mvpLocationBG = glGetUniformLocation(programBG, "modelViewProjection");

    while (!glfwWindowShouldClose(windowPtr)) {
        auto renderStart = system_clock::now();
        auto currentTime =
            duration<float>(system_clock::now() - startTime).count();
        glm::mat4 view = glm::lookAt(
            glm::vec3(std::sin(currentTime * 0.5f) * 2,
                      (std::sin(currentTime * 1.02f) ) * 2, //+ 1.0f) / 2.0f
                      std::cos(currentTime * 0.5f) *
                          2),    // Camera is at (4,3,3), in World Space
            glm::vec3(0, .2, 0), // and looks at the origin
            glm::vec3(0, 1, 0) // Head is up (set to 0,-1,0 to look upside-down)
        );

        mvp = projection * view * model;
        mvpInv = glm::inverse(mvp);
        //glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(programBG);
        glProgramUniformMatrix4fv(programBG, invMvpLocationBG, 1, GL_FALSE,
                                  glm::value_ptr(mvpInv));
        glProgramUniformMatrix4fv(programBG, mvpLocationBG, 1, GL_FALSE,
                                  glm::value_ptr(mvp));
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glUseProgram(program);
        

        glProgramUniformMatrix4fv(program, mvpLocation, 1, GL_FALSE,
                                  glm::value_ptr(mvp));
        glDrawArrays(GL_TRIANGLES, 0, (gl::GLsizei)meshData.vertices.size());

        glfwSwapBuffers(windowPtr);
        auto timeTaken =
            duration<float>(system_clock::now() - renderStart).count();
        // fmt::print(stderr, "render time: {}\n", timeTaken);
        glfwPollEvents();
    }

    glfwTerminate();
}
