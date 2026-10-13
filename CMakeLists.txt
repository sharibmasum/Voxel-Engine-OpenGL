cmake_minimum_required(VERSION 3.28)
project(Minecraft)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/glad)
include_directories(${CMAKE_SOURCE_DIR}/Shaders)
include_directories(${CMAKE_SOURCE_DIR}/stb)
include_directories(${CMAKE_SOURCE_DIR}/imgui)
include_directories(${CMAKE_SOURCE_DIR}/imgui/backends)
include_directories(/opt/homebrew/include)

# Add GLM
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLM REQUIRED glm)

link_directories(/opt/homebrew/lib)

set(SOURCES
        src/main.cpp
        src/Mesh.cpp
        src/Shader.cpp
        glad/gl.c
        src/Window.cpp
        include/Window.h
        src/Camera.cpp
        include/Camera.h
        src/Texture.cpp
        include/Texture.h
        src/Light.cpp
        include/Light.h
        src/Material.cpp
        include/Material.h
        src/Chunk.cpp
        include/Chunk.h
        src/Planet.cpp
        include/Planet.h
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_opengl3.cpp
)

# Executable target
add_executable(Minecraft ${SOURCES})

# Link GLFW, GLM and macOS system frameworks
target_link_libraries(Minecraft
        glfw
        ${GLM_LIBRARIES}
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
)

target_include_directories(Minecraft PRIVATE ${GLM_INCLUDE_DIRS})
