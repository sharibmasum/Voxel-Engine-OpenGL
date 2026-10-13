//
// Created by sharib masum on 2025-07-02.
//

#include "Planet.h"
#include "Chunk.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <chrono>

namespace {
    const glm::ivec3 kNeighborDirs[6] = {
            {0, 0, -1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, -1, 0}
    };

    struct Frustum {
        glm::vec4 planes[6]; // left, right, bottom, top, near, far (unnormalized is fine, sign test only)
    };

    // Gribb/Hartmann plane extraction from a combined projection*view matrix.
    Frustum extractFrustum(const glm::mat4& vp) {
        Frustum f{};
        f.planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
        f.planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
        f.planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
        f.planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
        f.planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
        f.planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
        return f;
    }

    bool aabbInFrustum(const Frustum& f, const glm::vec3& mn, const glm::vec3& mx) {
        for (const auto& p : f.planes) {
            glm::vec3 positive(p.x >= 0 ? mx.x : mn.x, p.y >= 0 ? mx.y : mn.y, p.z >= 0 ? mx.z : mn.z);
            if (p.x * positive.x + p.y * positive.y + p.z * positive.z + p.w < 0.0f) return false;
        }
        return true;
    }
}

Planet::Planet(glm::vec3 cameraPos) {
    // Broad, gentle rolling terrain everywhere.
    continentNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    continentNoise.SetSeed(1337);
    continentNoise.SetFrequency(0.008f);
    continentNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    continentNoise.SetFractalOctaves(5);
    continentNoise.SetFractalLacunarity(2.0f);
    continentNoise.SetFractalGain(0.5f);

    // Very low frequency: carves out where mountain ranges exist vs. plains.
    mountainMaskNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    mountainMaskNoise.SetSeed(9001);
    mountainMaskNoise.SetFrequency(0.002f);
    mountainMaskNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    mountainMaskNoise.SetFractalOctaves(3);
    mountainMaskNoise.SetFractalLacunarity(2.0f);
    mountainMaskNoise.SetFractalGain(0.5f);

    // Ridged fractal gives sharp peaks/ridgelines instead of smooth bumps.
    ridgeNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    ridgeNoise.SetSeed(4242);
    ridgeNoise.SetFrequency(0.01f);
    ridgeNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    ridgeNoise.SetFractalOctaves(5);
    ridgeNoise.SetFractalLacunarity(2.1f);
    ridgeNoise.SetFractalGain(0.5f);

    unsigned int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 2;
    threadCount = std::max(1u, threadCount - 1); // leave a core for the main/render thread
    workerThreads.reserve(threadCount);
    for (unsigned int i = 0; i < threadCount; ++i) {
        workerThreads.emplace_back(&Planet::workerLoop, this);
    }

    // First update() call always rebuilds the queues (lastCamChunk* start out of range).
    // Drain them fully so the world around spawn is complete before the first frame renders;
    // later calls from the main loop only trickle a budget of work in per frame.
    update(cameraPos.x, cameraPos.y, cameraPos.z, 0);
    while (!pendingLoad.empty() || !pendingUnload.empty() || !inFlight.empty()) {
        processStreaming();
        if (!inFlight.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int Planet::terrainHeight(int worldX, int worldZ) const {
    float fx = static_cast<float>(worldX);
    float fz = static_cast<float>(worldZ);

    float base01 = (continentNoise.GetNoise(fx, fz) + 1.0f) * 0.5f;

    float mask01 = (mountainMaskNoise.GetNoise(fx, fz) + 1.0f) * 0.5f;
    float mountainBlend = glm::smoothstep(0.5f, 0.75f, mask01); // 0 in plains, 1 deep in mountain ranges

    float ridge01 = (ridgeNoise.GetNoise(fx, fz) + 1.0f) * 0.5f;
    ridge01 *= ridge01; // sharpen peaks, keep valleys low

    const int baseHeight = 18;
    const int rollingAmplitude = 14;
    const int mountainAmplitude = 56;

    float heightF = baseHeight
                  + base01 * rollingAmplitude
                  + mountainBlend * ridge01 * mountainAmplitude;

    int height = static_cast<int>(heightF);
    int worldTop = static_cast<int>(worldHeightChunks * chunksize) - 2;
    return std::max(1, std::min(height, worldTop));
}

std::vector<uint8_t> Planet::generateChunkData(int chunkX, int chunkY, int chunkZ) const {
    std::vector<uint8_t> chunkData;
    chunkData.reserve(static_cast<size_t>(chunksize) * chunksize * chunksize);

    // Height depends only on world X/Z, so every vertical chunk in a column shares one height map.
    std::vector<int> heightMap(static_cast<size_t>(chunksize) * chunksize); // index x*chunksize+z
    for (unsigned int x = 0; x < chunksize; x++) {
        for (unsigned int z = 0; z < chunksize; z++) {
            int worldX = chunkX * static_cast<int>(chunksize) + static_cast<int>(x);
            int worldZ = chunkZ * static_cast<int>(chunksize) + static_cast<int>(z);
            heightMap[x * chunksize + z] = terrainHeight(worldX, worldZ);
        }
    }

    // Must match Chunk's flat index order: x outer, y middle, z inner.
    for (unsigned int x = 0; x < chunksize; x++) {
        for (unsigned int y = 0; y < chunksize; y++) {
            int worldY = chunkY * static_cast<int>(chunksize) + static_cast<int>(y);
            for (unsigned int z = 0; z < chunksize; z++) {
                chunkData.push_back(worldY <= heightMap[x * chunksize + z] ? 1 : 0);
            }
        }
    }

    return chunkData;
}

std::string Planet::getChunkKey(int x, int y, int z) const {
    return std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z);
}

Chunk* Planet::getChunk(int x, int y, int z) {
    auto it = chunks.find(getChunkKey(x, y, z));
    if (it != chunks.end()) return it->second;
    return nullptr;
}

void Planet::getNeighbors(int x, int y, int z, Chunk* neighbors[6]) {
    neighbors[0] = getChunk(x, y, z - 1); // NORTH
    neighbors[1] = getChunk(x + 1, y, z); // EAST
    neighbors[2] = getChunk(x, y + 1, z); // UP
    neighbors[3] = getChunk(x, y, z + 1); // SOUTH
    neighbors[4] = getChunk(x - 1, y, z); // WEST
    neighbors[5] = getChunk(x, y - 1, z); // DOWN
}

void Planet::remesh(int x, int y, int z) {
    Chunk* chunk = getChunk(x, y, z);
    if (!chunk) return;

    Chunk* neighbors[6];
    getNeighbors(x, y, z, neighbors);
    chunk->buildMeshWithNeighbors(neighbors);
}

// Runs on a worker thread. Touches only: the (read-only, const) noise generators, and its own
// freshly-allocated Chunk — never the shared `chunks` map or any GL call. Boundary faces toward
// chunks that aren't loaded yet are intentionally over-drawn here and corrected by finalizeChunk's
// remesh() pass on the main thread once neighbors are known, exactly like the old synchronous path.
void Planet::workerLoop() {
    Chunk* noNeighbors[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    while (workerRunning) {
        glm::ivec3 coord;
        {
            std::unique_lock<std::mutex> lock(workMutex);
            workCv.wait(lock, [this] { return !workQueue.empty() || !workerRunning; });
            if (!workerRunning) return;
            coord = workQueue.front();
            workQueue.pop_front();
        }

        Chunk* chunk = new Chunk(chunksize);
        chunk->gridPos = coord;
        chunk->setPosition(glm::vec3(coord) * static_cast<float>(chunksize));

        chunk->generateChunk(generateChunkData(coord.x, coord.y, coord.z));
        chunk->buildMeshData(noNeighbors);

        std::lock_guard<std::mutex> lock(readyMutex);
        readyQueue.push_back(chunk);
    }
}

void Planet::finalizeChunk(Chunk* chunk) {
    chunk->uploadMesh();
    chunks.emplace(getChunkKey(chunk->gridPos.x, chunk->gridPos.y, chunk->gridPos.z), chunk);

    remesh(chunk->gridPos.x, chunk->gridPos.y, chunk->gridPos.z);
    for (const auto& d : kNeighborDirs) {
        remesh(chunk->gridPos.x + d.x, chunk->gridPos.y + d.y, chunk->gridPos.z + d.z);
    }
}

void Planet::unloadChunk(const glm::ivec3& coord) {
    std::string key = getChunkKey(coord.x, coord.y, coord.z);
    auto it = chunks.find(key);
    if (it == chunks.end()) return;

    delete it->second;
    chunks.erase(it);

    for (const auto& d : kNeighborDirs) {
        remesh(coord.x + d.x, coord.y + d.y, coord.z + d.z);
    }
}

void Planet::rebuildStreamingQueues(int centerChunkX, int centerChunkZ) {
    pendingLoad.clear();
    pendingUnload.clear();

    for (int cx = centerChunkX - renderDistance; cx <= centerChunkX + renderDistance; ++cx) {
        for (int cz = centerChunkZ - renderDistance; cz <= centerChunkZ + renderDistance; ++cz) {
            for (int cy = 0; cy < worldHeightChunks; ++cy) {
                std::string key = getChunkKey(cx, cy, cz);
                if (chunks.find(key) == chunks.end() && inFlight.find(key) == inFlight.end()) {
                    pendingLoad.push_back(glm::ivec3(cx, cy, cz));
                }
            }
        }
    }

    // Farthest first so the nearest column is the one popped (from the back) first,
    // making the world fill in outward from the player.
    std::sort(pendingLoad.begin(), pendingLoad.end(),
              [centerChunkX, centerChunkZ](const glm::ivec3& a, const glm::ivec3& b) {
                  int da = (a.x - centerChunkX) * (a.x - centerChunkX) + (a.z - centerChunkZ) * (a.z - centerChunkZ);
                  int db = (b.x - centerChunkX) * (b.x - centerChunkX) + (b.z - centerChunkZ) * (b.z - centerChunkZ);
                  return da > db;
              });

    for (auto& pair : chunks) {
        const glm::ivec3& g = pair.second->gridPos;
        int dist = std::max(std::abs(g.x - centerChunkX), std::abs(g.z - centerChunkZ));
        if (dist > renderDistance) {
            pendingUnload.push_back(g);
        }
    }
}

void Planet::processStreaming() {
    // Feed a budget of pending coords to the worker pool.
    if (!pendingLoad.empty()) {
        std::vector<glm::ivec3> batch;
        while (static_cast<int>(batch.size()) < maxLoadsPerFrame && !pendingLoad.empty()) {
            glm::ivec3 coord = pendingLoad.back();
            pendingLoad.pop_back();
            inFlight.insert(getChunkKey(coord.x, coord.y, coord.z));
            batch.push_back(coord);
        }
        if (!batch.empty()) {
            {
                std::lock_guard<std::mutex> lock(workMutex);
                for (auto& c : batch) workQueue.push_back(c);
            }
            workCv.notify_all();
        }
    }

    // Drain whatever the workers finished since the last frame and upload it to the GPU.
    std::deque<Chunk*> ready;
    {
        std::lock_guard<std::mutex> lock(readyMutex);
        ready.swap(readyQueue);
    }
    for (Chunk* chunk : ready) {
        inFlight.erase(getChunkKey(chunk->gridPos.x, chunk->gridPos.y, chunk->gridPos.z));
        finalizeChunk(chunk);
    }

    for (int i = 0; i < maxUnloadsPerFrame && !pendingUnload.empty(); ++i) {
        glm::ivec3 coord = pendingUnload.back();
        pendingUnload.pop_back();
        unloadChunk(coord);
    }
}

void Planet::setRenderDistance(int distance) {
    renderDistance = std::max(1, std::min(distance, 64));
}

void Planet::update(float camx, float camy, float camz, unsigned int modelLoc) {
    (void) modelLoc;
    (void) camy; // world columns always load their full fixed height, independent of camera altitude

    int camChunkX = static_cast<int>(std::floor(camx / static_cast<float>(chunksize)));
    int camChunkZ = static_cast<int>(std::floor(camz / static_cast<float>(chunksize)));

    if (camChunkX != lastCamChunkX || camChunkZ != lastCamChunkZ || renderDistance != lastRenderDistance) {
        lastCamChunkX = camChunkX;
        lastCamChunkZ = camChunkZ;
        lastRenderDistance = renderDistance;
        rebuildStreamingQueues(camChunkX, camChunkZ);
    }

    processStreaming();
}

void Planet::render(GLuint modelLoc, const glm::mat4& projection, const glm::mat4& view) {
    Frustum frustum = extractFrustum(projection * view);
    float size = static_cast<float>(chunksize);
    int culled = 0;

    for (auto& pair : chunks) {
        Chunk* chunk = pair.second;

        glm::vec3 mn = chunk->position;
        glm::vec3 mx = chunk->position + glm::vec3(size, size, size);
        if (!aabbInFrustum(frustum, mn, mx)) {
            ++culled;
            continue;
        }

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(chunk->modelMatrix));
        chunk->render();
    }

    lastCulledCount = culled;
}

Planet::~Planet() {
    {
        // Must flip the flag under the same mutex the workers wait on, otherwise a worker that
        // just finished its predicate check (saw running==true, queue empty) and a notify_all()
        // landing in the gap before it actually goes to sleep would miss the wakeup entirely and
        // block join() forever.
        std::lock_guard<std::mutex> lock(workMutex);
        workerRunning = false;
    }
    workCv.notify_all();
    for (auto& t : workerThreads) {
        if (t.joinable()) t.join();
    }

    for (Chunk* chunk : readyQueue) {
        delete chunk; // generated but never uploaded; no GL objects to free
    }
    readyQueue.clear();

    for (auto& pair : chunks) {
        delete pair.second;
    }
    chunks.clear();
}
