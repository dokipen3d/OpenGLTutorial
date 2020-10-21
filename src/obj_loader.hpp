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

    // for unordered_map
    bool operator==(const vertex3D& other) const {
        return position == other.position && normal == other.normal && texCoord == other.texCoord;
    }
};

inline uint64_t spread_bits_uint64(uint64_t x) {
    x = (x | (x << 32)) & 0x7fff00000000ffff;
    x = (x | (x << 16)) & 0x00ff0000ff0000ff;
    x = (x | (x << 8)) & 0x700f00f00f00f00f;
    x = (x | (x << 4)) & 0x30c30c30c30c30c3;
    x = (x | (x << 2)) & 0x1249249249249249;
    return x;
}

inline uint64_t mortonIndex64(uint32_t x, uint32_t y, uint32_t z) {
    return (spread_bits_uint64(x) | (spread_bits_uint64(y) << 1) | (spread_bits_uint64(z) << 2));
}

// for unordered_map
namespace std {
template <> struct hash<vertex3D> {
    size_t operator()(vertex3D const& vertex) const {
        return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

namespace std {

inline bool operator<(const glm::ivec3& a, const glm::ivec3& b) {
    return a.x < b.x || (a.x == b.x && (a.y < b.y || (a.y == b.y && a.z < b.z)));
    // return mortonIndex64(a.x, a.y, a.z) < mortonIndex64(b.x, b.y, b.z);
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

// add groups
constexpr uint32_t g = packCharsToIntKey('g', ' ');

// add groups
struct groupInfo {
    std::string name;
    int startOffset;
    int count;
};

struct RawMeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> textureCoords;
    // store the obj face info interleaved for now. makes finding unique verts easier
    std::vector<glm::ivec3> faceIndices;

    // add groups.
    std::vector<groupInfo> groupInfos;
};

struct MeshDataSplit {
    std::vector<vertex3D> vertices;
    // add groups.
    // an int which is the start index into the faceIndices of where groups start
    // to get the range, use the last(-1) offset
    std::vector<groupInfo> groupInfos;
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

    // if quads
    std::vector<std::string> faceTokensD;

    //for storing where spaces and slashes go
    std::vector<int> spacePositions(8);
    std::vector<int> slashPositionsA(6);
    std::vector<int> slashPositionsB(6);
    std::vector<int> slashPositionsC(6);




    bool hasTextureCoords = false;
    bool hasNormals = false;
    bool startGroupTracking = false;
    int groupCount = 0;

    while (std::getline(inputFileStream, line)) {
        spacePositions.clear();
        //spaces after the first will always be after 3
        for (auto i = 0u; i < line.size(); ++i){
            if(line[i] == ' '){
                spacePositions.push_back(i);
            }
        }
        spacePositions.push_back(line.size()-1);
        for (auto e : spacePositions){
            fmt::print("pos {}\n", e);
        }
        fmt::print("\n");

        pystring::split(line, tokens, " ");
        uint32_t key = packCharsToIntKey(line[0], line[1]);

        switch (key) {

        case v: {
            meshData.positions.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));

            break;
        }
        case vn: {
            meshData.normals.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            if (!startGroupTracking) {
                startGroupTracking = true;
            }
            break;
        }
        case vt: {
            meshData.textureCoords.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]));
            break;
        }
        case vp: {
            break;
        }
        // add groups
        case g: {
            if (startGroupTracking) {
                if (tokens.size() > 1 && !tokens[1].empty()) { // its a face group with a name
                    meshData.groupInfos.push_back({tokens[1], static_cast<int>(meshData.faceIndices.size())});
                } else {
                    meshData.groupInfos.push_back(
                        {fmt::format("group{}", ++groupCount), static_cast<int>(meshData.faceIndices.size())});
                }
            }
            break;
        }
        case f: {
            // is face

            pystring::split(tokens[1], faceTokensA, "/");
            pystring::split(tokens[2], faceTokensB, "/");
            pystring::split(tokens[3], faceTokensC, "/");

            // quad
            if (tokens.size() == 5) {
                pystring::split(tokens[4], faceTokensD, "/");
            }
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

            // quad
            if (tokens.size() == 5) {
                meshData.faceIndices.emplace_back(std::stoi(faceTokensA[0]) - 1, stoiIfNotEmpty(faceTokensA[1]) - 1,
                                                  stoiIfNotEmpty(faceTokensA[2]) - 1);

                meshData.faceIndices.emplace_back(std::stoi(faceTokensC[0]) - 1, stoiIfNotEmpty(faceTokensC[1]) - 1,
                                                  stoiIfNotEmpty(faceTokensC[2]) - 1);

                meshData.faceIndices.emplace_back(std::stoi(faceTokensD[0]) - 1, stoiIfNotEmpty(faceTokensD[1]) - 1,
                                                  stoiIfNotEmpty(faceTokensD[2]) - 1);
            }

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
    for (auto it = meshData.groupInfos.begin(); it != meshData.groupInfos.end() - 1; ++it) {
        (*it).count = (*std::next(it)).startOffset - (*it).startOffset;
    }
    meshData.groupInfos.back().count =
        static_cast<int>(meshData.faceIndices.size() - meshData.groupInfos.back().startOffset);

    fmt::print(stderr, "finished mesh read\n");

    auto timeTaken = duration<float>(system_clock::now() - startTime).count();
    fmt::print(stderr, "time taken {}\n", timeTaken);

    return meshData;
}

// for feeding into drawArrays as seperate triangles. hard to misuses as the type indicates the usage
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
        meshData.vertices[i] = {rawMeshData.positions[rawMeshData.faceIndices[i].x],
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

    auto rightshift_func = [&rawMeshData](const int& x) {
        return mortonIndex64(rawMeshData.faceIndices[x].x, rawMeshData.faceIndices[x].y, rawMeshData.faceIndices[x].z);
    };

    //    auto rightshift_func = [&rawMeshData](const uint64_t& x, const uint64_t offset) {
    //        return x >>
    //               offset;
    //    };

    auto compare_func = [&rawMeshData](const int a, const int b) {
        return rawMeshData.faceIndices[a] < rawMeshData.faceIndices[b];
    };
    //    boost::sort::spreadsort::integer_sort(trackingIds.begin(), trackingIds.end(), rightshift_func, compare_func);

    // ska_sort(trackingIds.begin(), trackingIds.end(), rightshift_func);
    // find unique verts. we have already copied face index so order isn't so important any more
    // std::sort(rawMeshData.faceIndices.begin(), rawMeshData.faceIndices.end());

    std::sort(trackingIds.begin(), trackingIds.end(),
              [&rawMeshData](int a, int b) { return rawMeshData.faceIndices[a] < rawMeshData.faceIndices[b]; });
    //boost::sort::parallel_stable_sort(trackingIds.begin(), trackingIds.end(), compare_func);
    auto uniqueEndIt =
        std::unique(trackingUniqueIds.begin(), trackingUniqueIds.end(), [&trackingIds, &rawMeshData](int a, int b) {
            return rawMeshData.faceIndices[trackingIds[a]] == rawMeshData.faceIndices[trackingIds[b]];
        });

    // trackingUniqueIds.erase(uniqueEndIt, trackingUniqueIds.end());

    // how many unique vertices
    auto count = std::distance(trackingUniqueIds.begin(), uniqueEndIt);

    meshData.vertices.resize(count);
    fmt::print(stderr, "pos size {}\n", rawMeshData.positions.size());
    fmt::print(stderr, "norm size {}\n", rawMeshData.normals.size());
    fmt::print(stderr, "tc size {}\n", rawMeshData.textureCoords.size());

    fmt::print(stderr, "unique point count is {}\n", count);

    if (rawMeshData.textureCoords.size() == 0) {
        rawMeshData.textureCoords.resize(rawMeshData.faceIndices.size());
    }

    if (rawMeshData.normals.size() == 0) {
        rawMeshData.normals.resize(rawMeshData.faceIndices.size());
    }

    std::atomic<int> k = 0;
#pragma omp parallel for
    for (auto i = 1ll; i <= count; ++i) {

        meshData.vertices[i - 1] = {
            rawMeshData.positions[rawMeshData.faceIndices[trackingIds[trackingUniqueIds[i - 1]]].x],
            rawMeshData.normals[rawMeshData.faceIndices[trackingIds[trackingUniqueIds[i - 1]]].z],
            rawMeshData.textureCoords[rawMeshData.faceIndices[trackingIds[trackingUniqueIds[i - 1]]].y]};

        // all the indices that reference this vertex
        for (auto j = trackingUniqueIds[i - 1]; j < trackingUniqueIds[i]; ++j, k++) {
            meshData.indices[trackingIds[j]] = i - 1;
        }
    }
    for (auto j = trackingUniqueIds[count - 1]; j < rawMeshData.faceIndices.size(); ++j) {
        // meshData.indices[trackingIds[j]] = i - 1;
        // fmt::print(stderr, "first index is  {}\n", trackingIds[j]);
        meshData.indices[trackingIds[j]] = count - 1;
    }
    // fmt::print(stderr, "first index is  {}\n", trackingIds[k]);

    fmt::print(stderr, "back j {}\n", trackingUniqueIds[count - 1]);

    fmt::print(stderr, "max k {}\n", k);

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

        vertex3D vertex = {rawMeshData.positions[rawMeshData.faceIndices[i].x],
                           rawMeshData.normals[rawMeshData.faceIndices[i].z],
                           rawMeshData.textureCoords[rawMeshData.faceIndices[i].y]};

        if (uniqueVertices.count(vertex) == 0) {
            uniqueVertices[vertex] = static_cast<uint32_t>(meshData.vertices.size());
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
