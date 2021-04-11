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



// // make uvs go from -1 to 1
//                 vec2 uv = (colour.xy - vec2(0.5f)) * 2.0;

//                 Ray R = glup_primary_ray(invModelViewProjection, uv);

//                 vec3 pos;
//                 float t = intersectplane( R.O, normalize(R.V), pos);

//                 float d = maxd;

//                 float dplane = tracePlaneY(R.O, normalize(R.V));
//                 vec2 cc = vec2(1, 0);
//                 if (dplane >= 0.) {
//                     d = min(d, dplane);
//                     cc = R.O.xz + normalize(R.V).xz * d; // grid uv at hit point
//                     cc *= 4.; // tiling
//                 }

//                 float aboveOrBelowZero = (float(R.O.y < pos.y)*2.0)-1.0;
//                 float facingDown = dot( normalize(R.V), vec3(0.0 ,aboveOrBelowZero, 0.0));
//                 float splitScreen = 1.0-step(facingDown, 0.0); 

//                 float c = float(dot(normalize(pos), normalize(R.V)) > 0.0);

//                 //float gt = clamp(1.0-gridTexture(cc),0.0,1.0)  * splitScreen;// 1.0-* float(R.O.y < pos.x);
//                 float gt = gridTexture(cc) ;
//                 // vec4 v_clip_coord = modelViewProjection * vec4(pos, 1.0);
//                 // float f_ndc_depth = clamp(v_clip_coord.z / v_clip_coord.w, 0.1, 2.0);
//                 // gl_FragDepth = ((f_ndc_depth + gt) * 0.5);


//                 float far = gl_DepthRange.far;
//                 float near = gl_DepthRange.near;
//                 //float t = -near / (far-near);

