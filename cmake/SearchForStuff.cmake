#-------------------------------------------------------------------------------
#                       Search all libraries on the system
#-------------------------------------------------------------------------------
if(EXISTS ${PROJECT_SOURCE_DIR}/.git)
	find_package(Git)
endif()

if (Linux)
	find_package(ALSA REQUIRED)
	make_imported_target_if_missing(ALSA::ALSA ALSA)
endif()

find_package(PCAP REQUIRED)
find_package(LibXml2 REQUIRED)
make_imported_target_if_missing(LibXml2::LibXml2 LibXml2)
find_package(Freetype REQUIRED) # GS OSD
find_package(Gettext) # translation tool
find_package(LibLZMA REQUIRED)
make_imported_target_if_missing(LibLZMA::LibLZMA LIBLZMA)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(Vtune)

if(Fedora AND CMAKE_CROSSCOMPILING)
	set(wxWidgets_CONFIG_OPTIONS --arch ${PCSX2_TARGET_ARCHITECTURES} --unicode=yes)
else()
	set(wxWidgets_CONFIG_OPTIONS --unicode=yes)
endif()

list(APPEND wxWidgets_CONFIG_OPTIONS --toolkit=gtk3)

if(EXISTS "/usr/bin/wx-config-3.2")
	set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-3.2")
endif()
if(EXISTS "/usr/bin/wx-config-3.1")
	set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-3.1")
endif()
if(EXISTS "/usr/bin/wx-config-3.0")
	set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-3.0")
endif()
if(EXISTS "/usr/bin/wx-config")
	set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config")
endif()

find_package(wxWidgets REQUIRED base core adv)
include(${wxWidgets_USE_FILE})
make_imported_target_if_missing(wxWidgets::all wxWidgets)

find_package(ZLIB REQUIRED)
include(FindLibc)
include(FindPulseAudio)
include(CheckLib)

if(UNIX AND NOT APPLE)
	check_lib(EGL EGL EGL/egl.h)
	if(X11_API)
		check_lib(X11_XCB X11-xcb X11/Xlib-xcb.h)
		check_lib(XCB xcb xcb/xcb.h)
		check_lib(XRANDR xrandr)
	endif()
	if(WAYLAND_API)
		find_package(Wayland REQUIRED)
	endif()

	if(Linux)
		check_lib(AIO aio libaio.h)
		if(CMAKE_CROSSCOMPILING)
			check_lib(LIBUDEV udev libudev.h)
		else()
			check_lib(LIBUDEV libudev libudev.h)
		endif()
	endif()
endif()

# if(PORTAUDIO_API)
# 	check_lib(PORTAUDIO portaudio portaudio.h pa_linux_alsa.h)
# endif()

check_lib(SOUNDTOUCH SoundTouch SoundTouch.h PATH_SUFFIXES soundtouch)
check_lib(SAMPLERATE samplerate samplerate.h)

if(SDL2_API)
	check_lib(SDL2 SDL2 SDL.h PATH_SUFFIXES SDL2)
	alias_library(SDL::SDL PkgConfig::SDL2)
else()
	# Tell cmake that we use SDL as a library and not as an application
	set(SDL_BUILDING_LIBRARY TRUE)
	find_package(SDL REQUIRED)
endif()

if(UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
	make_imported_target_if_missing(X11::X11 X11)
endif()

if(UNIX)
	# Most plugins (if not all) and PCSX2 core need gtk2, so set the required flags
	if (GTK2_API)
		find_package(GTK2 REQUIRED gtk)
		alias_library(GTK::gtk GTK2::gtk)
	else()
	if(CMAKE_CROSSCOMPILING)
		find_package(GTK3 REQUIRED gtk)
		alias_library(GTK::gtk GTK3::gtk)
	else()
		check_lib(GTK3 gtk+-3.0 gtk/gtk.h)
		alias_library(GTK::gtk PkgConfig::GTK3)
	endif()
	endif()
endif()

find_package(HarfBuzz)
find_package(Threads REQUIRED)

include(ApiValidation)

WX_vs_SDL()

# Blacklist bad GCC
if(GCC_VERSION VERSION_EQUAL "7.0" OR GCC_VERSION VERSION_EQUAL "7.1")
	GCC7_BUG()
endif()

find_package(fmt "7.1.3" QUIET)
if(NOT fmt_FOUND)
	if(EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/fmt/fmt/CMakeLists.txt")
		message(STATUS "No system fmt was found. Using bundled")
		add_subdirectory(3rdparty/fmt/fmt)
	else()
		message(FATAL_ERROR "No system or bundled fmt was found")
	endif()
else()
	message(STATUS "Found fmt: ${fmt_VERSION}")
endif()

if(USE_SYSTEM_YAML)
	find_package(yaml-cpp "0.6.3" QUIET)
	if(NOT yaml-cpp_FOUND)
		message(STATUS "No system yaml-cpp was found")
		set(USE_SYSTEM_YAML OFF)
	else()
		message(STATUS "Found yaml-cpp: ${yaml-cpp_VERSION}")
		message(STATUS "Note that the latest release of yaml-cpp is very outdated, and the bundled submodule in the repo has over a year of bug fixes and as such is preferred.")
	endif()
endif()

# add_subdirectory(3rdparty/libchdr/libchdr EXCLUDE_FROM_ALL)
# add_subdirectory(3rdparty/glad EXCLUDE_FROM_ALL)
