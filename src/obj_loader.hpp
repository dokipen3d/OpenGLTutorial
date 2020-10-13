#pragma once

#include "glm/glm.hpp"
#include <algorithm>
#include <cctype>
#include <chrono> // current time
#include <climits>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <omp.h>
#include <pystring.h>
#include <sstream>
#include <string>
#include <vector>

struct vertex3D {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

namespace std {

inline bool operator<(const glm::ivec3& a, const glm::ivec3& b) {
    return a.x < b.x || (a.x == b.x && (a.y < b.y || (a.y == b.y && a.z < b.z)));
}

template <> struct less<glm::ivec3> {
    bool operator()(const glm::ivec3& a, const glm::ivec3& b) {
        return a < b;
    }
};
} // namespace std

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

struct RawMeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> textureCoords;
    // store the obj face info interleaved for now. makes finding unique verts easier
    std::vector<glm::ivec3> faceIndices;
    //    std::vector<glm::ivec3> faceIndicesB;
    //    std::vector<glm::ivec3> faceIndicesC;
};

struct MeshDataSplit {
    std::vector<vertex3D> vertices;
};

struct MeshDataElements : MeshDataSplit {
    std::vector<int> indices;
};

RawMeshData readObjRaw(const std::string& filePath) {

    std::ios_base::sync_with_stdio(false);
    using namespace std::chrono;
    auto startTime = system_clock::now();

    fmt::print(stderr, "starting obj loader\n");
    RawMeshData meshData;

    std::ifstream inputFileStream(filePath);
    std::string line;
    std::string face;
    std::stringstream ss;

    std::vector<std::string> tokens;
    std::vector<std::string> faceTokensA;
    std::vector<std::string> faceTokensB;
    std::vector<std::string> faceTokensC;

    bool hasTextureCoords = false;
    bool hasNormals = false;

    while (std::getline(inputFileStream, line)) {
        pystring::split(line, tokens, " ");
        uint32_t key = packCharsToIntKey(line[0], line[1]);

        switch (key) {

        case v: {
            meshData.positions.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            break;
        }
        case vn: {
            meshData.normals.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            // hasNormals = true;
            break;
        }
        case vt: {
            meshData.textureCoords.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]));
            // hasTextureCoords = true;
            break;
        }
        case vp: {
            break;
        }
        case f: {
            // is face

            pystring::split(tokens[1], faceTokensA, "/");
            pystring::split(tokens[2], faceTokensB, "/");
            pystring::split(tokens[3], faceTokensC, "/");

            auto stoiIfNotEmpty = [](const std::string& str) -> int {
                if (!str.empty()) {
                    return std::stoi(str);
                }
                return 1;
            };

            meshData.faceIndices.emplace_back(std::stoi(faceTokensA[0]) - 1, stoiIfNotEmpty(faceTokensA[1]) - 1,
                                              stoiIfNotEmpty(faceTokensA[2]) - 1);

            meshData.faceIndices.emplace_back(std::stoi(faceTokensB[0]) - 1, stoiIfNotEmpty(faceTokensB[1]) - 1,
                                              stoiIfNotEmpty(faceTokensB[2]) - 1);

            meshData.faceIndices.emplace_back(std::stoi(faceTokensC[0]) - 1, stoiIfNotEmpty(faceTokensC[1]) - 1,
                                              stoiIfNotEmpty(faceTokensC[2]) - 1);

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

// simple reading. will add more functionality in the future for deduplication, split uvs sets etc
MeshDataElements readObj(const std::string& filePath) {
    auto rawMeshData = readObjRaw(filePath);

    MeshDataElements meshData;
    return meshData;
}

// for feeding into drawArrays as seperate triangles. hard to misuses as the type indicates the usage
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
        meshData.vertices[i] = {rawMeshData.positions[rawMeshData.faceIndices[i].x],
                                rawMeshData.normals[rawMeshData.faceIndices[i].z],
                                rawMeshData.textureCoords[rawMeshData.faceIndices[i].y]};
    }
    return meshData;
}

// for feeding into drawArrayElements
MeshDataElements readObjElements(const std::string& filePath) {
    auto rawMeshData = readObjRaw(filePath);

    MeshDataElements meshData;
    meshData.indices.reserve(rawMeshData.faceIndices.size());

#pragma omp parallel for
    for (int i = 0; i < rawMeshData.faceIndices.size(); ++i) {
        // x component is the face index
        meshData.indices[i] = rawMeshData.faceIndices[i].x;
    }

    // find unique verts. we have already copied face index so order isn't so bad any more
    std::sort(rawMeshData.faceIndices.begin(), rawMeshData.faceIndices.end());

    auto uniqueEndIt = std::unique(rawMeshData.faceIndices.begin(), rawMeshData.faceIndices.end());
    auto count = std::distance(rawMeshData.faceIndices.begin(), uniqueEndIt);
    meshData.vertices.resize(count);
    fmt::print(stderr, "unique point count is {}\n", count);

    // scatter results
#pragma omp parallel for
    for (auto i = 0; i < count; i++) {
        meshData.vertices[rawMeshData.faceIndices[i].x] = {rawMeshData.positions[rawMeshData.faceIndices[i].x],
                                                           rawMeshData.normals[rawMeshData.faceIndices[i].y],
                                                           rawMeshData.textureCoords[rawMeshData.faceIndices[i].z]};
    }

    return meshData;
}

} // namespace objLoader
