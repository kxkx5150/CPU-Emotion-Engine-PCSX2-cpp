include(macros/GlibCompileResourcesSupport)

set(CMAKE_C_STANDARD 11) 
set(CMAKE_CXX_STANDARD 17)

add_library(PCSX2_FLAGS INTERFACE)
add_executable(ps2)
target_link_libraries(ps2 PRIVATE PCSX2_FLAGS)
include_directories(Core)


target_compile_features(PCSX2_FLAGS INTERFACE cxx_std_17)
target_compile_definitions(PCSX2_FLAGS INTERFACE "${PCSX2_DEFS}")
target_compile_options(PCSX2_FLAGS INTERFACE "${PCSX2_WARNINGS}")

if (PACKAGE_MODE)
	install(TARGETS ps2 DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
	install(TARGETS ps2 DESTINATION ${CMAKE_SOURCE_DIR}/bin)
endif()

target_compile_options(PCSX2_FLAGS INTERFACE
-fno-strict-aliasing
-Wstrict-aliasing 		
-Wno-parentheses
-Wno-missing-braces
-Wno-unknown-pragmas
-DWX_PRECOMP
-Wno-free-nonheap-object
)

if(GCC_VERSION VERSION_EQUAL "8.0" OR GCC_VERSION VERSION_GREATER "8.0")
		target_compile_options(PCSX2_FLAGS INTERFACE -Wno-packed-not-aligned -Wno-class-memaccess)
endif()

if ("${PGO}" STREQUAL "generate")
	target_compile_options(PCSX2_FLAGS INTERFACE -fprofile-generate)
elseif("${PGO}" STREQUAL "use")
	target_compile_options(PCSX2_FLAGS INTERFACE -fprofile-use)
endif()

if(TARGET PkgConfig::PORTAUDIO)
	target_compile_definitions(PCSX2_FLAGS INTERFACE SPU2X_PORTAUDIO)
	target_link_libraries(PCSX2_FLAGS INTERFACE PkgConfig::PORTAUDIO)
endif()

if(TARGET PulseAudio::PulseAudio)
	target_compile_definitions(PCSX2_FLAGS INTERFACE SPU2X_PULSEAUDIO)
	target_link_libraries(PCSX2_FLAGS INTERFACE PulseAudio::PulseAudio)
endif()

if(XDG_STD)
	target_compile_definitions(PCSX2_FLAGS INTERFACE XDG_STD)
endif()

if(TARGET SDL::SDL)
	target_compile_definitions(PCSX2_FLAGS INTERFACE SDL_BUILD)
	target_link_libraries(PCSX2_FLAGS INTERFACE SDL::SDL)
endif()


set(pcsx2Sources
    Core/Cache.cpp
    Core/COP0.cpp
    Core/COP2.cpp
    Core/Counters.cpp
    Core/Elfheader.cpp
    Core/FW.cpp
    Core/FiFo.cpp
    Core/FPU.cpp
    Core/Gif.cpp
    Core/Gif_Logger.cpp
    Core/Gif_Unit.cpp
    Core/GS.cpp
    Core/GSState.cpp
    Core/Hw.cpp
    Core/HwRead.cpp
    Core/HwWrite.cpp
    Core/Interpreter.cpp
    Core/IopBios.cpp
    Core/IopCounters.cpp
    Core/IopDma.cpp
    Core/IopGte.cpp
    Core/IopHw.cpp
    Core/IopIrq.cpp
    Core/IopMem.cpp
    Core/IopSio2.cpp
    Core/IPC.cpp
    Core/Mdec.cpp
    Core/Memory.cpp
    Core/MemoryCardFile.cpp
    Core/MemoryCardFolder.cpp
    Core/MMI.cpp
    Core/MTGS.cpp
    Core/MTVU.cpp
    Core/MultipartFileReader.cpp
    Core/OutputIsoFile.cpp
    Core/Pcsx2Config.cpp
    Core/R3000A.cpp
    Core/R3000AInterpreter.cpp
    Core/R3000AOpcodeTables.cpp
    Core/R5900.cpp
    Core/R5900OpcodeImpl.cpp
    Core/R5900OpcodeTables.cpp
    Core/SaveState.cpp
    Core/ShiftJisToUnicode.cpp
    Core/Sif.cpp
    Core/Sif0.cpp
    Core/Sif1.cpp
    Core/sif2.cpp
    Core/Sio.cpp
    Core/SPR.cpp
    Core/System.cpp
    Core/Vif0_Dma.cpp
    Core/Vif1_Dma.cpp
    Core/Vif1_MFIFO.cpp
    Core/Vif.cpp
    Core/Vif_Codes.cpp
    Core/Vif_Transfer.cpp
    Core/Vif_Unpack.cpp
    Core/vtlb.cpp
    Core/VU0.cpp
    Core/VUmicro.cpp
    Core/VU0micro.cpp
    Core/VU0microInterp.cpp
    Core/VU1micro.cpp
    Core/VU1microInterp.cpp
    Core/VUflags.cpp
    Core/VUmicroMem.cpp
    Core/VUops.cpp
)

set(pcsx2Headers
    Core/AsyncFileReader.h
    Core/Cache.h
    Core/cheatscpp.h
    Core/Common.h
    Core/Config.h
    Core/COP0.h
    Core/Counters.h
    Core/Dmac.h
    Core/Elfheader.h
    Core/FW.h
    Core/Gif.h
    Core/Gif_Unit.h
    Core/GS.h
    Core/Hardware.h
    Core/Hw.h
    Core/IopBios.h
    Core/IopCommon.h
    Core/IopCounters.h
    Core/IopDma.h
    Core/IopGte.h
    Core/IopHw.h
    Core/IopMem.h
    Core/IopSio2.h
    Core/IPC.h
    Core/Mdec.h
    Core/MTVU.h
    Core/Memory.h
    Core/MemoryCardFile.h
    Core/MemoryCardFolder.h
    Core/MemoryTypes.h
    Core/PrecompiledHeader.h
    Core/R3000A.h
    Core/R5900Exceptions.h
    Core/R5900.h
    Core/R5900OpcodeTables.h
    Core/SaveState.h
    Core/Sifcmd.h
    Core/Sif.h
    Core/SingleRegisterTypes.h
    Core/Sio.h
    Core/sio_internal.h
    Core/SPR.h
    Core/SysForwardDefs.h
    Core/System.h
    Core/Vif_Dma.h
    Core/Vif.h
    Core/Vif_Unpack.h
    Core/vtlb.h
    Core/VUflags.h
    Core/VUmicro.h
    Core/VUops.h
)

set(pcsx2CDVDSources
	CDVD/BlockdumpFileReader.cpp
	CDVD/CdRom.cpp
	CDVD/CDVDaccess.cpp
	CDVD/CDVD.cpp
	CDVD/CDVDdiscReader.cpp
	CDVD/CDVDisoReader.cpp
	CDVD/CDVDdiscThread.cpp
	CDVD/InputIsoFile.cpp
	CDVD/OutputIsoFile.cpp
	CDVD/ChunksCache.cpp
	CDVD/CompressedFileReader.cpp
	CDVD/ChdFileReader.cpp
	CDVD/CsoFileReader.cpp
	CDVD/GzippedFileReader.cpp
	CDVD/ThreadedFileReader.cpp
	CDVD/IsoFS/IsoFile.cpp
	CDVD/IsoFS/IsoFSCDVD.cpp
	CDVD/IsoFS/IsoFS.cpp
	)

set(pcsx2CDVDHeaders
	CDVD/CdRom.h
	CDVD/CDVDaccess.h
	CDVD/CDVD.h
	CDVD/CDVD_internal.h
	CDVD/CDVDdiscReader.h
	CDVD/CDVDisoReader.h
	CDVD/ChunksCache.h
	CDVD/CompressedFileReader.h
	CDVD/CompressedFileReaderUtils.h
	CDVD/ChdFileReader.h
	CDVD/CsoFileReader.h
	CDVD/GzippedFileReader.h
	CDVD/ThreadedFileReader.h
	CDVD/IsoFileFormats.h
	CDVD/IsoFS/IsoDirectory.h
	CDVD/IsoFS/IsoFileDescriptor.h
	CDVD/IsoFS/IsoFile.h
	CDVD/IsoFS/IsoFSCDVD.h
	CDVD/IsoFS/IsoFS.h
	CDVD/IsoFS/SectorSource.h
	CDVD/zlib_indexed.h
	)

set(pcsx2SPU2Sources
	SPU2/ADSR.cpp
	SPU2/Debug.cpp
	SPU2/DplIIdecoder.cpp
	SPU2/Dma.cpp
	SPU2/Mixer.cpp
	SPU2/spu2.cpp
	SPU2/ReadInput.cpp
	SPU2/RegLog.cpp
	SPU2/RegTable.cpp
	SPU2/Reverb.cpp
	SPU2/SndOut.cpp
	SPU2/SndOut_SDL.cpp
	SPU2/spu2freeze.cpp
	SPU2/spu2sys.cpp
	SPU2/Timestretcher.cpp
	SPU2/Wavedump_wav.cpp
	SPU2/WavFile.cpp
	)

# if(TARGET PkgConfig::PORTAUDIO)
# 	list(APPEND pcsx2SPU2Sources SPU2/SndOut_Portaudio.cpp)
# endif()

set(pcsx2SPU2Headers
	SPU2/Config.h
	SPU2/Debug.h
	SPU2/defs.h
	SPU2/Dma.h
	SPU2/Global.h
	SPU2/interpolate_table.h
	SPU2/Mixer.h
	SPU2/spu2.h
	SPU2/regs.h
	SPU2/SndOut.h
	SPU2/spdif.h
	SPU2/WavFile.h
)

set(pcsx2DEV9Sources
	DEV9/ATA/Commands/ATA_Command.cpp
	DEV9/ATA/Commands/ATA_CmdDMA.cpp
	DEV9/ATA/Commands/ATA_CmdExecuteDeviceDiag.cpp
	DEV9/ATA/Commands/ATA_CmdNoData.cpp
	DEV9/ATA/Commands/ATA_CmdPIOData.cpp
	DEV9/ATA/Commands/ATA_CmdSMART.cpp
	DEV9/ATA/Commands/ATA_SCE.cpp
	DEV9/ATA/ATA_Info.cpp
	DEV9/ATA/ATA_State.cpp
	DEV9/ATA/ATA_Transfer.cpp
	DEV9/ATA/HddCreate.cpp
	DEV9/InternalServers/DHCP_Server.cpp
	DEV9/InternalServers/DNS_Logger.cpp
	DEV9/InternalServers/DNS_Server.cpp
	DEV9/PacketReader/IP/UDP/DHCP/DHCP_Options.cpp
	DEV9/PacketReader/IP/UDP/DHCP/DHCP_Packet.cpp
	DEV9/PacketReader/IP/UDP/DNS/DNS_Classes.cpp
	DEV9/PacketReader/IP/UDP/DNS/DNS_Packet.cpp
	DEV9/PacketReader/IP/UDP/UDP_Packet.cpp
	DEV9/PacketReader/IP/IP_Options.cpp
	DEV9/PacketReader/IP/IP_Packet.cpp
	DEV9/PacketReader/EthernetFrame.cpp
	DEV9/PacketReader/NetLib.cpp
	DEV9/smap.cpp
	DEV9/DEV9.cpp
	DEV9/DEV9Config.cpp
	DEV9/flash.cpp
	DEV9/pcap_io.cpp
	DEV9/net.cpp
	)

set(pcsx2DEV9Headers
	DEV9/ATA/ATA.h
	DEV9/ATA/HddCreate.h
	DEV9/DEV9.h
	DEV9/InternalServers/DHCP_Server.cpp
	DEV9/InternalServers/DNS_Logger.h
	DEV9/InternalServers/DNS_Server.h
	DEV9/net.h
	DEV9/PacketReader/IP/UDP/DHCP/DHCP_Options.h
	DEV9/PacketReader/IP/UDP/DHCP/DHCP_Packet.h
	DEV9/PacketReader/IP/UDP/DNS/DNS_Classes.h
	DEV9/PacketReader/IP/UDP/DNS/DNS_Enums.h
	DEV9/PacketReader/IP/UDP/DNS/DNS_Packet.h
	DEV9/PacketReader/IP/UDP/UDP_Packet.h
	DEV9/PacketReader/IP/IP_Address.h
	DEV9/PacketReader/IP/IP_Options.h
	DEV9/PacketReader/IP/IP_Packet.h
	DEV9/PacketReader/IP/IP_Payload.h
	DEV9/PacketReader/EthernetFrame.h
	DEV9/PacketReader/NetLib.h
	DEV9/PacketReader/Payload.h
	DEV9/pcap_io.h
	DEV9/SimpleQueue.h
	DEV9/smap.h
	)


set(PADImgHeader "${CMAKE_BINARY_DIR}/src/pcsx2/PAD/Linux/ImgHeader")
set(PADImg "${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/Img")
target_include_directories(PCSX2_FLAGS INTERFACE "${CMAKE_BINARY_DIR}/src/pcsx2/PAD/Linux/")

file(MAKE_DIRECTORY ${PADImgHeader})

foreach(result_file IN ITEMS
		analog circle cross dp_bottom dp_left dp_right dp_up dualshock2
		joystick_cursor l1 r1 l2 r2 l3 r3 select square start triangle arrow_up
		arrow_bottom arrow_left arrow_right)
	add_custom_command(
		OUTPUT  "${PADImgHeader}/${result_file}.h"
		DEPENDS ${BIN2CPPDEP} "${PADImg}/${result_file}.png"
		COMMAND ${BIN2CPP} "${PADImg}/${result_file}.png" "${PADImgHeader}/${result_file}" )
endforeach()

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/src/pcsx2/PAD/Linux/resources_pad.h
	COMMAND glib-compile-resources --sourcedir "${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/res" --generate-header
	--c-name PAD_res "${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/res/PAD-res.xml" --target=${CMAKE_BINARY_DIR}/src/pcsx2/PAD/Linux/resources_pad.h
	DEPENDS ${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/res/PAD-res.xml ${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/res/game_controller_db.txt)

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/src/pcsx2/PAD/Linux/resources_pad.cpp
	COMMAND glib-compile-resources --sourcedir "${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/res" --generate-source
	--c-name PAD_res "${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/res/PAD-res.xml" --target=${CMAKE_BINARY_DIR}/src/pcsx2/PAD/Linux/resources_pad.cpp
	DEPENDS ${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/res/PAD-res.xml ${CMAKE_SOURCE_DIR}/src/pcsx2/PAD/Linux/res/game_controller_db.txt)

set(pcsx2PADSources
	PAD/Linux/Device.cpp
	PAD/Linux/InputManager.cpp
	PAD/Linux/SDL/joystick.cpp
	PAD/Linux/keyboard.cpp
	PAD/Linux/KeyStatus.cpp
	PAD/Linux/PAD.cpp
	PAD/Linux/state_management.cpp
	PAD/Linux/Config.cpp
	PAD/Linux/linux.cpp
	${CMAKE_BINARY_DIR}/src/pcsx2/PAD/Linux/resources_pad.cpp
)

set(pcsx2PADHeaders
	PAD/Linux/bitwise.h
	PAD/Linux/Config.h
	PAD/Linux/Device.h
	PAD/Linux/Global.h
	PAD/Linux/InputManager.h
	PAD/Linux/SDL/joystick.h
	PAD/Linux/keyboard.h
	PAD/Linux/KeyStatus.h
	PAD/Linux/PAD.h
	PAD/Linux/state_management.h
	${CMAKE_BINARY_DIR}/src/pcsx2/PAD/Linux/resources_pad.h
	)

set(pcsx2GSSources
	GS/GS.cpp
	GS/GSAlignedClass.cpp
	GS/GSBlock.cpp
	GS/GSCapture.cpp
	GS/GSClut.cpp
	GS/GSCodeBuffer.cpp
	GS/GSCrc.cpp
	GS/GSDrawingContext.cpp
	GS/GSDump.cpp
	GS/GSLocalMemory.cpp
	GS/GSLzma.cpp
	GS/GSPerfMon.cpp
	GS/GSPng.cpp
	GS/GSState.cpp
	GS/GSTables.cpp
	GS/GSUtil.cpp
	GS/GSVector.cpp
	GS/Renderers/Common/GSDevice.cpp
	GS/Renderers/Common/GSDirtyRect.cpp
	GS/Renderers/Common/GSFunctionMap.cpp
	GS/Renderers/Common/GSOsdManager.cpp
	GS/Renderers/Common/GSRenderer.cpp
	GS/Renderers/Common/GSTexture.cpp
	GS/Renderers/Common/GSVertexTrace.cpp
	GS/Renderers/HW/GSHwHack.cpp
	GS/Renderers/HW/GSRendererHW.cpp
	GS/Renderers/HW/GSTextureCache.cpp
	GS/Renderers/OpenGL/GLLoader.cpp
	GS/Renderers/OpenGL/GLState.cpp
	GS/Renderers/OpenGL/GSDeviceOGL.cpp
	GS/Renderers/OpenGL/GSRendererOGL.cpp
	GS/Renderers/OpenGL/GSShaderOGL.cpp
	GS/Renderers/OpenGL/GSTextureCacheOGL.cpp
	GS/Renderers/OpenGL/GSTextureOGL.cpp
		)

set(pcsx2GSHeaders
	GS/config.h
	GS/GSAlignedClass.h
	GS/GSBlock.h
	GS/GSCapture.h
	GS/GSClut.h
	GS/GSCodeBuffer.h
	GS/GSCrc.h
	GS/GSDrawingContext.h
	GS/GSDrawingEnvironment.h
	GS/GSDump.h
	GS/GS_types.h
	GS/GS.h
	GS/GSLocalMemory.h
	GS/GSLzma.h
	GS/GSPerfMon.h
	GS/GSPng.h
	GS/GSState.h
	GS/GSTables.h
	GS/GSThread_CXX11.h
	GS/GSThread.h
	GS/GSUtil.h
	GS/GSVector.h
	GS/GSVector4.h
	GS/GSVector4i.h
	GS/GSVector8.h
	GS/GSVector8i.h
	GS/Renderers/Common/GSDevice.h
	GS/Renderers/Common/GSDirtyRect.h
	GS/Renderers/Common/GSFastList.h
	GS/Renderers/Common/GSFunctionMap.h
	GS/Renderers/Common/GSOsdManager.h
	GS/Renderers/Common/GSRenderer.h
	GS/Renderers/Common/GSTexture.h
	GS/Renderers/Common/GSVertex.h
	GS/Renderers/Common/GSVertexList.h
	GS/Renderers/Common/GSVertexTrace.h			
	GS/Renderers/HW/GSRendererHW.h
	GS/Renderers/HW/GSTextureCache.h
	GS/Renderers/HW/GSVertexHW.h												
	GS/Renderers/OpenGL/GLLoader.h
	GS/Renderers/OpenGL/GLState.h
	GS/Renderers/OpenGL/GSDeviceOGL.h
	GS/Renderers/OpenGL/GSRendererOGL.h
	GS/Renderers/OpenGL/GSShaderOGL.h
	GS/Renderers/OpenGL/GSTextureCacheOGL.h
	GS/Renderers/OpenGL/GSTextureOGL.h
	GS/Renderers/OpenGL/GSUniformBufferOGL.h
)

list(APPEND pcsx2SPU2Sources
	SPU2/Linux/CfgHelpers.cpp
	SPU2/Linux/Config.cpp
	SPU2/Linux/ConfigDebug.cpp
	SPU2/Linux/ConfigSoundTouch.cpp
	SPU2/Linux/Dialogs.cpp
	# SPU2/wx/wxConfig.cpp
)
list(APPEND pcsx2SPU2Headers
	SPU2/Linux/Config.h
	SPU2/Linux/Dialogs.h
	# SPU2/wx/wxConfig.h
)

list(APPEND pcsx2DEV9Sources
	DEV9/Linux/Config.cpp
)

																														
set(pcsx2GSResources
	GS/res/fxaa.fx
	GS/res/glsl/common_header.glsl
	GS/res/glsl/convert.glsl
	GS/res/glsl/interlace.glsl
	GS/res/glsl/merge.glsl
	GS/res/glsl/shadeboost.glsl
	GS/res/glsl/tfx_fs.glsl
	GS/res/glsl/tfx_vgs.glsl
)

set(GSBin "${CMAKE_BINARY_DIR}/src/pcsx2/GS")
target_include_directories(PCSX2_FLAGS INTERFACE "${GSBin}")
file(MAKE_DIRECTORY "${GSBin}")

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/src/pcsx2/GS/GS_res.h
	COMMAND glib-compile-resources --sourcedir "${CMAKE_SOURCE_DIR}/src/pcsx2/GS/res" --generate-header
	--c-name GS_res "${CMAKE_SOURCE_DIR}/src/pcsx2/GS/res/GS-res.xml" --target=${CMAKE_BINARY_DIR}/src/pcsx2/GS/GS_res.h
	DEPENDS ${pcsx2GSResources})

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/src/pcsx2/GS/GS_res.cpp
	COMMAND glib-compile-resources --sourcedir "${CMAKE_SOURCE_DIR}/src/pcsx2/GS/res" --generate-source
	--c-name GS_res "${CMAKE_SOURCE_DIR}/src/pcsx2/GS/res/GS-res.xml" --target=${CMAKE_BINARY_DIR}/src/pcsx2/GS/GS_res.cpp
	DEPENDS ${pcsx2GSResources})

