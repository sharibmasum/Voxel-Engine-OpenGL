//
// Created by sharib masum on 2025-07-01.
//

#ifndef MINECRAFT_CHUNK_H
#define MINECRAFT_CHUNK_H

#include <vector>
#include <cstdint>
#include "gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Mesh.h"

class Chunk {
public:
    Chunk(unsigned int chunkSize);

    // Takes ownership of already block-ordered (x outer, y middle, z inner) data.
    void generateChunk(std::vector<uint8_t>&& data);
    void render();
    glm::vec3 position;
    glm::mat4 modelMatrix{1.0f}; // cached translate(position); recomputed only when position changes
    glm::ivec3 gridPos;

    void setPosition(const glm::vec3& pos);

    // CPU-only: builds greedy-merged mesh geometry into meshVertices/meshIndices. Safe to call
    // off the main thread (touches no GL state and no other Chunk's data besides neighbor block
    // lookups, which are read-only).
    void buildMeshData(Chunk* neighbors[6]);

    // GL-only: uploads whatever buildMeshData last produced. Must run on the thread owning the
    // GL context (main thread).
    void uploadMesh();

    // Convenience for same-thread (re)meshing: CPU build + immediate GPU upload.
    void buildMeshWithNeighbors(Chunk* neighbors[6]);

    bool isSolidLocal(int x, int y, int z) const;

    ~Chunk();
    bool isMeshed = false;

private:
    unsigned int chunkSize;

    std::vector<uint8_t> blocks; // flat, index = (x * chunkSize + y) * chunkSize + z
    Mesh mesh;

    std::vector<GLfloat> meshVertices;
    std::vector<unsigned int> meshIndices;

    inline int blockIndex(int x, int y, int z) const {
        return (x * static_cast<int>(chunkSize) + y) * static_cast<int>(chunkSize) + z;
    }

    bool isSolidAt(int x, int y, int z, Chunk* neighbors[6]) const;

    void emitQuad(int direction, float fixed, float a0, float a1, float b0, float b1,
                  unsigned int& vertexOffset);

    void greedySweep(int direction, Chunk* neighbors[6], unsigned int& vertexOffset);
};


#endif //MINECRAFT_CHUNK_H
