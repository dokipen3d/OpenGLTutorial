#pragma once
#include "pystring.h"
#include <fmt/color.h>
#include <fmt/core.h> // for fmt::print(). implements c++20 std::format

#include <glbinding/gl/gl.h>
#include <iostream>
#include <string>
#include <unordered_map>
using namespace gl;

namespace errorHandler {

static const std::unordered_map<GLenum, std::string> errorSourceMap{
    {GL_DEBUG_SOURCE_API, "SOURCE_API"},
    {GL_DEBUG_SOURCE_WINDOW_SYSTEM, "WINDOW_SYSTEM"},
    {GL_DEBUG_SOURCE_SHADER_COMPILER, "SHADER_COMPILER"},
    {GL_DEBUG_SOURCE_THIRD_PARTY, "THIRD_PARTY"},
    {GL_DEBUG_SOURCE_APPLICATION, "APPLICATION"},
    {GL_DEBUG_SOURCE_OTHER, "OTHER"}};

static const std::unordered_map<GLenum, std::string> errorTypeMap{
    {GL_DEBUG_TYPE_ERROR, "ERROR"},
    {GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "DEPRECATED_BEHAVIOR"},
    {GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "UNDEFINED_BEHAVIOR"},
    {GL_DEBUG_TYPE_PORTABILITY, "PORTABILITY"},
    {GL_DEBUG_TYPE_PERFORMANCE, "PERFORMANCE"},
    {GL_DEBUG_TYPE_OTHER, "OTHER"},
    {GL_DEBUG_TYPE_MARKER, "MARKER"}};

static const std::unordered_map<GLenum, std::string> severityMap{
    {GL_DEBUG_SEVERITY_HIGH, "HIGH"},
    {GL_DEBUG_SEVERITY_MEDIUM, "MEDIUM"},
    {GL_DEBUG_SEVERITY_LOW, "LOW"},
    {GL_DEBUG_SEVERITY_NOTIFICATION, "NOTIFICATION"}};

void MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                     GLsizei length, const GLchar* message,
                     const void* userParam) {
    std::string src = errorSourceMap.at(source);
    std::string tp = errorTypeMap.at(type);
    std::string sv = severityMap.at(severity);
    int found =
        pystring::find(message, "is being recompiled based on GL state");
    if (found != -1) {
        return;
    }
    fmt::print(
        stderr,
        "GL CALLBACK: {0:s} type = {1:s}, severity = {2:s}, message = {3:s}\n",
        src, tp, sv, message);
}

void printShaderLog(GLuint shader) {
    if (glIsShader(shader)) {
        // Shader log length
        GLint log_length;

        // Get info string length
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> v(log_length);

        std::string log;
        log.reserve(log_length);

        // Get info log
        glGetShaderInfoLog(shader, log_length, nullptr, v.data());
        std::string s(begin(v), end(v));

        if (log_length > 0) {
            // Print Log
            fmt::print(stderr, fmt::fg(fmt::color::light_green), "{}\n", s);
        }
    } else {
        fmt::print(stderr, "Name {} is not a shader\n",
                   shader);
    }
}

bool checkShader(GLuint shaderIn, std::string shaderName) {
    // Check fragment shader for errors
    GLboolean fShaderCompiled = GL_FALSE;
    glGetShaderiv(shaderIn, GL_COMPILE_STATUS, &fShaderCompiled);
    if (fShaderCompiled != GL_TRUE) {
        fmt::print(stderr, "Unable to compile {0} shader {1}\n", shaderName, shaderIn);
        printShaderLog(shaderIn);
        return false;
    }
    return true;
}

} // namespace errorHandler
