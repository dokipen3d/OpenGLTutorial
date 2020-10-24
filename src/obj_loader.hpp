#pragma once

#include "glm/glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <algorithm>
#include <numeric>

//#include "ska_sort.hpp"
//#include <boost/sort/sort.hpp>
//#include <boost/sort/spreadsort/integer_sort.hpp>
#include <cctype>
#include <chrono> // current time
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fstream>
//#include <iostream>
#include <omp.h>
//#include <pystring.h>
//#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
struct vertex3D {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;

    // for unordered_map
    bool operator==(const vertex3D& other) const {
        return position == other.position && normal == other.normal &&
               texCoord == other.texCoord;
    }
};


namespace std {

inline bool operator<(const glm::ivec3& a, const glm::ivec3& b) {
    return a.x < b.x ||
           (a.x == b.x && (a.y < b.y || (a.y == b.y && a.z < b.z)));
}

template <> struct less<glm::ivec3> {
    bool operator()(const glm::ivec3& a, const glm::ivec3& b) {
        return a < b;
    }
};

template <> struct hash<vertex3D> {
    size_t operator()(vertex3D const& vertex) const {
        return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1);
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
constexpr uint32_t material = packCharsToIntKey('u', 's');

// add groups
constexpr uint32_t g = packCharsToIntKey('g', ' ');

// add groups
struct groupInfo {
    std::string name;
    int startOffset;
    int count;
};

struct RawMeshData {

    RawMeshData() : positions(1), normals(1), textureCoords(1) {
    }
    // dummy value at 0. removes the need for subtracting 1 from obj file
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> textureCoords;
    // store the obj face info interleaved for now. makes finding unique verts
    // easier
    std::vector<glm::ivec3> faceIndices;

    // add groups.
    std::vector<groupInfo> groupInfos;
};

struct MeshDataSplit {
    std::vector<vertex3D> vertices;
    // add groups.
    // an int which is the start index into the faceIndices of where groups
    // start to get the range, use the last(-1) offset
    std::vector<groupInfo> groupInfos;
};

struct MeshDataElements : MeshDataSplit {
    std::vector<int> indices;
};

// only supports tris and quads
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

    std::string face;

    // for storing where spaces and slashes go
    std::vector<int> spacePositions(8);

    char* end;
    int startPos = 0;

    bool hasTextureCoords = false;
    bool hasNormals = false;
    bool startGroupTracking = false;
    int groupCount = 0;
    bool groupJustAdded = false;
    uint16_t key;

    while (fgets(line, 128, fp)) {

        { // setup
            line_size = strlen(line);
            spacePositions.clear();
            key = packCharsToIntKey(line[0], line[1]);
        }

        // remove last group if it wasn't a face group
        if (groupJustAdded && !(key == f || key == material)) {
            meshData.groupInfos.pop_back();
        }
        groupJustAdded = false;
        {
            // spaces after the first will always be after 3
            for (auto i = 0u; i < line_size; ++i) {
                if (line[i] == ' ') {
                    line[i] = '\0';
                    spacePositions.push_back(i + 1);
                }
            }
            // signify end of line
            spacePositions.push_back(line_size);
        }

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
            if (!startGroupTracking) {
                startGroupTracking = true;
            }
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

        case g: {                // add groups
            if (line_size > 3) { // its a face group with a name as 'g' 'space'
                                 // '\n' is 3 characters
                size_t pos = spacePositions[0];
                size_t count = spacePositions[1] - spacePositions[0];
                meshData.groupInfos.push_back(
                    {{&line[pos], count - 1},
                     static_cast<int>(meshData.faceIndices.size())});

            } else {
                meshData.groupInfos.push_back(
                    {fmt::format("group{}", ++groupCount),
                     static_cast<int>(meshData.faceIndices.size())});
            }
            groupJustAdded = true;
            break;
        }

        case comment: {
            break;
        }
        default: {
        }
        }
    }

    // fix up groups
    for (auto it = meshData.groupInfos.begin();
         it != meshData.groupInfos.end() - 1; ++it) {
        (*it).count = (*std::next(it)).startOffset - (*it).startOffset;
    }
    meshData.groupInfos.back().count = static_cast<int>(
        meshData.faceIndices.size() - meshData.groupInfos.back().startOffset);

    fmt::print(stderr, "finished mesh read\n");

    auto timeTaken = duration<float>(system_clock::now() - startTime).count();
    fmt::print(stderr, "doki obj time took {}\n", timeTaken);

