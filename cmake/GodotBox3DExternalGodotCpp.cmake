include_guard()

# Fetches godot-cpp via ExternalProject_Add (not FetchContent, not a submodule) at
# configure time, pinned to the godot-4.3-stable tag (matching this extension's
# compatibility_minimum). Builds a single Debug/Release pair rather than godot-jolt's
# six-way Debug/Development/Distribution/EditorDebug/EditorDevelopment/EditorDistribution
# matrix -- v1 doesn't need per-editor-build configs yet.

include(ExternalProject)

set(GODOT_CPP_GIT_REPOSITORY "https://github.com/godotengine/godot-cpp.git")
set(GODOT_CPP_GIT_TAG "godot-4.3-stable")

set(GODOT_CPP_PREFIX "${CMAKE_BINARY_DIR}/_deps/godot-cpp")
set(GODOT_CPP_SOURCE_DIR "${GODOT_CPP_PREFIX}/src/godot-cpp")
set(GODOT_CPP_BINARY_DIR "${GODOT_CPP_PREFIX}/src/godot-cpp-build")

# godot-cpp's own CMakeLists.txt names its output library
# "godot-cpp.<system_name>.<build_type>.<bits>" (see its OUTPUT_NAME logic) and derives
# <build_type> from CMAKE_BUILD_TYPE -- defaulting it to Debug when empty, even under
# multi-config generators that otherwise ignore CMAKE_BUILD_TYPE. So the variable must
# always be passed explicitly or the library name won't match what we import below.
# The library is placed under <binary_dir>/bin; multi-config generators additionally
# append a per-config subdirectory (bin/Debug, bin/Release, ...) because godot-cpp sets
# ARCHIVE_OUTPUT_DIRECTORY without a generator expression to suppress it.
string(TOLOWER "${CMAKE_SYSTEM_NAME}" GODOT_CPP_SYSTEM_NAME_LOWER)
math(EXPR GODOT_CPP_BITS "${CMAKE_SIZEOF_VOID_P} * 8")

if(CMAKE_CONFIGURATION_TYPES)
	# Multi-config generators (Visual Studio, Xcode, Ninja Multi-Config): forward the
	# active configuration to the external project's configure and build steps at build
	# time, and point each imported configuration at its per-config library below.
	set(GODOT_CPP_BUILD_TYPE_ARG "-DCMAKE_BUILD_TYPE=$<CONFIG>")
	set(GODOT_CPP_BUILD_CONFIG_ARG --config $<CONFIG>)
	set(GODOT_CPP_LIB_PATH
		"${GODOT_CPP_BINARY_DIR}/bin/$<CONFIG>/${CMAKE_STATIC_LIBRARY_PREFIX}godot-cpp.${GODOT_CPP_SYSTEM_NAME_LOWER}.$<LOWER_CASE:$<CONFIG>>.${GODOT_CPP_BITS}${CMAKE_STATIC_LIBRARY_SUFFIX}"
	)
else()
	if(NOT CMAKE_BUILD_TYPE)
		set(GODOT_CPP_EFFECTIVE_BUILD_TYPE "Release")
	else()
		set(GODOT_CPP_EFFECTIVE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
	endif()
	string(TOLOWER "${GODOT_CPP_EFFECTIVE_BUILD_TYPE}" GODOT_CPP_BUILD_TYPE_LOWER)
	set(GODOT_CPP_BUILD_TYPE_ARG "-DCMAKE_BUILD_TYPE=${GODOT_CPP_EFFECTIVE_BUILD_TYPE}")
	set(GODOT_CPP_BUILD_CONFIG_ARG "")
	set(GODOT_CPP_LIB_PATH
		"${GODOT_CPP_BINARY_DIR}/bin/${CMAKE_STATIC_LIBRARY_PREFIX}godot-cpp.${GODOT_CPP_SYSTEM_NAME_LOWER}.${GODOT_CPP_BUILD_TYPE_LOWER}.${GODOT_CPP_BITS}${CMAKE_STATIC_LIBRARY_SUFFIX}"
	)
endif()

ExternalProject_Add(godot-cpp-external
	PREFIX "${GODOT_CPP_PREFIX}"
	GIT_REPOSITORY "${GODOT_CPP_GIT_REPOSITORY}"
	GIT_TAG "${GODOT_CPP_GIT_TAG}"
	GIT_SHALLOW TRUE
	SOURCE_DIR "${GODOT_CPP_SOURCE_DIR}"
	BINARY_DIR "${GODOT_CPP_BINARY_DIR}"
	CMAKE_ARGS
		-G ${CMAKE_GENERATOR}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON
		${GODOT_CPP_BUILD_TYPE_ARG}
	BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${GODOT_CPP_BUILD_CONFIG_ARG} --target godot-cpp
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS "${GODOT_CPP_LIB_PATH}"
	UPDATE_DISCONNECTED TRUE
)

file(MAKE_DIRECTORY "${GODOT_CPP_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${GODOT_CPP_SOURCE_DIR}/gdextension")
file(MAKE_DIRECTORY "${GODOT_CPP_BINARY_DIR}/gen/include")

add_library(godot-cpp STATIC IMPORTED GLOBAL)
add_dependencies(godot-cpp godot-cpp-external)

if(CMAKE_CONFIGURATION_TYPES)
	foreach(GODOT_CPP_CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
		string(TOUPPER "${GODOT_CPP_CONFIG}" GODOT_CPP_CONFIG_UPPER)
		string(TOLOWER "${GODOT_CPP_CONFIG}" GODOT_CPP_CONFIG_LOWER)
		set_target_properties(godot-cpp PROPERTIES
			IMPORTED_LOCATION_${GODOT_CPP_CONFIG_UPPER}
			"${GODOT_CPP_BINARY_DIR}/bin/${GODOT_CPP_CONFIG}/${CMAKE_STATIC_LIBRARY_PREFIX}godot-cpp.${GODOT_CPP_SYSTEM_NAME_LOWER}.${GODOT_CPP_CONFIG_LOWER}.${GODOT_CPP_BITS}${CMAKE_STATIC_LIBRARY_SUFFIX}"
		)
	endforeach()
else()
	set_target_properties(godot-cpp PROPERTIES
		IMPORTED_LOCATION "${GODOT_CPP_LIB_PATH}"
	)
endif()

set_target_properties(godot-cpp PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES
		"${GODOT_CPP_SOURCE_DIR}/include;${GODOT_CPP_SOURCE_DIR}/gdextension;${GODOT_CPP_BINARY_DIR}/gen/include"
)

# godot-cpp's own CMakeLists exports TYPED_METHOD_BIND as a PUBLIC compile definition on
# MSVC (and its SCons build sets it plus NOMINMAX in tools/windows.py). That usage
# requirement is lost across this hand-made IMPORTED target, so it must be restored here:
# without it, any extension TU that instantiates ClassDB::bind_method takes the
# reinterpret_cast-to-_gde_UnexistingClass path in method_bind.hpp, which MSVC rejects
# with C2440 ("Pointers to members have different representations; cannot cast between
# them") because MSVC member-pointer sizes vary by inheritance model. TYPED_METHOD_BIND
# selects the fully-typed MethodBind templates with no cast, matching how the godot-cpp
# static library itself was compiled inside the external project. GCC/Clang are
# unaffected either way (Itanium ABI member pointers are uniform), which is why this only
# surfaces on Windows.
if(MSVC)
	set_property(TARGET godot-cpp APPEND PROPERTY
		INTERFACE_COMPILE_DEFINITIONS TYPED_METHOD_BIND NOMINMAX
	)
endif()
