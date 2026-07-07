cmake_minimum_required(VERSION 3.22)

project(godot-box3d LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# --- box3d (already a submodule, exports box3d::box3d) ---
# These are option()s defined inside box3d's own CMakeLists.txt, guarded by
# if(PROJECT_IS_TOP_LEVEL), which is false once we add_subdirectory() it from here -- but
# BOX3D_DOUBLE_PRECISION is defined unconditionally, so it must still be forced OFF before
# the add_subdirectory() call to match Godot's single-precision default build.
set(BOX3D_DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX3D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX3D_UNIT_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(box3d)

# --- godot-cpp (fetched via ExternalProject_Add) ---
include(cmake/GodotBox3DExternalGodotCpp.cmake)

# --- godot-box3d extension ---
file(GLOB_RECURSE GODOT_BOX3D_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(godot-box3d SHARED ${GODOT_BOX3D_SOURCES})

target_include_directories(godot-box3d PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_precompile_headers(godot-box3d PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/precompiled.hpp")

target_link_libraries(godot-box3d PRIVATE box3d::box3d godot-cpp)

target_compile_features(godot-box3d PRIVATE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(godot-box3d PRIVATE -Wall)
endif()

# The .gdextension expects "godot-box3d.dll" on Windows but "libgodot-box3d.so/.dylib"
# elsewhere, so the prefix is per-platform rather than a blanket "lib".
if(WIN32)
	set(GODOT_BOX3D_LIB_PREFIX "")
else()
	set(GODOT_BOX3D_LIB_PREFIX "lib")
endif()

set_target_properties(godot-box3d PROPERTIES
	PREFIX "${GODOT_BOX3D_LIB_PREFIX}"
	OUTPUT_NAME "godot-box3d"
	POSITION_INDEPENDENT_CODE ON
)

# Place the built extension where the .gdextension template expects it during local
# testing. The $<1:...> wrapper stops multi-config generators (Visual Studio, Xcode)
# from appending a Debug/Release subdirectory the .gdextension wouldn't look in.
set_target_properties(godot-box3d PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_SOURCE_DIR}/bin>"
	RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_SOURCE_DIR}/bin>"
)
