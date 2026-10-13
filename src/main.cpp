#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <cstdio>
#include <stdlib.h>
#include <cmath>
#include <vector>

#include "gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

#include "Window.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "Light.h"
#include "Material.h"
#include "Chunk.h"
#include "Planet.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"



std::vector<Mesh *> meshList;
std::vector<glm::vec3> cubePositions;
std::vector<Shader> shaderList;

Window mainWindow;
Camera camera;

GLfloat deltaTime = 0.0f;
GLfloat lastTime = 0.0f;

Texture brickTexture;
Texture dirtTexture;

Material shinyMaterial;
Material dullMaterial;

Planet* planet = nullptr;

Light mainLight;
glm::vec3 lightPosition (-2.0f, 2.0f, -1.0f);
glm::vec3 cameraPosition(1.0f, 1.0f, -1.0f);

GLuint uniformProjection = 0, uniformModel = 0, uniformView = 0, uniformAmbientIntensity = 0, uniformAmbientColour = 0, uniformDirection = 0, uniformDiffuseIntensity = 0, uniformCameraPosition = 0, uniformSpecularIntensity = 0, uniformShininess = 0;

static const char *vShader = "Shaders/vertexShader.glsl";
static const char *fShader = "Shaders/fragmentShader.glsl";


glm::vec3 getRandomNearbyPosition(float maxDistance) {
    // Ensure rand is seeded once
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        seeded = true;
    }

    auto randomFloat = [](float min, float max) {
        return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
    };

    float x = randomFloat(-maxDistance, maxDistance);
    float y = randomFloat(-maxDistance, maxDistance);
    float z = randomFloat(-maxDistance, maxDistance);

    return glm::vec3(x, y, z);
}

void calcAverageNormals(unsigned int *indices, unsigned int indiceCount, GLfloat *vertices, unsigned int verticiesCount,
                        unsigned int vLength, unsigned int normalOffset) {
    for (size_t i = 0; i < indiceCount; i += 3) {
        unsigned int in0 = indices[i] * vLength;
        unsigned int in1 = indices[i + 1] * vLength;
        unsigned int in2 = indices[i + 2] * vLength;

        glm::vec3 p0(vertices[in0], vertices[in0 + 1], vertices[in0 + 2]);
        glm::vec3 p1(vertices[in1], vertices[in1 + 1], vertices[in1 + 2]);
        glm::vec3 p2(vertices[in2], vertices[in2 + 1], vertices[in2 + 2]);

        glm::vec3 v1 = p1 - p0;
        glm::vec3 v2 = p2 - p0;

        glm::vec3 normal = glm::normalize(glm::cross(v1, v2));

        in0 += normalOffset;
        in1 += normalOffset;
        in2 += normalOffset;

        vertices[in0] += normal.x;
        vertices[in0 + 1] += normal.y;
        vertices[in0 + 2] += normal.z;

        vertices[in1] += normal.x;
        vertices[in1 + 1] += normal.y;
        vertices[in1 + 2] += normal.z;

        vertices[in2] += normal.x;
        vertices[in2 + 1] += normal.y;
        vertices[in2 + 2] += normal.z;

    }


    for (size_t i = 0; i < verticiesCount / vLength; i++) {
        unsigned int nOffset = i * vLength + normalOffset;
        glm::vec3 vec(vertices[nOffset], vertices[nOffset + 1], vertices[nOffset + 2]);
        vec = glm::normalize(vec);
        vertices[nOffset] = vec.x;
        vertices[nOffset + 1] = vec.y;
        vertices[nOffset + 2] = vec.z;

    }
}

void createObjects() {
    planet = new Planet(cameraPosition);
}

void createShader() {
    Shader *shader1 = new Shader();
    shader1->createFromFiles(vShader, fShader); //i dont know waht the arrow is for.
    shaderList.push_back(*shader1);
}

bool wireframe = false;
float fpsTimer = 0.0f;
int frameCount = 0;
int displayedFps = 0;

bool cursorFree = false; // true while Caps Lock is held, releasing the mouse for the UI


