# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
	message(FATAL_ERROR "
	You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
	It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

add_library(common)

target_sources(common PRIVATE
	AlignedMalloc.cpp
	VirtualMemory.cpp
	EventSource.inl
	SafeArray.inl
	Console.cpp
	EventSource.cpp
	Exceptions.cpp
	FastFormatString.cpp
	FastJmp.cpp
	GL/Context.cpp
	GL/StreamBuffer.cpp
	IniInterface.cpp
	Mutex.cpp
	Misc.cpp
	PathUtils.cpp
	# PrecompiledHeader.cpp
	Perf.cpp
	pxStreams.cpp
	# pxTranslate.cpp
	RwMutex.cpp
	Semaphore.cpp
	SettingsWrapper.cpp
	StringHelpers.cpp
	StringUtil.cpp
	ThreadTools.cpp
	WindowInfo.cpp
	emitter/bmi.cpp
	emitter/cpudetect.cpp
	emitter/fpu.cpp
	emitter/groups.cpp
	emitter/jmp.cpp
	emitter/legacy.cpp
	emitter/legacy_sse.cpp
	emitter/movs.cpp
	emitter/simd.cpp
	emitter/LnxCpuDetect.cpp
	emitter/WinCpuDetect.cpp
	emitter/x86emitter.cpp
	x86/MemcpyFast.cpp
	# Darwin/DarwinThreads.cpp
	# Darwin/DarwinMisc.cpp
	# Darwin/DarwinSemaphore.cpp
	Linux/LnxHostSys.cpp
	Linux/LnxThreads.cpp
	Linux/LnxMisc.cpp
	# Windows/WinThreads.cpp
	# Windows/WinHostSys.cpp
	# Windows/WinMisc.cpp
)
target_sources(common PRIVATE
	Align.h
	Assertions.h
	boost_spsc_queue.hpp
	Console.h
	Dependencies.h
	EventSource.h
	Exceptions.h
	FastJmp.h
	General.h
	GL/Context.h
	GL/StreamBuffer.h
	MemcpyFast.h
	MemsetFast.inl
	Path.h
	PageFaultSource.h
	PrecompiledHeader.h
	pxForwardDefs.h
	pxStreams.h
	RedtapeWindows.h
	RwMutex.h
	SafeArray.h
	ScopedAlloc.h
	ScopedGuard.h
	ScopedPtrMT.h
	SettingsInterface.h
	SettingsWrapper.h
	StringHelpers.h
	StringUtil.h
	Threading.h
	TraceLog.h
	WindowInfo.h
	wxBaseTools.h
	emitter/cpudetect_internal.h
	emitter/implement/dwshift.h
	emitter/implement/group1.h
	emitter/implement/group2.h
	emitter/implement/group3.h
	emitter/implement/helpers.h
	emitter/implement/incdec.h
	emitter/implement/jmpcall.h
	emitter/implement/movs.h
	emitter/implement/simd_arithmetic.h
	emitter/implement/simd_comparisons.h
	emitter/implement/simd_helpers.h
	emitter/implement/simd_moremovs.h
	emitter/implement/simd_shufflepack.h
	emitter/implement/simd_templated_helpers.h
	emitter/implement/test.h
	emitter/implement/xchg.h
	emitter/instructions.h
	emitter/internal.h
	emitter/legacy_instructions.h
	emitter/legacy_internal.h
	emitter/legacy_types.h
	emitter/tools.h
	emitter/x86emitter.h
	emitter/x86types.h
	)

if(USE_VTUNE)
	target_link_libraries(common PUBLIC Vtune::Vtune)
endif()

target_sources(common PRIVATE
GL/ContextEGL.cpp
GL/ContextEGL.h
)
target_link_libraries(common PRIVATE PkgConfig::EGL)

target_sources(common PRIVATE
GL/ContextEGLX11.cpp
GL/ContextEGLX11.h
)
if(TARGET PkgConfig::XRANDR)
target_link_libraries(common PRIVATE PkgConfig::XRANDR)
target_compile_definitions(common PRIVATE "HAS_XRANDR=1")
endif()

target_link_libraries(common PRIVATE ${LIBC_LIBRARIES} PUBLIC wxWidgets::all ${GLAD_LIBRARIES})
target_compile_features(common PUBLIC cxx_std_17)
target_include_directories(common PUBLIC ../3rdparty/include ../)
target_compile_definitions(common PUBLIC "${PCSX2_DEFS}")
target_compile_options(common PRIVATE "${PCSX2_WARNINGS}")

# if(COMMAND target_precompile_headers)
# 	target_precompile_headers(common PRIVATE PrecompiledHeader.h)
# endif()
