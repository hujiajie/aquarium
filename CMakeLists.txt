cmake_minimum_required(VERSION 3.14)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(aquarium)
set(CMAKE_CXX_STANDARD 14)

###############################################################################
# Directories definitions
###############################################################################

set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party")

set(DAWN_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/dawn")
if(NOT EXISTS "${DAWN_SOURCE_DIR}")
    message(FATAL_ERROR "Dawn not found in third_party/dawn")
endif()

if(NOT EMSCRIPTEN)
    set(DAWN_BUILD_DIR "${PROJECT_SOURCE_DIR}/out/Release/obj/third_party/dawn")
    if(NOT EXISTS "${DAWN_BUILD_DIR}")
        message(FATAL_ERROR "No build found in ${DAWN_BUILD_DIR}. Create it and be sure to build with dawn_complete_static_libs = true")
    endif()

    set(DAWN_GEN_DIR "${PROJECT_SOURCE_DIR}/out/Release/gen/third_party/dawn")
    if(NOT EXISTS "${DAWN_GEN_DIR}")
        message(FATAL_ERROR "No build found in ${DAWN_GEN_DIR}. Create it and be sure to build with dawn_complete_static_libs = true")
    endif()
endif()

###############################################################################
# Third Parties
###############################################################################

if(NOT EMSCRIPTEN)
    add_subdirectory(third_party/glfw)
endif()

set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SHADERC_SPIRV_TOOLS_DIR "${THIRD_PARTY_DIR}/dawn/third_party/SPIRV-Tools" CACHE STRING "" FORCE)
set(SHADERC_SPIRV_HEADERS_DIR "${THIRD_PARTY_DIR}/dawn/third_party/spirv-headers" CACHE STRING "" FORCE)
set(SHADERC_GLSLANG_DIR "${THIRD_PARTY_DIR}/dawn/third_party/glslang" CACHE STRING "" FORCE)
add_subdirectory(third_party/dawn/third_party/shaderc)

###############################################################################
# Aquarium itself
###############################################################################

# Ew
add_definitions(-DENABLE_DAWN_BACKEND)

if(EMSCRIPTEN)
    add_link_options("SHELL: -s ENVIRONMENT=web")
    add_link_options("SHELL: -s ASSERTIONS=1")
    add_link_options("SHELL: -s TOTAL_MEMORY=67108864 -s ALLOW_MEMORY_GROWTH=1 --no-heap-copy")
    add_link_options("SHELL: -s USE_WEBGPU=1")
    add_link_options("SHELL: --preload-file ${PROJECT_SOURCE_DIR}/assets@assets")
    add_link_options("SHELL: --preload-file ${PROJECT_SOURCE_DIR}/shaders@shaders")
else()
    add_definitions(-DDAWN_ENABLE_BACKEND_METAL)
endif()