int main() {

    mainWindow = Window();
    mainWindow.initalize();
    glEnable(GL_DEPTH_TEST);

    createObjects();
    createShader();

    camera = Camera(cameraPosition, glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f, 15.0f, 0.05f);

    // ImGui must be set up after the GLFW window/callbacks exist (Window::initalize() installs
    // the camera's key/mouse callbacks first) so ImGui_ImplGlfw chains onto them instead of
    // replacing them.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(mainWindow.getWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 150");

    brickTexture = Texture("Textures/brick.png");
    brickTexture.loadTexture();
    dirtTexture = Texture("Textures/DirtBlock.png");
    dirtTexture.loadTexture();

    shinyMaterial = Material(1.0f, 32.0f);
    dullMaterial = Material(0.4f, 4.0f);

    mainLight = Light(1.0f, 1.0f, 1.0f, 1.0f, lightPosition.x, lightPosition.y, lightPosition.z, 0.0f);


    while (!mainWindow.getShouldClose()) {

        /// CAMERA AND MOVEMENTS - PRE MUCH FIXED ////
        GLfloat now = glfwGetTime();
        deltaTime = now - lastTime;
        lastTime = now;


        fpsTimer += deltaTime;
        frameCount++;

        if (fpsTimer >= 1.0f) {
            displayedFps = frameCount;
            std::cout << "FPS: " << frameCount << std::endl;
            fpsTimer = 0.0f;
            frameCount = 0;
        }


        glfwPollEvents();

        // Hold Caps Lock to release the cursor and use the UI; release it to go back to flying.
        bool capsHeld = mainWindow.getsKeys()[GLFW_KEY_CAPS_LOCK];
        if (capsHeld != cursorFree) {
            cursorFree = capsHeld;
            glfwSetInputMode(mainWindow.getWindow(), GLFW_CURSOR,
                              cursorFree ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            mainWindow.resetMouseDelta(); // avoid a camera snap from the cursor warp/jump
        }

        if (!cursorFree) {
            camera.keyControl(mainWindow.getsKeys(), deltaTime);
            camera.mouseControl(mainWindow.getXChange(), mainWindow.getYChange());
        }

        glm::mat4 projection = glm::perspective(45.0f, (GLfloat) mainWindow.getBufferWidth() /
                                                       (GLfloat) mainWindow.getBufferHeight(), 0.1f, 400.f);

        glClearColor(94.0f / 250.0f, 198.0f / 250.0f, 242.0f / 250.f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        /// /  / / / // / / / / / / // / / / / /////

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %d (%.2f ms/frame)", displayedFps, deltaTime * 1000.0f);
        ImGui::Text("Chunks loaded: %d  pending: %d  culled: %d", planet->getLoadedChunkCount(), planet->getPendingChunkCount(), planet->getCulledChunkCount());
        int renderDistance = planet->getRenderDistance();
        if (ImGui::SliderInt("Render Distance", &renderDistance, 1, 32)) {
            planet->setRenderDistance(renderDistance);
        }
        ImGui::TextDisabled("Hold Caps Lock to free the cursor");
        ImGui::End();


        static bool fWasPressed = false;

        if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS &&
            glfwGetKey(mainWindow.getWindow(), GLFW_KEY_F) == GLFW_PRESS)
        {
            if (!fWasPressed) {
                wireframe = !wireframe;
                glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
                std::cout << (wireframe ? "Wireframe ON\n" : "Wireframe OFF\n");
                fWasPressed = true;
            }
        }
        else {
            fWasPressed = false;
        }


         //SHADERS  AND LIGHT
        shaderList[0].useShader();
        uniformModel = shaderList[0].getModelLocation();
        uniformProjection = shaderList[0].getProjectionLocation();
        uniformView = shaderList[0].getViewLocation();
        uniformAmbientIntensity = shaderList[0].getAmbientIntensityLocation();
        uniformAmbientColour = shaderList[0].getAmbientColourLocation();
        uniformDirection = shaderList[0].getDirectionLocation();
        uniformDiffuseIntensity = shaderList[0].getDiffuseIntensityLocation();
        uniformSpecularIntensity = shaderList[0].getSpecularIntensityLocation();
        uniformShininess = shaderList[0].getShininessLocation();
        uniformCameraPosition = shaderList[0].getCameraPosition();
        mainLight.useLight(uniformAmbientIntensity, uniformAmbientColour, uniformDiffuseIntensity, uniformDirection);

        glUniform3f(uniformCameraPosition, camera.getCameraPosition().x, camera.getCameraPosition().y, camera.getCameraPosition().z); // attaches to the camera pos in frag shader

        // chunk shit test

        glm::mat4 view = camera.calculateViewMatrix();

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
        dirtTexture.useTexture();

        dullMaterial.useMaterial(uniformSpecularIntensity, uniformShininess);

        glm::vec3 camPos = camera.getCameraPosition();
        planet->update(camPos.x, camPos.y, camPos.z, uniformModel);
        planet->render(uniformModel, projection, view);



        glUseProgram(0);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        mainWindow.swapBuffers();

    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
