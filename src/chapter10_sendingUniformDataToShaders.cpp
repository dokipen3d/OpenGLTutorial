#include "error_handling.hpp"
#include <array>
#include <chrono>     // current time
#include <cmath>      // sin & cos
#include <cstdlib>    // for std::exit()
#include <fmt/core.h> // for fmt::print(). implements c++20 std::format
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
    const int width = 1280;
    const int height = 720;

    auto windowPtr = [](int w, int h) {
        if (!glfwInit()) {
            fmt::print("glfw didnt initialize!\n");
            std::exit(EXIT_FAILURE);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

        /* Create a windowed mode window and its OpenGL context */
        auto windowPtr = glfwCreateWindow(w, h, "Chapter 10 - Sending Uniform Paramters to Shaders",
                                       nullptr, nullptr);

        if (!windowPtr) {
            fmt::print("window doesn't exist\n");
            glfwTerminate();
            std::exit(EXIT_FAILURE);
        }
        glfwSetWindowPos(windowPtr, 520, 180);

        glfwMakeContextCurrent(windowPtr);
        glbinding::initialize(glfwGetProcAddress, false);
        return windowPtr;
    }(width, height);
    // debugging
    {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(errorHandler::MessageCallback, 0);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, false);
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

    auto program = createProgram(R"(
        #version 450 core
        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 colours;

        out vec2 fragCoord;

        void main(){
            fragCoord = colours;
            gl_Position = vec4(position, 0.0f, 1.0f);
        }
    )",
                                 R"(
        #version 450 core

        // The MIT License
        // Copyright © 2013 Inigo Quilez
        // Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
        // associated documentation files (the "Software"), to deal in the Software without restriction,
        // including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
        // and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
        // subject to the following conditions: The above copyright notice and this permission notice shall be
        // included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS",
        // WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
        // MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
        // COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
        // TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
        // IN THE SOFTWARE.
        //
        // I've not seen anybody out there computing correct cell interior distances for Voronoi
        // patterns yet. That's why they cannot shade the cell interior correctly, and why you've
        // never seen cell boundaries rendered correctly.
        //
        // However, here's how you do mathematically correct distances (note the equidistant and non
        // degenerated grey isolines inside the cells) and hence edges (in yellow):
        //
        // http://www.iquilezles.org/www/articles/voronoilines/voronoilines.htm
        //
        // More Voronoi shaders:
        //
        // Exact edges:  https://www.shadertoy.com/view/ldl3W8
        // Hierarchical: https://www.shadertoy.com/view/Xll3zX
        // Smooth:       https://www.shadertoy.com/view/ldB3zc
        // Voronoise:    https://www.shadertoy.com/view/Xd23Dh

        in vec2 fragCoord;
        out vec4 finalColor;

        uniform float iTime;
        uniform vec2 iResolution;

        vec2 hash2( vec2 p )
        {
            // texture based white noise
            //return textureLod( iChannel0, (p+0.5)/256.0, 0.0 ).xy;

            // procedural white noise
            return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
        }

        vec3 voronoi( in vec2 x )
        {
            vec2 n = floor(x);
            vec2 f = fract(x);

            //----------------------------------
            // first pass: regular voronoi
            //----------------------------------
            vec2 mg, mr;

            float md = 8.0;
            for( int j=-1; j<=1; j++ )
            for( int i=-1; i<=1; i++ )
            {
                vec2 g = vec2(float(i),float(j));
                vec2 o = hash2( n + g );

                o = 0.5 + 0.5*sin( iTime + 6.2831*o );

                vec2 r = g + o - f;
                float d = dot(r,r);

                if( d<md )
                {
                    md = d;
                    mr = r;
                    mg = g;
                }
            }

            //----------------------------------
            // second pass: distance to borders
            //----------------------------------
            md = 8.0;
            for( int j=-2; j<=2; j++ )
            for( int i=-2; i<=2; i++ )
            {
                vec2 g = mg + vec2(float(i),float(j));
                vec2 o = hash2( n + g );

                o = 0.5 + 0.5*sin( iTime + 6.2831*o );

                vec2 r = g + o - f;

                if( dot(mr-r,mr-r)>0.00001 )
                md = min( md, dot( 0.5*(mr+r), normalize(r-mr) ) );
            }

            return vec3( md, mr );
        }


        void main() {

            vec2 p = fragCoord * vec2(iResolution.x/iResolution.y, 1);

            vec3 c = voronoi( 8.0*p );

            // isolines
            vec3 col = c.x*(0.5 + 0.5*sin(64.0*c.x))*vec3(1.0);
            // borders
            col = mix( vec3(1.0,0.6,0.0), col, smoothstep( 0.04, 0.07, c.x ) );
            // feature points
            float dd = length( c.yz );
            col = mix( vec3(1.0,0.6,0.1), col, smoothstep( 0.0, 0.12, dd) );
            col += vec3(1.0,0.6,0.1)*(1.0-smoothstep( 0.0, 0.04, dd));

            finalColor = vec4(col,1.0);
        }
    )");

    struct vertex2D {
        glm::vec2 position;
        glm::vec2 colour;
    };

    // clang-format off
    // interleaved data
    const std::array<vertex2D, 3> backGroundVertices {{
        //   position   |   colour
        {{-1.f, -1.f},  {0.0f, 0.0f}},
        {{ 3.f, -1.f},  {2.0f, 0.0f}},
        {{-1.f,  3.f},  {0.0f, 2.00f}}
    }};

    // clang-format on

    // buffers
    auto createBufferAndVao = [&program](const std::array<vertex2D, 3>& vertices) -> GLuint {
        // in core profile, at least 1 vao is needed
        GLuint vao;
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint bufferObject;
        glCreateBuffers(1, &bufferObject);

        // upload immediately
        glNamedBufferStorage(bufferObject, vertices.size() * sizeof(vertex2D), vertices.data(),
                             GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "position"),
                                   /*buffer index*/ 0);
        glVertexArrayAttribFormat(vao, 0, glm::vec2::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex2D, position));
        glEnableVertexArrayAttrib(vao, 0);

        glVertexArrayAttribBinding(vao, glGetAttribLocation(program, "colours"), /*buffs idx*/ 0);
        glVertexArrayAttribFormat(vao, 1, glm::vec2::length(), GL_FLOAT, GL_FALSE,
                                  offsetof(vertex2D, colour));
        glEnableVertexArrayAttrib(vao, 1);

        // buffer to index mapping
        glVertexArrayVertexBuffer(vao, 0, bufferObject, /*offset*/ 0, /*stride*/ sizeof(vertex2D));

        return vao;
    };

    auto backGroundVao = createBufferAndVao(backGroundVertices);

    std::array<GLfloat, 4> clearColour{0.f, 0.f, 0.f, 0.f};
    std::array<GLfloat, 1> clearDepth{1.0};

    glUseProgram(program);

    int timeUniformLocation = glGetUniformLocation(program, "iTime");
    int resolutionUniformLocation = glGetUniformLocation(program, "iResolution");
    glProgramUniform2f(program, resolutionUniformLocation, width, height);

    while (!glfwWindowShouldClose(windowPtr)) {

        glClearBufferfv(GL_COLOR, 0, clearColour.data());
        glClearBufferfv(GL_DEPTH, 0, clearDepth.data());

        // draw full screen triangle
        glBindVertexArray(backGroundVao);

        // send time to shader
        auto currentTime = duration<float>(system_clock::now() - startTime).count();
        glProgramUniform1f(program, timeUniformLocation, currentTime);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(windowPtr);
        glfwPollEvents();
    }

    glfwTerminate();
}
