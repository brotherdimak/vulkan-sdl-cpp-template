cmake_minimum_required(VERSION 3.20)

project(VulkanTemplate)

add_executable(${PROJECT_NAME}
    src/Main.cpp
    src/Application.cpp
    src/Renderer.cpp
    src/UIRenderer.cpp
    src/RenderUtils.cpp
    src/RenderPipeline.cpp
    src/Buffer.cpp
    src/Texture.cpp
    src/Shader.cpp
    src/Mesh.cpp
    src/Camera.cpp
    src/SceneObject.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VulkanTemplate)

# Vulkan
find_package(Vulkan REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

# Vulkan Memory Allocator
target_include_directories(${PROJECT_NAME} PRIVATE lib/vma/include)

# GLM
target_include_directories(${PROJECT_NAME} PRIVATE lib/glm)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# STB
target_include_directories(${PROJECT_NAME} PRIVATE lib/stb)

# Assimp
set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL "")
set(ASSIMP_INSTALL OFF CACHE INTERNAL "")
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE INTERNAL "")

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE INTERNAL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE INTERNAL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE INTERNAL "")

add_subdirectory(lib/assimp)

target_link_libraries(${PROJECT_NAME} PRIVATE assimp)

# ImGUI
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/lib/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/imgui_demo.cpp # Optional: remove if you don't want the demo
)

target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR}/backends) 

# SDL
add_subdirectory(lib/sdl)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)

# Copy assets and libraries
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:assimp> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
    COMMENT "Deploying runtime assets and third-party binaries ..."
)