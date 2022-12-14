project(Pcsx2)
cmake_minimum_required(VERSION 3.10...3.17)
set(TOP_CMAKE_WAS_SOURCED TRUE)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/exe)

include(Pcsx2Utils)

check_no_parenthesis_in_path()
detectOperatingSystem()
check_compiler_version("7.0" "7.0")

include(GNUInstallDirs)
include_directories(3rdparty/include)
include_directories(src/Core)


option(DISABLE_BUILD_DATE "Disable including the binary compile date")
option(ENABLE_TESTS "Enables building the unit tests" OFF)
option(USE_SYSTEM_YAML "Uses a system version of yaml, if found")
option(LTO_PCSX2_CORE "Enable LTO/IPO/LTCG on the subset of pcsx2 that benefits most from it but not anything else")
option(USE_NATIVE_TOOLS "Uses c++ tools instead of ones written in scripting languages.  OFF requires perl, ON may fail if cross compiling" ${DEFAULT_NATIVE_TOOLS})
option(USE_VTUNE "Plug VTUNE to profile GS JIT.")
option(BUILD_REPLAY_LOADERS "Build GS replayer to ease testing (developer option)")
option(PACKAGE_MODE "Use this option to ease packaging of PCSX2 (developer/distribution option)")
option(DISABLE_CHEATS_ZIP "Disable including the cheats_ws.zip file")
option(DISABLE_PCSX2_WRAPPER "Disable including the PCSX2-linux.sh file")
option(DISABLE_SETCAP "Do not set files capabilities")
option(XDG_STD "Use XDG standard path instead of the standard PCSX2 path")
option(PORTAUDIO_API "Build portaudio support on SPU2" OFF)
option(SDL2_API "Use SDL2 on SPU2 and PAD Linux (wxWidget mustn't be built with SDL1.2 support" ON)
option(GTK2_API "Use GTK2 api (legacy)")
option(X11_API "Enable X11 support" ON)
option(WAYLAND_API "Enable Wayland support" OFF)
option(USE_ASAN "Enable address sanitizer")


set(PCSX2_DEFS "")
set(DEFAULT_NATIVE_TOOLS OFF)
set(DISABLE_BUILD_DATE ON)
set(ENABLE_TESTS OFF)
set(BUILD_REPLAY_LOADERS OFF)
set(DISABLE_CHEATS_ZIP ON)
set(DISABLE_PCSX2_WRAPPER ON)
set(DISABLE_SETCAP ON)
set(USE_SYSTEM_YAML ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -ffast-math")
set(CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math -s")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ffast-math -g -Wno-free-nonheap-object")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -Wno-free-nonheap-object")
set(USE_GCC TRUE)

if(NOT CMAKE_BUILD_TYPE MATCHES "Debug|Devel|MinSizeRel|RelWithDebInfo|Release")
	set(CMAKE_BUILD_TYPE Devel)
	message(STATUS "BuildType set to ${CMAKE_BUILD_TYPE} by default")
endif()


set(CMAKE_C_FLAGS_DEVEL "${CMAKE_C_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used by the C compiler during development builds" FORCE)
set(CMAKE_CXX_FLAGS_DEVEL "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used by the C++ compiler during development builds" FORCE)
set(CMAKE_LINKER_FLAGS_DEVEL "${CMAKE_LINKER_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used for linking binaries during development builds" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEVEL "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used for linking shared libraries during development builds" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEVEL "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used for linking executables during development builds" FORCE)



if(CMAKE_CONFIGURATION_TYPES)
	list(INSERT CMAKE_CONFIGURATION_TYPES 0 Devel)
endif()
mark_as_advanced(CMAKE_C_FLAGS_DEVEL CMAKE_CXX_FLAGS_DEVEL CMAKE_LINKER_FLAGS_DEVEL CMAKE_SHARED_LINKER_FLAGS_DEVEL CMAKE_EXE_LINKER_FLAGS_DEVEL)
if(CMAKE_BUILD_TYPE MATCHES "Debug")
	SET(DISABLE_ADVANCE_SIMD ON)
endif()

option(CMAKE_BUILD_STRIP "Srip binaries to save a couple of MB (developer option)")
set(CMAKE_BUILD_PO FALSE)


option(DISABLE_ADVANCE_SIMD "Disable advance use of SIMD (SSE2+ & AVX)" OFF)

if(CMAKE_CROSSCOMPILING)
	message(STATUS "Cross compilation is enabled.")
else()
	message(STATUS "Cross compilation is disabled.")
endif()

include(TargetArch)
target_architecture(PCSX2_TARGET_ARCHITECTURES)
if(${PCSX2_TARGET_ARCHITECTURES} MATCHES "x86_64" OR ${PCSX2_TARGET_ARCHITECTURES} MATCHES "i386")
	message(STATUS "Compiling a ${PCSX2_TARGET_ARCHITECTURES} build on a ${CMAKE_HOST_SYSTEM_PROCESSOR} host.")
else()
	message(FATAL_ERROR "Unsupported architecture: ${PCSX2_TARGET_ARCHITECTURES}")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED ARCH_FLAG AND NOT MSVC)
	if (DISABLE_ADVANCE_SIMD)
		if (USE_ICC)
			set(ARCH_FLAG "-msse2 -msse4.1")
		else()
			set(ARCH_FLAG "-msse -msse2 -msse4.1 -mfxsr")
		endif()
	else()
					set(ARCH_FLAG "-march=native")
	endif()
