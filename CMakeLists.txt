cmake_minimum_required(VERSION 3.22)

project(godot-box3d LANGUAGES C CXX)

# SCons is the recommended portable build because it follows Godot's official
# godot-cpp platform toolchains. This CMake path remains available for IDEs,
# desktop development, and teams that already maintain CMake toolchain files.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies statically" FORCE)

foreach(required_dependency IN ITEMS box3d godot-cpp)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${required_dependency}/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing dependency '${required_dependency}'. "
            "Run scripts/bootstrap_dependencies.sh from the repository root."
        )
    endif()
endforeach()

# Box3D must use the same single-precision layout as the default Godot build.
set(BOX3D_DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(BOX3D_DISABLE_SIMD OFF CACHE BOOL "" FORCE)
set(BOX3D_SANITIZE OFF CACHE BOOL "" FORCE)
set(BOX3D_PROFILE OFF CACHE BOOL "" FORCE)
set(BOX3D_VALIDATE OFF CACHE BOOL "" FORCE)
set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX3D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX3D_UNIT_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(box3d EXCLUDE_FROM_ALL)

# Building godot-cpp in the same CMake graph is essential for cross-compilation:
# Android NDK, Apple SDK, and Emscripten settings are inherited automatically.
set(GODOT_CPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
set(GODOT_CPP_WARNING_AS_ERROR OFF CACHE BOOL "" FORCE)
set(FLOAT_PRECISION "single" CACHE STRING "" FORCE)
add_subdirectory(godot-cpp EXCLUDE_FROM_ALL)

file(GLOB_RECURSE GODOT_BOX3D_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(godot-box3d SHARED ${GODOT_BOX3D_SOURCES})

target_include_directories(godot-box3d PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_precompile_headers(godot-box3d PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/precompiled.hpp"
)

target_link_libraries(godot-box3d PRIVATE
    box3d::box3d
    godot::cpp
)

target_compile_features(godot-box3d PRIVATE cxx_std_17)
target_compile_definitions(godot-box3d PRIVATE GDEXTENSION)

if(MSVC)
    target_compile_options(godot-box3d PRIVATE /W3)
else()
    target_compile_options(godot-box3d PRIVATE -Wall -fno-exceptions)
    target_compile_options(godot-box3d PRIVATE -fvisibility=hidden)
endif()

# Determine Godot feature tags and filenames. These names match the committed
# .gdextension descriptor and the official godot-cpp template convention.
set(GODOT_BOX3D_PLATFORM "")
set(GODOT_BOX3D_ARCH "")
set(GODOT_BOX3D_THREAD_SUFFIX "")
set(GODOT_BOX3D_PREFIX "lib")
set(GODOT_BOX3D_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(GODOT_BOX3D_PLATFORM_DEFINES GDEXTENSION)

if(EMSCRIPTEN)
    set(GODOT_BOX3D_PLATFORM "web")
    set(GODOT_BOX3D_ARCH "wasm32")
    set(GODOT_BOX3D_THREAD_SUFFIX ".nothreads")
    set(GODOT_BOX3D_SUFFIX ".wasm")
    list(APPEND GODOT_BOX3D_PLATFORM_DEFINES WEB_ENABLED UNIX_ENABLED)
    target_compile_options(godot-box3d PRIVATE
        "-sSIDE_MODULE=1"
        "-sSUPPORT_LONGJMP=wasm"
    )
    target_link_options(godot-box3d PRIVATE
        "-sSIDE_MODULE=1"
        "-sSUPPORT_LONGJMP=wasm"
        "-fvisibility=hidden"
    )
elseif(ANDROID)
    set(GODOT_BOX3D_PLATFORM "android")
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(GODOT_BOX3D_ARCH "arm64")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(GODOT_BOX3D_ARCH "x86_64")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(GODOT_BOX3D_ARCH "arm32")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(GODOT_BOX3D_ARCH "x86_32")
    else()
        message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
    endif()
    list(APPEND GODOT_BOX3D_PLATFORM_DEFINES ANDROID_ENABLED UNIX_ENABLED)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(GODOT_BOX3D_PLATFORM "ios")
    set(GODOT_BOX3D_ARCH "arm64")
    list(APPEND GODOT_BOX3D_PLATFORM_DEFINES IOS_ENABLED UNIX_ENABLED)
elseif(APPLE)
    set(GODOT_BOX3D_PLATFORM "macos")
    # Universal macOS output deliberately omits the architecture tag.
    set(GODOT_BOX3D_ARCH "")
    list(APPEND GODOT_BOX3D_PLATFORM_DEFINES MACOS_ENABLED UNIX_ENABLED)
elseif(WIN32)
    set(GODOT_BOX3D_PLATFORM "windows")
    set(GODOT_BOX3D_PREFIX "")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(GODOT_BOX3D_ARCH "x86_64")
    else()
        set(GODOT_BOX3D_ARCH "x86_32")
    endif()
    list(APPEND GODOT_BOX3D_PLATFORM_DEFINES WINDOWS_ENABLED)
elseif(UNIX)
    set(GODOT_BOX3D_PLATFORM "linux")
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" GODOT_BOX3D_PROCESSOR)
    if(GODOT_BOX3D_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(GODOT_BOX3D_ARCH "arm64")
    elseif(GODOT_BOX3D_PROCESSOR MATCHES "^(x86_64|amd64)$")
        set(GODOT_BOX3D_ARCH "x86_64")
    else()
        message(FATAL_ERROR
            "Unsupported Linux processor '${CMAKE_SYSTEM_PROCESSOR}'. "
            "Use the SCons build for additional Godot architectures."
        )
    endif()
    list(APPEND GODOT_BOX3D_PLATFORM_DEFINES LINUX_ENABLED UNIX_ENABLED)
else()
    message(FATAL_ERROR "Unsupported target platform: ${CMAKE_SYSTEM_NAME}")
endif()

target_compile_definitions(godot-box3d PRIVATE ${GODOT_BOX3D_PLATFORM_DEFINES})

target_compile_definitions(godot-box3d PRIVATE
    "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:DEBUG_ENABLED>"
    "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:DEBUG_METHODS_ENABLED>"
)

if(GODOT_BOX3D_ARCH)
    set(GODOT_BOX3D_ARCH_SUFFIX ".${GODOT_BOX3D_ARCH}")
else()
    set(GODOT_BOX3D_ARCH_SUFFIX "")
endif()

set(GODOT_BOX3D_DEBUG_NAME
    "godot-box3d.${GODOT_BOX3D_PLATFORM}.template_debug${GODOT_BOX3D_ARCH_SUFFIX}${GODOT_BOX3D_THREAD_SUFFIX}"
)
set(GODOT_BOX3D_RELEASE_NAME
    "godot-box3d.${GODOT_BOX3D_PLATFORM}.template_release${GODOT_BOX3D_ARCH_SUFFIX}${GODOT_BOX3D_THREAD_SUFFIX}"
)

set(GODOT_BOX3D_OUTPUT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/${GODOT_BOX3D_PLATFORM}"
)
set(GODOT_BOX3D_TEST_OUTPUT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/test_project/addons/godot-box3d/bin/${GODOT_BOX3D_PLATFORM}"
)

set_target_properties(godot-box3d PROPERTIES
    PREFIX "${GODOT_BOX3D_PREFIX}"
    SUFFIX "${GODOT_BOX3D_SUFFIX}"
    OUTPUT_NAME "${GODOT_BOX3D_RELEASE_NAME}"
    OUTPUT_NAME_DEBUG "${GODOT_BOX3D_DEBUG_NAME}"
    OUTPUT_NAME_RELWITHDEBINFO "${GODOT_BOX3D_DEBUG_NAME}"
    OUTPUT_NAME_RELEASE "${GODOT_BOX3D_RELEASE_NAME}"
    OUTPUT_NAME_MINSIZEREL "${GODOT_BOX3D_RELEASE_NAME}"
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "$<1:${GODOT_BOX3D_OUTPUT_DIR}>"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${GODOT_BOX3D_OUTPUT_DIR}>"
)

add_custom_command(TARGET godot-box3d POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GODOT_BOX3D_TEST_OUTPUT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:godot-box3d>"
        "${GODOT_BOX3D_TEST_OUTPUT_DIR}/$<TARGET_FILE_NAME:godot-box3d>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/godot-box3d.gdextension"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_project/addons/godot-box3d/godot-box3d.gdextension"
    VERBATIM
)