//                 float spotlight = min(1.0, 1.5 - 0.6*length(pos.xz));
//                 vec3 finalColour = vec3(gt);
//                 finalColor = vec4(finalColour, 1.0);

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

            // halfspace
            float tracePlaneY(vec3 rp, vec3 rd)
            {
                return rp.y <= 0. ? 0. :
                    rd.y >= 0. ? -1. :
                    rp.y / -rd.y;
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

            float filterWidth2(vec2 uv)
            {
                vec2 dx = dFdx(uv), dy = dFdy(uv);
                return dot(dx, dx) + dot(dy, dy) + .0001;
            }

            float gridThickness = .1; //.2; //.25; //.02; //.4; //


            float gridSmooth(vec2 p)
            {
                vec2 q = p;
                q += .5;
                q -= floor(q);
                q = (gridThickness + 1.) * .5 - abs(q - .5);
                float w = 12.*filterWidth2(p);
                float s = sqrt(gridThickness);
                return smoothstep(.5-w*s,.5+w, max(q.x, q.y));
            }

            #define TILE_SIZE 1.0
            #define LINE_WIDTH 2.0

            // gives the grid lines alpha factor
            float grid(in vec2 uv)
            {
                vec2 grid = vec2(0.0);
                float line = 0.0;
                
                for (int i = 0; i < 1; ++i)
                {
                    // antialiased lines
                    grid = 1.0 * abs(mod(uv + 0.5*TILE_SIZE, TILE_SIZE * pow(10.0, float(i))) - 0.5) / fwidth(uv) / LINE_WIDTH;
                    line = max(line, pow(4.0, float(i)) * (1.0 - min(min(grid.x, grid.y), 1.0)));
                }
                
                return line;
            }

            float intersectplane2( vec3 rayOrigin, vec3 rayDirection, out vec3 pos ){

                const float maxd = 0.;
                vec3 rayNormalized = normalize(rayDirection);
                            
                float hitDepth = (-rayOrigin.y)/rayNormalized.y;
                            
                float d = 80000;
                pos = rayOrigin + rayDirection* 10000; vec3(0.0);
                if (hitDepth >= 0.) {
                    d = min(d, hitDepth);
                    pos = rayOrigin + normalize(rayDirection) * d; // grid uv at hit point
                }
                return d ;
            }


            float filteredGrid( in vec2 p, in vec2 dpdx, in vec2 dpdy )
            {
                const float N = 24.0;
                vec2 w = max(abs(dpdx), abs(dpdy));
                vec2 a = p + 0.5*w;                        
                vec2 b = p - 0.5*w;           
                vec2 i = (floor(a)+min(fract(a)*N,1.0)-
                        floor(b)-min(fract(b)*N,1.0))/(N*w);
                return (1.0-i.x)*(1.0-i.y);
            }

            void main() {

                vec2 uv = (colour.xy - vec2(0.5f)) * 2.0;

                Ray R = glup_primary_ray(invModelViewProjection, uv);

                vec3 pos;
                float depth = intersectplane2(R.O, R.V, pos);

                vec2 samplePos = pos.xz*4;
                float gt = 1.0-filteredGrid(samplePos, dFdx( samplePos ), dFdy( samplePos ));
 
                float far = gl_DepthRange.far; // 1.0
                float near = gl_DepthRange.near;// 0.0;

                vec4 clip_space_pos = modelViewProjection* vec4(pos.xyz, 1.0);

                // get the depth value in normalized device coordinates
                float clip_space_depth = clip_space_pos.z / clip_space_pos.w;

                // and compute the range based on gl_DepthRange settings (not necessary with default settings, but left for completeness)

                float falloff = 24;
                float falloffPower = 2;
                float spotlight = pow(max(1.0-(length(pos.xz)/falloff), 0.0 ), 3.0);

                float depth2 = (((far-near) * clip_space_depth) + near + far) / 2.0;


                // and return the result
                gl_FragDepth =  clamp(depth2, 1e-05, 1.0-1e-05) ;
                finalColor = vec4(vec3(1.0f), gt*spotlight);


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
    glEnable(GL_BLEND);  
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
    std::array<GLfloat, 4> clearColour{0.f, 0.f, 0.f, 1.f};
    GLfloat clearDepth{1.0f};

    const glm::mat4 projection = glm::perspective(
        glm::radians(65.0f),
        static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

    const glm::mat4 model = glm::mat4(1.0f);

    glm::mat4 mvp;
    glm::mat4 mvpInv;
    glClearDepth(1.0f);

    int mvpLocation = glGetUniformLocation(program, "modelViewProjection");
    int invMvpLocationBG = glGetUniformLocation(programBG, "invModelViewProjection");
    int mvpLocationBG = glGetUniformLocation(programBG, "modelViewProjection");

    while (!glfwWindowShouldClose(windowPtr)) {
        auto renderStart = system_clock::now();
        auto currentTime =
            duration<float>(system_clock::now() - startTime).count();
        glm::mat4 view = glm::lookAt(
            glm::vec3(std::sin(currentTime * 0.5f) * 2,
                      (std::sin(currentTime * 0.64f) + 1.5f) / 2.0f,
                      std::cos(currentTime * 0.5f) *
                          2),    // Camera is at (4,3,3), in World Space
            glm::vec3(0, .7, 0), // and looks at the origin
            glm::vec3(0, 1, 0) // Head is up (set to 0,-1,0 to look upside-down)
        );

        mvp = projection * view * model;
        mvpInv = glm::inverse(mvp);
        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

 

        glUseProgram(program);
        

        glProgramUniformMatrix4fv(program, mvpLocation, 1, GL_FALSE,
                                  glm::value_ptr(mvp));
        glDrawArrays(GL_TRIANGLES, 0, (gl::GLsizei)meshData.vertices.size());


        glUseProgram(programBG);
        glProgramUniformMatrix4fv(programBG, invMvpLocationBG, 1, GL_FALSE,
                                  glm::value_ptr(mvpInv));
        glProgramUniformMatrix4fv(programBG, mvpLocationBG, 1, GL_FALSE,
                                  glm::value_ptr(mvp));
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(windowPtr);
        auto timeTaken =
            duration<float>(system_clock::now() - renderStart).count();
        // fmt::print(stderr, "render time: {}\n", timeTaken);
        glfwPollEvents();
    }

    glfwTerminate();
}