set(AQUARIUM_SOURCES
    "${SRC_DIR}/common/AQUARIUM_ASSERT.h"
    "${SRC_DIR}/common/FPSTimer.cpp"
    "${SRC_DIR}/common/FPSTimer.h"
    "${SRC_DIR}/aquarium-optimized/Aquarium.cpp"
    "${SRC_DIR}/aquarium-optimized/Aquarium.h"
    "${SRC_DIR}/aquarium-optimized/Buffer.h"
    "${SRC_DIR}/aquarium-optimized/Context.h"
    "${SRC_DIR}/aquarium-optimized/Context.cpp"
    "${SRC_DIR}/aquarium-optimized/ContextFactory.cpp"
    "${SRC_DIR}/aquarium-optimized/ContextFactory.h"
    "${SRC_DIR}/aquarium-optimized/FishModel.h"
    "${SRC_DIR}/aquarium-optimized/Main.cpp"
    "${SRC_DIR}/aquarium-optimized/Matrix.h"
    "${SRC_DIR}/aquarium-optimized/Model.cpp"
    "${SRC_DIR}/aquarium-optimized/Model.h"
    "${SRC_DIR}/aquarium-optimized/Program.h"
    "${SRC_DIR}/aquarium-optimized/ResourceHelper.h"
    "${SRC_DIR}/aquarium-optimized/ResourceHelper.cpp"
    "${SRC_DIR}/aquarium-optimized/SeaweedModel.h"
    "${SRC_DIR}/aquarium-optimized/Texture.cpp"
    "${SRC_DIR}/aquarium-optimized/Texture.h"
    "${SRC_DIR}/aquarium-optimized/dawn/BufferDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/BufferDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/ContextDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/ContextDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/FishModelDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/FishModelDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/FishModelInstancedDrawDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/FishModelInstancedDrawDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/GenericModelDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/GenericModelDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/InnerModelDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/InnerModelDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/OutsideModelDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/OutsideModelDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/ProgramDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/ProgramDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/SeaweedModelDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/SeaweedModelDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/TextureDawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/TextureDawn.h"
    "${SRC_DIR}/aquarium-optimized/dawn/imgui_impl_dawn.cpp"
    "${SRC_DIR}/aquarium-optimized/dawn/imgui_impl_dawn.h"
    "${THIRD_PARTY_DIR}/imgui/imconfig.h"
    "${THIRD_PARTY_DIR}/imgui/imgui.h"
    "${THIRD_PARTY_DIR}/imgui/imgui.cpp"
    "${THIRD_PARTY_DIR}/imgui/imgui_demo.cpp"
    "${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp"
    "${THIRD_PARTY_DIR}/imgui/imgui_internal.h"
    "${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp"
    "${THIRD_PARTY_DIR}/imgui/examples/imgui_impl_glfw.h"
    "${THIRD_PARTY_DIR}/imgui/examples/imgui_impl_glfw.cpp"
    "${THIRD_PARTY_DIR}/dawn/src/common/Assert.h"
    "${THIRD_PARTY_DIR}/dawn/src/common/Assert.cpp"
    "${THIRD_PARTY_DIR}/dawn/src/common/Compiler.h"
    "${THIRD_PARTY_DIR}/dawn/src/common/Log.h"
    "${THIRD_PARTY_DIR}/dawn/src/common/Log.cpp"
    "${THIRD_PARTY_DIR}/dawn/src/utils/ComboRenderPipelineDescriptor.h"
    "${THIRD_PARTY_DIR}/dawn/src/utils/ComboRenderPipelineDescriptor.cpp"
    "${THIRD_PARTY_DIR}/dawn/src/utils/WGPUHelpers.h"
    "${THIRD_PARTY_DIR}/dawn/src/utils/WGPUHelpers.cpp"
    )

if(NOT EMSCRIPTEN)
    set(AQUARIUM_SOURCES ${AQUARIUM_SOURCES}
        "${THIRD_PARTY_DIR}/dawn/src/utils/SystemUtils.h"
        "${THIRD_PARTY_DIR}/dawn/src/utils/SystemUtils.cpp"
        "${THIRD_PARTY_DIR}/dawn/src/utils/BackendBinding.h"
        "${THIRD_PARTY_DIR}/dawn/src/utils/BackendBinding.cpp"
        "${THIRD_PARTY_DIR}/dawn/src/utils/MetalBinding.mm"
        )
endif()

add_executable(aquarium ${AQUARIUM_SOURCES})
if(EMSCRIPTEN)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/web/index.html
        ${CMAKE_CURRENT_BINARY_DIR}/index.html
        COPYONLY)
endif()

if(NOT EMSCRIPTEN)
    target_include_directories(aquarium
        PRIVATE "${DAWN_GEN_DIR}/src/include"
        )
endif()

target_include_directories(aquarium
    PRIVATE "${SRC_DIR}"
    PRIVATE "${THIRD_PARTY_DIR}/include"
    PRIVATE "${THIRD_PARTY_DIR}/rapidjson/include"
    PRIVATE "${THIRD_PARTY_DIR}/imgui"
    PRIVATE "${THIRD_PARTY_DIR}/imgui/examples"
    PRIVATE "${THIRD_PARTY_DIR}/stb"
    PRIVATE "${DAWN_SOURCE_DIR}/src/include"
    PRIVATE "${DAWN_SOURCE_DIR}/src"
    )

target_link_libraries(aquarium
    shaderc
    )

if(NOT EMSCRIPTEN)
    target_link_libraries(aquarium
        glfw
        "${DAWN_BUILD_DIR}/libdawn_native.a"
        "${DAWN_BUILD_DIR}/src/dawn/libdawn.a"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework IOSurface"
        "-framework Metal"
        "-framework QuartzCore"
        )
endif()