    return meshData;
}

// for feeding into drawArrays as seperate triangles. hard to misuses as the
// type indicates the usage
MeshDataSplit readObjSplit(const std::string& filePath) {
    auto rawMeshData = readObjRaw(filePath);

    MeshDataSplit meshData;
    meshData.groupInfos = std::move(rawMeshData.groupInfos);

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

// for feeding into drawArrayElements
MeshDataElements readObjElements(const std::string& filePath) {
    using namespace std::chrono;

    auto rawMeshData = readObjRaw(filePath);
    auto startTime = system_clock::now();

    MeshDataElements meshData;

    // add groups
    meshData.groupInfos = std::move(rawMeshData.groupInfos);
    meshData.indices.resize(rawMeshData.faceIndices.size());
    std::vector<int> trackingIds(rawMeshData.faceIndices.size());

    // for building offsets of unique ranges
    std::vector<int> trackingUniqueIds(rawMeshData.faceIndices.size());
    std::iota(trackingIds.begin(), trackingIds.end(), 0);
    std::iota(trackingUniqueIds.begin(), trackingUniqueIds.end(), 0);

    std::sort(
        trackingIds.begin(), trackingIds.end(), [&rawMeshData](int a, int b) {
            return rawMeshData.faceIndices[a] < rawMeshData.faceIndices[b];
        });

    auto uniqueEndIt =
        std::unique(trackingUniqueIds.begin(), trackingUniqueIds.end(),
                    [&trackingIds, &rawMeshData](int a, int b) {
                        return rawMeshData.faceIndices[trackingIds[a]] ==
                               rawMeshData.faceIndices[trackingIds[b]];
                    });

    // how many unique vertices
    auto count = std::distance(trackingUniqueIds.begin(), uniqueEndIt);

    meshData.vertices.resize(count);

    fmt::print(stderr, "unique point count is {}\n", count);

    std::atomic<int> k = 0;
#pragma omp parallel for
    for (auto i = 1ll; i <= count; ++i) {

        meshData.vertices[i - 1] = {
            rawMeshData.positions
                [rawMeshData.faceIndices[trackingIds[trackingUniqueIds[i - 1]]]
                     .x],
            rawMeshData
                .normals[rawMeshData
                             .faceIndices[trackingIds[trackingUniqueIds[i - 1]]]
                             .z],
            rawMeshData.textureCoords
                [rawMeshData.faceIndices[trackingIds[trackingUniqueIds[i - 1]]]
                     .y]};

        // all the indices that reference this vertex
        for (auto j = trackingUniqueIds[i - 1]; j < trackingUniqueIds[i];
             ++j, k++) {
            meshData.indices[trackingIds[j]] = i - 1;
        }
    }
    for (auto j = trackingUniqueIds[count - 1];
         j < rawMeshData.faceIndices.size(); ++j) {
        meshData.indices[trackingIds[j]] = count - 1;
    }

    auto timeTaken = duration<float>(system_clock::now() - startTime).count();
    fmt::print(stderr, "indexing time taken {}\n", timeTaken);

    return meshData;
}

MeshDataElements readObjElementsMap(const std::string& filePath) {
    using namespace std::chrono;

    auto rawMeshData = readObjRaw(filePath);
    auto startTime = system_clock::now();

    MeshDataElements meshData;
    std::unordered_map<vertex3D, uint32_t> uniqueVertices;
    // add groups
    meshData.groupInfos = std::move(rawMeshData.groupInfos);

    //#pragma omp parallel for
    for (auto i = 0u; i < rawMeshData.faceIndices.size(); ++i) {

        vertex3D vertex = {
            rawMeshData.positions[rawMeshData.faceIndices[i].x],
            rawMeshData.normals[rawMeshData.faceIndices[i].z],
            rawMeshData.textureCoords[rawMeshData.faceIndices[i].y]};

        if (uniqueVertices.count(vertex) == 0) {
            uniqueVertices[vertex] =
                static_cast<uint32_t>(meshData.vertices.size());
            meshData.vertices.push_back(vertex);
        }

        meshData.indices.push_back(uniqueVertices[vertex]);
    }

    fmt::print(stderr, "total unique count {}\n", uniqueVertices.size());

    auto timeTaken = duration<float>(system_clock::now() - startTime).count();
    fmt::print(stderr, "indexing map time taken {}\n", timeTaken);

    return meshData;
}

} // namespace objLoader
