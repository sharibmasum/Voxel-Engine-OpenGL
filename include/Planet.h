//
// Created by sharib masum on 2025-07-02.
//

#ifndef MINECRAFT_PLANET_H
#define MINECRAFT_PLANET_H

#include <vector>
#include <string>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <glm/glm.hpp>
#include "Chunk.h"
#include "FastNoiseLite.h"

class Planet {
public:
    Planet();
    Planet(glm::vec3 cameraPos);

    // Streams chunks in/out around the camera; call once per frame.
    void update(float camx, float camy, float camz, unsigned int modelLoc);
    void render(GLuint modelLoc, const glm::mat4& projection, const glm::mat4& view);

    int getRenderDistance() const { return renderDistance; }
    void setRenderDistance(int distance);

    int getLoadedChunkCount() const { return static_cast<int>(chunks.size()); }
    int getPendingChunkCount() const { return static_cast<int>(pendingLoad.size()) + static_cast<int>(inFlight.size()); }
    int getCulledChunkCount() const { return lastCulledCount; }

    ~Planet();

private:
    std::vector<uint8_t> generateChunkData(int chunkX, int chunkY, int chunkZ) const;
    int terrainHeight(int worldX, int worldZ) const;

    std::string getChunkKey(int x, int y, int z) const;
    Chunk* getChunk(int x, int y, int z);
    void getNeighbors(int x, int y, int z, Chunk* neighbors[6]);
    void remesh(int x, int y, int z);

    void unloadChunk(const glm::ivec3& coord);
    void finalizeChunk(Chunk* chunk); // GPU upload + insert into map + boundary remesh (main thread)

    void rebuildStreamingQueues(int centerChunkX, int centerChunkZ);
    void processStreaming();

    void workerLoop(); // runs on background threads: generation + CPU-only mesh build

    std::unordered_map<std::string, Chunk*> chunks;

    std::vector<glm::ivec3> pendingLoad;    // farthest-first so nearest pops off the back first
    std::vector<glm::ivec3> pendingUnload;
    std::unordered_set<std::string> inFlight; // coords queued/being built by a worker thread (main-thread only)

    // Producer (main thread) -> consumer (workers): coords to generate.
    std::deque<glm::ivec3> workQueue;
    std::mutex workMutex;
    std::condition_variable workCv;

    // Producer (workers) -> consumer (main thread): fully CPU-built chunks awaiting GPU upload.
    std::deque<Chunk*> readyQueue;
    std::mutex readyMutex;

    std::vector<std::thread> workerThreads;
    std::atomic<bool> workerRunning{true};

    FastNoiseLite continentNoise;    // broad rolling terrain
    FastNoiseLite mountainMaskNoise; // low-frequency mask: where mountain ranges sit
    FastNoiseLite ridgeNoise;        // ridged-fractal peaks, blended in via the mask

    int renderDistance = 8;                  // horizontal load radius, in chunks (adjustable at runtime)
    static const int worldHeightChunks = 6;  // fixed vertical column height, in chunks
    unsigned int chunksize = 16;

    static const int maxLoadsPerFrame = 24;  // coords handed to workers per frame (generation is async now)
    static const int maxUnloadsPerFrame = 16;

    int lastCamChunkX = 0x7fffffff;
    int lastCamChunkZ = 0x7fffffff;
    int lastRenderDistance = -1;
    int lastCulledCount = 0;
};

#endif //MINECRAFT_PLANET_H
