#pragma once

#include "glm/glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <algorithm>
#include <numeric>

#include <cctype>
#include <chrono> // current time
#include <climits>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <omp.h>
#include <pystring.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct vertex3D {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

namespace objLoader {

using namespace std;

constexpr uint32_t packCharsToIntKey(char a, char b) {
    return (static_cast<uint32_t>(a) << 8) | static_cast<uint32_t>(b);
}

constexpr uint32_t v = packCharsToIntKey('v', ' ');
constexpr uint32_t vn = packCharsToIntKey('v', 'n');
constexpr uint32_t vt = packCharsToIntKey('v', 't');
constexpr uint32_t vp = packCharsToIntKey('v', 'p');
constexpr uint32_t f = packCharsToIntKey('f', ' ');
constexpr uint32_t comment = packCharsToIntKey('#', ' ');

// add groups
constexpr uint32_t g = packCharsToIntKey('g', ' ');

struct RawMeshData {

    RawMeshData() : positions(1), normals(1), textureCoords(1) {
    }
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> textureCoords;
    std::vector<glm::ivec3> faceIndices;
};

struct MeshDataSplit {
    std::vector<vertex3D> vertices;
};

RawMeshData readObjRaw(const std::string& filePath) {

    std::ios_base::sync_with_stdio(false);

    using namespace std::chrono;
    auto startTime = system_clock::now();

    fmt::print(stderr, "starting obj loader\n");
    RawMeshData meshData;

    char line[128];
    size_t line_buf_size = 0;
    size_t line_size;

    FILE* fp = fopen(filePath.c_str(), "r");
    if (!fp) {
        fmt::print(stderr, "Error opening file\n");
    }
    // for storing where spaces and slashes go
    std::vector<int> spacePositions(8);

    char* end;
    uint16_t key;
    int a, b, c, d, e, fi, gi, h, i;
    

    while (fgets(line, 128, fp)) {
        { // setup
            line_size = strlen(line);
            spacePositions.clear();
            key = packCharsToIntKey(line[0], line[1]);
        }

        // spaces after the first will always be after 3
        for (auto i = 0u; i < line_size; ++i) {
            if (line[i] == ' ') {
                line[i] = '\0';
                spacePositions.push_back(i + 1);
            }
        }
        spacePositions.push_back(static_cast<int>(line_size));


        switch (key) {

        case v: {
            meshData.positions.emplace_back(
                std::strtof(&line[spacePositions[0]], &end),
                std::strtof(&line[spacePositions[1]], nullptr),
                std::strtof(&line[spacePositions[2]], nullptr));

            break;
        }
        case vn: {
            meshData.normals.emplace_back(
                std::strtof(&line[spacePositions[0]], nullptr),
                std::strtof(&line[spacePositions[1]], nullptr),
                std::strtof(&line[spacePositions[2]], nullptr));
            break;
        }
        case vt: {
            meshData.textureCoords.emplace_back(
                std::strtof(&line[spacePositions[0]], nullptr),
                std::strtof(&line[spacePositions[1]], nullptr));
            break;
        }
        case f: {
            // is face

            a = std::strtol(&line[spacePositions[0]], &end, 10);
            b = std::strtol(end + (*end == '/'), &end, 10);
            c = std::strtol(end + (*end == '/'), &end, 10);
            meshData.faceIndices.emplace_back(a, b, c);

            d = std::strtol(&line[spacePositions[1]], &end, 10);
            e = std::strtol(end + (*end == '/'), &end, 10);
            fi = std::strtol(end + (*end == '/'), &end, 10);

            meshData.faceIndices.emplace_back(d, e, fi);

            gi = std::strtol(&line[spacePositions[2]], &end, 10);
            h = std::strtol(end + (*end == '/'), &end, 10);
            i = std::strtol(end + (*end == '/'), &end, 10);

            meshData.faceIndices.emplace_back(gi, h, i);

            if (spacePositions.size() == 5) {
                // face 0
                meshData.faceIndices.emplace_back(a, b, c);
                // face 2
                meshData.faceIndices.emplace_back(gi, h, i);

                // reuse def as those temps aren't needed
                d = std::strtol(&line[spacePositions[3]], &end, 10);
                e = std::strtol(end + (*end == '/'), &end, 10);
                fi = std::strtol(end + (*end == '/'), &end, 10);

                meshData.faceIndices.emplace_back(d, e, fi);
            }

            break;
        }

        case g: { // add groups
            break;
        }

        case comment: {
            break;
        }
        default: {
        }
        }
    }
    fmt::print(stderr, "finished mesh read\n");

    auto timeTaken = duration<float>(system_clock::now() - startTime).count();
    fmt::print(stderr, "time taken {}\n", timeTaken);

    return meshData;
}

// for feeding into drawArrays as seperate triangles. hard to misuses as the
// type indicates the usage
MeshDataSplit readObjSplit(const std::string& filePath) {
    auto rawMeshData = readObjRaw(filePath);

    MeshDataSplit meshData;

    meshData.vertices.resize(rawMeshData.faceIndices.size());
    if (rawMeshData.textureCoords.size() == 0) {
        rawMeshData.textureCoords.resize(rawMeshData.faceIndices.size());
    }

    if (rawMeshData.normals.size() == 0) {
        rawMeshData.normals.resize(rawMeshData.faceIndices.size());
    }

#pragma omp parallel for
    for (int i = 0u; i < rawMeshData.faceIndices.size(); ++i) {
        meshData.vertices[i] = {
            rawMeshData.positions[rawMeshData.faceIndices[i].x],
            rawMeshData.normals[rawMeshData.faceIndices[i].z],
            rawMeshData.textureCoords[rawMeshData.faceIndices[i].y]};
    }
    return meshData;
}

} // namespace objLoader