list(APPEND pcsx2GSSources
	GS/GS_res.cpp
		)

list(APPEND pcsx2GSHeaders
	GS/GS_res.h
)


set(pcsx2GuiSources
	gui/AppCoreThread.cpp
	gui/main.cpp
)

set(pcsx2GuiHeaders
	gui/AppCoreThread.h
	gui/main.h
)


set(pcsx2IPUSources
	IPU/IPU.cpp
	IPU/IPU_Fifo.cpp
	IPU/IPUdither.cpp
	IPU/IPUdma.cpp
	IPU/mpeg2lib/Idct.cpp
	IPU/mpeg2lib/Mpeg.cpp
	IPU/yuv2rgb.cpp)

set(pcsx2IPUHeaders
	IPU/IPUdma.h
	IPU/IPU_Fifo.h
	IPU/IPU.h
	IPU/mpeg2lib/Mpeg.h
	IPU/mpeg2lib/Vlc.h
	IPU/yuv2rgb.h
	)

set(pcsx2LinuxSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	# Linux/LnxConsolePipe.cpp
	# Linux/LnxKeyCodes.cpp
	Linux/LnxFlatFileReader.cpp
	)

set(pcsx2OSXSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	# Linux/LnxConsolePipe.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

set(pcsx2FreeBSDSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	# Linux/LnxConsolePipe.cpp
	# Linux/LnxKeyCodes.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

set(pcsx2LinuxHeaders
	)

set(pcsx2ps2Sources
	ps2/BiosTools.cpp
	ps2/pgif.cpp
	ps2/LegacyDmac.cpp
	ps2/Iop/IopHwRead.cpp
	ps2/Iop/IopHwWrite.cpp
	ps2/Iop/PsxBios.cpp)

set(pcsx2ps2Headers
	ps2/BiosTools.h
	ps2/eeHwTraceLog.inl
	ps2/pgif.h
	ps2/HwInternal.h
	ps2/Iop/IopHw_Internal.h)





set(pcsx2SystemSources
	System/SysCoreThread.cpp
	System/SysThreadBase.cpp)

set(pcsx2SystemHeaders
	System/RecTypes.h
	System/SysThreads.h)

set(pcsx2UtilitiesSources
	Utilities/FileUtils.cpp)

set(pcsx2UtilitiesHeaders
	Utilities/AsciiFile.h)

set(pcsx2x86Sources
	x86/BaseblockEx.cpp
	x86/iCOP0.cpp
	x86/iCore.cpp
	x86/iFPU.cpp
	x86/iFPUd.cpp
	x86/iMisc.cpp
	x86/iMMI.cpp
	x86/iR3000A.cpp
	x86/iR3000Atables.cpp
	x86/iR5900Misc.cpp
	x86/ir5900tables.cpp
	x86/ix86-32/iCore-32.cpp
	x86/ix86-32/iR5900-32.cpp
	x86/ix86-32/iR5900Arit.cpp
	x86/ix86-32/iR5900AritImm.cpp
	x86/ix86-32/iR5900Branch.cpp
	x86/ix86-32/iR5900Jump.cpp
	x86/ix86-32/iR5900LoadStore.cpp
	x86/ix86-32/iR5900Move.cpp
	x86/ix86-32/iR5900MultDiv.cpp
	x86/ix86-32/iR5900Shift.cpp
	x86/ix86-32/iR5900Templates.cpp
	x86/ix86-32/recVTLB.cpp
	x86/newVif_Dynarec.cpp
	x86/newVif_Unpack.cpp
	x86/newVif_UnpackSSE.cpp
	)

set(pcsx2x86Headers
	x86/BaseblockEx.h
	x86/iCOP0.h
	x86/iCore.h
	x86/iFPU.h
	x86/iMMI.h
	x86/iR3000A.h
	x86/iR5900Arit.h
	x86/iR5900AritImm.h
	x86/iR5900Branch.h
	x86/iR5900.h
	x86/iR5900Jump.h
	x86/iR5900LoadStore.h
	x86/iR5900Move.h
	x86/iR5900MultDiv.h
	x86/iR5900Shift.h
	x86/microVU_Alloc.inl
	x86/microVU_Analyze.inl
	x86/microVU_Branch.inl
	x86/microVU_Clamp.inl
	x86/microVU_Compile.inl
	x86/microVU.cpp
	x86/microVU_Execute.inl
	x86/microVU_Flags.inl
	x86/microVU.h
	x86/microVU_IR.h
	x86/microVU_Log.inl
	x86/microVU_Lower.inl
	x86/microVU_Macro.inl
	x86/microVU_Misc.h
	x86/microVU_Misc.inl
	x86/microVU_Profiler.h
	x86/microVU_Tables.inl
	x86/microVU_Upper.inl
	x86/newVif.h
	x86/newVif_HashBucket.h
	x86/newVif_UnpackSSE.h
	x86/R5900_Profiler.h
	)

set(pcsx2LTOSources
	${pcsx2Sources}
	${pcsx2Headers}
	${pcsx2IPUSources}
	${pcsx2IPUHeaders}
	${pcsx2x86Sources}
	${pcsx2x86Headers}
)

if(LTO_PCSX2_CORE)
	add_library(PCSX2_LTO ${pcsx2LTOSources})
	target_link_libraries(PCSX2_LTO PRIVATE PCSX2_FLAGS)
	target_link_libraries(ps2 PRIVATE PCSX2_LTO)
	set_target_properties(PCSX2_LTO PROPERTIES INTERPROCEDURAL_OPTIMIZATION true)
else()
	target_sources(ps2 PRIVATE ${pcsx2LTOSources})
endif()

target_sources(ps2 PRIVATE
	${pcsx2CDVDSources}
	${pcsx2CDVDHeaders}
	${pcsx2SPU2Sources}
	${pcsx2SPU2Headers}
	${pcsx2DEV9Sources}
	${pcsx2DEV9Headers}
	${pcsx2PADHeaders}
	${pcsx2PADSources}
	${pcsx2GSHeaders}
	${pcsx2GSSources}
	${pcsx2DebugToolsSources}
	${pcsx2DebugToolsHeaders}
	${pcsx2GuiSources}
	${pcsx2GuiHeaders}
	${pcsx2ps2Sources}
	${pcsx2ps2Headers}
	${pcsx2SystemSources}
	${pcsx2SystemHeaders}
	${pcsx2UtilitiesSources}
	${pcsx2UtilitiesHeaders})

target_sources(ps2 PRIVATE
	${pcsx2LinuxSources}
	${pcsx2LinuxHeaders}
	)

target_link_libraries(PCSX2_FLAGS INTERFACE
	PkgConfig::AIO
	PkgConfig::LIBUDEV
	ALSA::ALSA
)

# if(UNIX AND NOT APPLE)
# 	if(X11_API)
# 		target_link_libraries(PCSX2_FLAGS INTERFACE
# 			PkgConfig::X11_XCB
# 			PkgConfig::XCB
# 			X11::X11
# 		)
# 	endif()
# 	if(WAYLAND_API)
# 		target_link_libraries(PCSX2_FLAGS INTERFACE
# 			${WAYLAND_CLIENT_LIBRARIES}
# 		)
# 	endif()
# endif()


if(UNIX AND NOT Linux)
	target_sources(ps2 PRIVATE
		${pcsx2OSXSources}
		${pcsx2LinuxHeaders}
				)
else()
	target_sources(ps2 PRIVATE
		${pcsx2USBSources}
		${pcsx2USBHeaders})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	target_sources(ps2 PRIVATE
		${pcsx2FreeBSDSources}
		${pcsx2LinuxHeaders})
endif()


set(CHDR_LIBRARIES ${CMAKE_SOURCE_DIR}/3rdparty/lib/libchdr-static.a)
set(CHDR_LIBRARIES2 ${CMAKE_SOURCE_DIR}/3rdparty/lib/liblzma.a)
set(GLAD_LIBRARIES ${CMAKE_SOURCE_DIR}/3rdparty/lib/libglad.a)

target_link_libraries(PCSX2_FLAGS INTERFACE
	common
	fmt::fmt
	yaml-cpp
	# chdr-static
	${CHDR_LIBRARIES}
	${CHDR_LIBRARIES2}
	${ZLIB_LIBRARIES}
	${GLAD_LIBRARIES}
	wxWidgets::all
	# ZLIB::ZLIB
	PkgConfig::SOUNDTOUCH
	PkgConfig::SAMPLERATE
	PNG::PNG
	Freetype::Freetype
	LibLZMA::LibLZMA
	${LIBC_LIBRARIES}
)

target_link_libraries(PCSX2_FLAGS INTERFACE
GTK::gtk
HarfBuzz::HarfBuzz
OpenGL::GL
PCAP::PCAP
LibXml2::LibXml2
)

foreach(res_file IN ITEMS
		circlePressed controllerFull controllerHalf controllerThreeQuarters crossPressed downPressed l1Pressed l2Pressed l3Pressed leftPressed
		r1Pressed r2Pressed r3Pressed rightPressed selectPressed squarePressed startPressed trianglePressed upPressed)
	add_custom_command(
		OUTPUT "${res_rec_vp_src}/${res_file}.h"
		DEPENDS ${BIN2CPPDEP} "${res_rec_vp_src}/${res_file}.png"
		COMMAND ${BIN2CPP} "${res_rec_vp_src}/${res_file}.png" "${res_rec_vp_src}/${res_file}"
	)
endforeach()

target_include_directories(PCSX2_FLAGS INTERFACE
	.
	x86
	${CMAKE_BINARY_DIR}/pcsx2
	${CMAKE_BINARY_DIR}/common/include/
	# "${CMAKE_SOURCE_DIR}/3rdparty/jpgd/"
	"${CMAKE_SOURCE_DIR}/3rdparty/xbyak/"
)

# if(COMMAND target_precompile_headers)
# 	message("Using precompiled headers.")
# 	target_precompile_headers(PCSX2_FLAGS INTERFACE PrecompiledHeader.h)
# endif()


source_group(System/Ps2/DEV9 REGULAR_EXPRESSION DEV9/*)
source_group(System/Ps2/PAD FILES ${pcsx2PADSources} ${pcsx2PADHeaders})
source_group(System/Ps2/SPU2 REGULAR_EXPRESSION SPU2/*)
source_group(System/Ps2/USB REGULAR_EXPRESSION USB/*)

source_group(Resources/GUI FILES ${pcsx2GuiResources})
source_group(Resources/PAD FILES ${pcsx2PADResources})
source_group(Resources/Recording FILES)
source_group(Resources FILES ../bin/GameIndex.yaml gui/Resources/PCSX2.icns)
source_group(Resources REGULAR_EXPRESSION ${CMAKE_CURRENT_BINARY_DIR}/*)
