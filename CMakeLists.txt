cmake_minimum_required(VERSION 3.16)
project(SmartFlow)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------
# Source Files
# --------------------------------------------------
file(GLOB_RECURSE SOURCES
    src/*.cpp
)
list(APPEND SOURCES src/glad.c)

# ImGui sources (single-file backends in external/include/ImGui)
file(GLOB IMGUI_SRC
    "${CMAKE_SOURCE_DIR}/external/include/ImGui/*.cpp"
)

# --------------------------------------------------
# Create executable
# --------------------------------------------------
add_executable(SmartFlow ${SOURCES} "src/simulate/Simulation.h" "src/simulate/Simulation.cpp")
target_sources(SmartFlow PRIVATE ${IMGUI_SRC} "src/simulate/Simulation.h")
# Ensure Shader.cpp is compiled (explicitly add new render sources)
target_sources(SmartFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src/render/Shader.cpp
)
target_sources(SmartFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src/render/Quad.cpp
    ${CMAKE_SOURCE_DIR}/src/render/Texture.cpp
)
target_sources(SmartFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src/render/Renderer.cpp
)
target_sources(SmartFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src/render/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/src/render/Model.cpp
)
target_sources(SmartFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src/logging.cpp
)
## tinyobjloader is a header-only implementation included from include/; no source file to add
target_sources(SmartFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src/render/Camera.cpp
)

# --------------------------------------------------
# Include Directories
# --------------------------------------------------
target_include_directories(SmartFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/include
    ${CMAKE_SOURCE_DIR}/external/include/MyGUI/include
    ${CMAKE_SOURCE_DIR}/external/include/MyGUI/Common
    ${CMAKE_SOURCE_DIR}/external/include/ImGui
)

# --------------------------------------------------
# Link Libraries
# --------------------------------------------------
target_link_directories(SmartFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/external/lib
)
target_link_libraries(SmartFlow
    glfw3dll
    opengl32
)

# ImGui: tell backend to use glad as loader
target_compile_definitions(SmartFlow PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# --------------------------------------------------
# Output Directory
# --------------------------------------------------
set_target_properties(SmartFlow PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR}/bin
)

# --------------------------------------------------
# Copy DLLs and Resources
# --------------------------------------------------
add_custom_command(TARGET SmartFlow POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/external/lib/glfw3.dll
        $<TARGET_FILE_DIR:SmartFlow>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/shaders
        $<TARGET_FILE_DIR:SmartFlow>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/external/include/ImGui
        $<TARGET_FILE_DIR:SmartFlow>/ImGui
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/models
        $<TARGET_FILE_DIR:SmartFlow>/models
)