cmake_minimum_required(VERSION 3.18.1)

if (NOT DEFINED MODULE_NAME)
    message(FATAL_ERROR "MODULE_NAME is not set")
else()
    project(${MODULE_NAME} LANGUAGES C CXX)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
#  Compiler / linker flags
# ---------------------------------------------------------------------------
set(COMMON_FLAGS "-ffixed-x18 -Werror=format -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -fno-exceptions -fno-rtti")

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O2 -fvisibility=hidden -fvisibility-inlines-hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fvisibility=hidden -fvisibility-inlines-hidden")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}
        -Wl,--hash-style=both -Wl,--gc-sections -Wl,--strip-all -Wl,-exclude-libs,ALL")
else()
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O0 -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
endif()

# ---------------------------------------------------------------------------
#  Include directories
# ---------------------------------------------------------------------------
include_directories(
    KittyMemory
    Include
    Includes
    Includes/Dobby
    ImGui
    ImGui/backends
    xdl/include
    il2cpp
    il2cpp/src
)

# ---------------------------------------------------------------------------
#  Source collection
# ---------------------------------------------------------------------------
file(GLOB_RECURSE XDL_SRC   CONFIGURE_DEPENDS xdl/*.c xdl/*.cpp)
file(GLOB_RECURSE IL2CPP_SRC CONFIGURE_DEPENDS il2cpp/*.c il2cpp/*.cpp)

set(MODULE_SRCS
    ImGui/imgui.cpp
    ImGui/imgui_draw.cpp
    ImGui/imgui_demo.cpp
    ImGui/imgui_widgets.cpp
    ImGui/imgui_tables.cpp
    ImGui/backends/imgui_impl_opengl3.cpp
    ImGui/backends/imgui_impl_android.cpp
    # Vulkan backend removed

    KittyMemory/KittyArm64.cpp
    KittyMemory/KittyMemory.cpp
    KittyMemory/KittyScanner.cpp
    KittyMemory/KittyUtils.cpp
    KittyMemory/MemoryBackup.cpp
    KittyMemory/MemoryPatch.cpp

    main.cpp
    hook.cpp
    menu.cpp

    ${XDL_SRC}
    ${IL2CPP_SRC}
)

add_library(${MODULE_NAME} SHARED ${MODULE_SRCS})

# ---------------------------------------------------------------------------
#  Pre‑built static libraries (Dobby + curl)
# ---------------------------------------------------------------------------
set(DOBBY_LIB ${CMAKE_CURRENT_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}/libdobby.a)
set(CURL_LIB  ${CMAKE_CURRENT_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}/libcurl.a)

# ---------------------------------------------------------------------------
#  System libraries
# ---------------------------------------------------------------------------
find_library(LOG_LIB   log)
find_library(EGL_LIB   EGL)
find_library(ANDROID_LIB android)
find_library(GLESV2_LIB GLESv2)
find_library(Z_LIB     z)

if (ANDROID_PLATFORM_LEVEL LESS 12)
    message(FATAL_ERROR "OpenGL ES 2.0 requires API level 12+")
elseif (ANDROID_PLATFORM_LEVEL LESS 18)
    add_definitions("-DDYNAMIC_ES3")
    set(OPENGL_LIB GLESv2)
else()
    set(OPENGL_LIB GLESv3)
endif()

target_link_libraries(${MODULE_NAME}
    ${LOG_LIB}
    ${DOBBY_LIB}
    ${CURL_LIB}
    ${OPENGL_LIB}
    ${GLESV2_LIB}
    ${EGL_LIB}
    ${ANDROID_LIB}
    ${Z_LIB}
)

# ---------------------------------------------------------------------------
#  Strip the final .so in Release builds
# ---------------------------------------------------------------------------
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-all --remove-section=.comment
                "$<TARGET_FILE:${MODULE_NAME}>"
        COMMENT "Stripping ${MODULE_NAME}")
endif()