#pragma once

#include <fmt/core.h>
#include <vector>
#include <cstdio>


#include "glm/glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


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
constexpr uint32_t f = packCharsToIntKey('f', ' ');

struct RawMeshData {

    RawMeshData() : positions(1), normals(1), textureCoords(1) {
    }
    // dummy value at 0. removes the need for subtracting 1 from obj file
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> textureCoords;
    std::vector<glm::ivec3> faceIndices;

};


RawMeshData readObjRaw(const std::string& filePath) {
    RawMeshData meshData;

    FILE* fp = fopen(filePath.c_str(), "r");
    if (!fp) {
        fmt::print(stderr, "Error opening file\n");
    }


    char line[128];
    size_t line_size;

    std::vector<int> spacePositions(8);

    char* end;
    uint32_t key;

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
                std::strtof(&line[spacePositions[0]], nullptr),
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

            int a = std::strtol(&line[spacePositions[0]], &end, 10);
            int b = std::strtol(end + (*end == '/'), &end, 10);
            int c = std::strtol(end + (*end == '/'), &end, 10);
            meshData.faceIndices.emplace_back(a, b, c);

            int d = std::strtol(&line[spacePositions[1]], &end, 10);
            int e = std::strtol(end + (*end == '/'), &end, 10);
            int f = std::strtol(end + (*end == '/'), &end, 10);

            meshData.faceIndices.emplace_back(d, e, f);

            int g = std::strtol(&line[spacePositions[2]], &end, 10);
            int h = std::strtol(end + (*end == '/'), &end, 10);
            int i = std::strtol(end + (*end == '/'), &end, 10);

            meshData.faceIndices.emplace_back(g, h, i);

            if (spacePositions.size() == 5) {
                // face 0
                meshData.faceIndices.emplace_back(a, b, c);
                // face 2
                meshData.faceIndices.emplace_back(g, h, i);

                // reuse def as those temps aren't needed
                d = std::strtol(&line[spacePositions[3]], &end, 10);
                e = std::strtol(end + (*end == '/'), &end, 10);
                f = std::strtol(end + (*end == '/'), &end, 10);

                meshData.faceIndices.emplace_back(d, e, f);
            }

            break;
        }

        default: {
        
        }
        }//end of switch-case
        
    }//end of whilefgets

    return meshData;
}//end of func


struct MeshDataSplit {
    std::vector<vertex3D> vertices;
};

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


}