endif()
list(APPEND PCSX2_DEFS _ARCH_64=1 _M_X86=1 _M_X86_64=1 __M_X86_64=1)
set(_ARCH_64 1)
set(_M_X86 1)
set(_M_X86_64 1)

string(REPLACE " " ";" ARCH_FLAG_LIST "${ARCH_FLAG}")
add_compile_options("${ARCH_FLAG_LIST}")

option(USE_PGO_GENERATE "Enable PGO optimization (generate profile)")
option(USE_PGO_OPTIMIZE "Enable PGO optimization (use profile)")

add_compile_options(-pipe -fvisibility=hidden -pthread -fno-builtin-strcmp -fno-builtin-memcmp -mfpmath=sse -fno-operator-names)


if(USE_VTUNE)
	list(APPEND PCSX2_DEFS ENABLE_VTUNE)
endif()

list(APPEND PCSX2_DEFS X11_API)

set(DEFAULT_WARNINGS -Wall -Wextra -Wno-attributes -Wno-unused-function -Wno-unused-parameter -Wno-missing-field-initializers -Wno-deprecated-declarations -Wno-format -Wno-format-security -Wno-overloaded-virtual)
list(APPEND DEFAULT_WARNINGS -Wno-unused-value)
list(APPEND DEFAULT_WARNINGS -Wno-stringop-truncation -Wno-stringop-overflow)
set(AGGRESSIVE_WARNING -Wstrict-aliasing -Wstrict-overflow=1)

if (USE_PGO_GENERATE OR USE_PGO_OPTIMIZE)
	add_compile_options("-fprofile-dir=${CMAKE_SOURCE_DIR}/profile")
endif()

if (USE_PGO_GENERATE)
	add_compile_options(-fprofile-generate)
endif()

if(USE_PGO_OPTIMIZE)
	add_compile_options(-fprofile-use)
endif()

if (USE_ASAN)
	add_compile_options(-fsanitize=address)
	list(APPEND PCSX2_DEFS ASAN_WORKAROUND)
endif()

set(PCSX2_WARNINGS ${DEFAULT_WARNINGS} ${AGGRESSIVE_WARNING})

if(CMAKE_BUILD_STRIP)
	add_link_options(-s)
endif()

if("$ENV{CI}" STREQUAL "true")
	list(APPEND PCSX2_DEFS PCSX2_CI)
endif()

include(SearchForStuff)

# get_git_version_info()
# write_svnrev_h()

add_subdirectory(src/common)
add_subdirectory(src/pcsx2)

