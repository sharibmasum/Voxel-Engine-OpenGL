//
// Created by sharib masum on 2025-07-01.
//
#include "Chunk.h"
#include <algorithm>

namespace {
    constexpr float kOneThird = 1.0f / 3.0f;
    constexpr float kTwoThirds = 2.0f / 3.0f;

    struct GreedyRect {
        int a0, b0, w, h;
    };

    // Greedily merges a W x H boolean mask (indexed mask[a*H+b]) into maximal rectangles.
    // Mutates mask, clearing cells as they're consumed.
    void greedyMergeMask(std::vector<uint8_t>& mask, int W, int H, std::vector<GreedyRect>& out) {
        for (int a = 0; a < W; ++a) {
            for (int b = 0; b < H; ++b) {
                if (!mask[a * H + b]) continue;

                int w = 1;
                while (a + w < W && mask[(a + w) * H + b]) ++w;

                int h = 1;
                bool canExtend = true;
                while (canExtend && b + h < H) {
                    for (int k = 0; k < w; ++k) {
                        if (!mask[(a + k) * H + (b + h)]) { canExtend = false; break; }
                    }
                    if (canExtend) ++h;
                }

                for (int da = 0; da < w; ++da)
                    for (int db = 0; db < h; ++db)
                        mask[(a + da) * H + (b + db)] = 0;

                out.push_back({a, b, w, h});
            }
        }
    }
}

Chunk::Chunk(unsigned int chunkSize) {
    this->chunkSize = chunkSize;
}

void Chunk::generateChunk(std::vector<uint8_t>&& data) {
    blocks = std::move(data);
}

void Chunk::setPosition(const glm::vec3& pos) {
    position = pos;
    modelMatrix = glm::translate(glm::mat4(1.0f), pos);
}

bool Chunk::isSolidLocal(int x, int y, int z) const {
    int N = static_cast<int>(chunkSize);
    if (x < 0 || x >= N || y < 0 || y >= N || z < 0 || z >= N) return false;
    return blocks[blockIndex(x, y, z)] != 0;
}

bool Chunk::isSolidAt(int x, int y, int z, Chunk* neighbors[6]) const {
    int N = static_cast<int>(chunkSize);
    if (x >= 0 && x < N && y >= 0 && y < N && z >= 0 && z < N) {
        return blocks[blockIndex(x, y, z)] != 0;
    }

    // Exactly one axis is out of range by one step at a time (greedySweep only ever offsets
    // a single axis), so these checks are mutually exclusive.
    if (x < 0)  return neighbors[4] && neighbors[4]->isSolidLocal(N - 1, y, z); // WEST
    if (x >= N) return neighbors[1] && neighbors[1]->isSolidLocal(0, y, z);     // EAST
    if (y < 0)  return neighbors[5] && neighbors[5]->isSolidLocal(x, N - 1, z); // DOWN
    if (y >= N) return neighbors[2] && neighbors[2]->isSolidLocal(x, 0, z);     // UP
    if (z < 0)  return neighbors[0] && neighbors[0]->isSolidLocal(x, y, N - 1); // NORTH
    if (z >= N) return neighbors[3] && neighbors[3]->isSolidLocal(x, y, 0);     // SOUTH

    return false;
}

// direction indices match the project-wide convention:
// 0 FRONT(-Z) 1 RIGHT(+X) 2 TOP(+Y) 3 BACK(+Z) 4 LEFT(-X) 5 BOTTOM(-Y)
void Chunk::emitQuad(int direction, float fixed, float a0, float a1, float b0, float b1,
                      unsigned int& vertexOffset) {
    struct Corner { glm::vec3 pos; glm::vec2 uv; };
    Corner c[4];
    glm::vec3 normal;
    float uMin, uMax;

    float uLen = a1 - a0;
    float vLen = b1 - b0;

    switch (direction) {
        case 0: // FRONT (-Z)
            normal = glm::vec3(0, 0, -1);
            uMin = 0.0f; uMax = kOneThird;
            c[0] = {{a0, b0, fixed}, {0.0f, 0.0f}};
            c[1] = {{a1, b0, fixed}, {uLen, 0.0f}};
            c[2] = {{a0, b1, fixed}, {0.0f, vLen}};
            c[3] = {{a1, b1, fixed}, {uLen, vLen}};
            break;
        case 1: // RIGHT (+X)
            normal = glm::vec3(1, 0, 0);
            uMin = 0.0f; uMax = kOneThird;
            c[0] = {{fixed, b0, a0}, {0.0f, 0.0f}};
            c[1] = {{fixed, b0, a1}, {uLen, 0.0f}};
            c[2] = {{fixed, b1, a0}, {0.0f, vLen}};
            c[3] = {{fixed, b1, a1}, {uLen, vLen}};
            break;
        case 2: // TOP (+Y)
            normal = glm::vec3(0, 1, 0);
            uMin = kTwoThirds; uMax = 1.0f;
            c[0] = {{a0, fixed, b0}, {0.0f, 0.0f}};
            c[1] = {{a1, fixed, b0}, {uLen, 0.0f}};
            c[2] = {{a0, fixed, b1}, {0.0f, vLen}};
            c[3] = {{a1, fixed, b1}, {uLen, vLen}};
            break;
        case 3: // BACK (+Z)
            normal = glm::vec3(0, 0, 1);
            uMin = 0.0f; uMax = kOneThird;
            c[0] = {{a1, b0, fixed}, {0.0f, 0.0f}};
            c[1] = {{a0, b0, fixed}, {uLen, 0.0f}};
            c[2] = {{a1, b1, fixed}, {0.0f, vLen}};
            c[3] = {{a0, b1, fixed}, {uLen, vLen}};
            break;
        case 4: // LEFT (-X)
            normal = glm::vec3(-1, 0, 0);
            uMin = 0.0f; uMax = kOneThird;
            c[0] = {{fixed, b0, a1}, {0.0f, 0.0f}};
            c[1] = {{fixed, b0, a0}, {uLen, 0.0f}};
            c[2] = {{fixed, b1, a1}, {0.0f, vLen}};
            c[3] = {{fixed, b1, a0}, {uLen, vLen}};
            break;
        default: // 5: BOTTOM (-Y)
            normal = glm::vec3(0, -1, 0);
            uMin = kOneThird; uMax = kTwoThirds;
            c[0] = {{a0, fixed, b1}, {0.0f, 0.0f}};
            c[1] = {{a1, fixed, b1}, {uLen, 0.0f}};
            c[2] = {{a0, fixed, b0}, {0.0f, vLen}};
            c[3] = {{a1, fixed, b0}, {uLen, vLen}};
            break;
    }

    for (const auto& corner : c) {
        meshVertices.insert(meshVertices.end(), {
                corner.pos.x, corner.pos.y, corner.pos.z,
                corner.uv.x, corner.uv.y,
                normal.x, normal.y, normal.z,
                uMin, uMax
        });
    }

    meshIndices.insert(meshIndices.end(), {
            vertexOffset + 0, vertexOffset + 2, vertexOffset + 3,
            vertexOffset + 3, vertexOffset + 1, vertexOffset + 0
    });

    vertexOffset += 4;
}

void Chunk::greedySweep(int direction, Chunk* neighbors[6], unsigned int& vertexOffset) {
    int N = static_cast<int>(chunkSize);
    std::vector<uint8_t> mask(static_cast<size_t>(N) * static_cast<size_t>(N));
    std::vector<GreedyRect> rects;

    for (int layer = 0; layer < N; ++layer) {
        std::fill(mask.begin(), mask.end(), 0);
        bool any = false;

        for (int a = 0; a < N; ++a) {
            for (int b = 0; b < N; ++b) {
                int x, y, z;
                switch (direction) {
                    case 0: case 3: x = a; y = b; z = layer; break;     // FRONT/BACK
                    case 1: case 4: x = layer; y = b; z = a; break;     // RIGHT/LEFT
                    default:        x = a; y = layer; z = b; break;    // TOP/BOTTOM
                }

                int nx = x, ny = y, nz = z;
                switch (direction) {
                    case 0: nz -= 1; break; // FRONT:  exposed toward -Z
                    case 3: nz += 1; break; // BACK:   exposed toward +Z
                    case 1: nx += 1; break; // RIGHT:  exposed toward +X
                    case 4: nx -= 1; break; // LEFT:   exposed toward -X
                    case 2: ny += 1; break; // TOP:    exposed toward +Y
                    default: ny -= 1; break; // BOTTOM: exposed toward -Y
                }

                if (isSolidAt(x, y, z, neighbors) && !isSolidAt(nx, ny, nz, neighbors)) {
                    mask[a * N + b] = 1;
                    any = true;
                }
            }
        }

        if (!any) continue;

        rects.clear();
        greedyMergeMask(mask, N, N, rects);

        float fixed;
        switch (direction) {
            case 0: fixed = static_cast<float>(layer); break;      // FRONT: min boundary
            case 3: fixed = static_cast<float>(layer + 1); break;  // BACK: max boundary
            case 1: fixed = static_cast<float>(layer + 1); break;  // RIGHT: max boundary
            case 4: fixed = static_cast<float>(layer); break;      // LEFT: min boundary
            case 2: fixed = static_cast<float>(layer + 1); break;  // TOP: max boundary
            default: fixed = static_cast<float>(layer); break;     // BOTTOM: min boundary
        }

        for (auto& r : rects) {
            emitQuad(direction, fixed,
                     static_cast<float>(r.a0), static_cast<float>(r.a0 + r.w),
                     static_cast<float>(r.b0), static_cast<float>(r.b0 + r.h),
                     vertexOffset);
        }
    }
}

void Chunk::buildMeshData(Chunk* neighbors[6]) {
    meshVertices.clear();
    meshIndices.clear();

    unsigned int vertexOffset = 0;
    for (int direction = 0; direction < 6; ++direction) {
        greedySweep(direction, neighbors, vertexOffset);
    }
}

void Chunk::uploadMesh() {
    mesh.createMesh(meshVertices.data(), meshIndices.data(),
                     static_cast<unsigned int>(meshVertices.size()),
                     static_cast<unsigned int>(meshIndices.size()));
    isMeshed = true;
}

void Chunk::buildMeshWithNeighbors(Chunk* neighbors[6]) {
    buildMeshData(neighbors);
    uploadMesh();
}

void Chunk::render() {
    mesh.renderMesh();
}

Chunk::~Chunk() {
    mesh.clearMesh();
}
