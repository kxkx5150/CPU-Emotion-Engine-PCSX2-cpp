
#pragma once

#include "common/emitter/tools.h"
#include "common/General.h"
#include "common/Path.h"
#include <string>

class SettingsInterface;
class SettingsWrapper;

enum class CDVD_SourceType : uint8_t;

enum GamefixId
{
    GamefixId_FIRST = 0,

    Fix_FpuMultiply = GamefixId_FIRST,
    Fix_FpuNegDiv,
    Fix_GoemonTlbMiss,
    Fix_SkipMpeg,
    Fix_OPHFlag,
    Fix_EETiming,
    Fix_DMABusy,
    Fix_GIFFIFO,
    Fix_VIFFIFO,
    Fix_VIF1Stall,
    Fix_VuAddSub,
    Fix_Ibit,
    Fix_VUKickstart,
    Fix_VUOverflow,
    Fix_XGKick,

    GamefixId_COUNT
};


enum SpeedhackId
{
    SpeedhackId_FIRST = 0,

    Speedhack_mvuFlag = SpeedhackId_FIRST,
    Speedhack_InstantVU1,

    SpeedhackId_COUNT
};

enum class VsyncMode
{
    Off,
    On,
    Adaptive,
};

enum class AspectRatioType : u8
{
    Stretch,
    R4_3,
    R16_9,
    MaxCount
};

enum class FMVAspectRatioSwitchType : u8
{
    Off,
    R4_3,
    R16_9,
    MaxCount
};

enum class MemoryCardType
{
    Empty,
    File,
    Folder,
    MaxCount
};

enum class LimiterModeType : u8
{
    Nominal,
    Turbo,
    Slomo,
};

template <typename Enumeration> typename std::underlying_type<Enumeration>::type enum_cast(Enumeration E)
{
    return static_cast<typename std::underlying_type<Enumeration>::type>(E);
}

ImplementEnumOperators(GamefixId);
ImplementEnumOperators(SpeedhackId);

#define DEFAULT_sseMXCSR   0xffc0
#define DEFAULT_sseVUMXCSR 0xffc0

struct TraceFiltersEE
{
    BITFIELD32()
    bool m_EnableAll : 1, m_EnableDisasm : 1, m_EnableRegisters : 1, m_EnableEvents : 1;
    BITFIELD_END

    TraceFiltersEE()
    {
        bitset = 0;
    }

    bool operator==(const TraceFiltersEE &right) const
    {
        return OpEqu(bitset);
    }

    bool operator!=(const TraceFiltersEE &right) const
    {
        return !this->operator==(right);
    }
};

struct TraceFiltersIOP
{
    BITFIELD32()
    bool m_EnableAll : 1, m_EnableDisasm : 1, m_EnableRegisters : 1, m_EnableEvents : 1;
    BITFIELD_END

    TraceFiltersIOP()
    {
        bitset = 0;
    }

    bool operator==(const TraceFiltersIOP &right) const
    {
        return OpEqu(bitset);
    }

    bool operator!=(const TraceFiltersIOP &right) const
    {
        return !this->operator==(right);
    }
};

struct TraceLogFilters
{
    bool Enabled;

    TraceFiltersEE  EE;
    TraceFiltersIOP IOP;

    TraceLogFilters()
    {
        Enabled = false;
    }

    void LoadSave(SettingsWrapper &ini);

    bool operator==(const TraceLogFilters &right) const
    {
        return OpEqu(Enabled) && OpEqu(EE) && OpEqu(IOP);
    }

    bool operator!=(const TraceLogFilters &right) const
    {
        return !this->operator==(right);
    }
};

struct Pcsx2Config
{
    struct ProfilerOptions
    {
        BITFIELD32()
        bool Enabled : 1, RecBlocks_EE : 1, RecBlocks_IOP : 1, RecBlocks_VU0 : 1, RecBlocks_VU1 : 1;
        BITFIELD_END

        ProfilerOptions() : bitset(0xfffffffe)
        {
        }
        void LoadSave(SettingsWrapper &wrap);

        bool operator==(const ProfilerOptions &right) const
        {
            return OpEqu(bitset);
        }

        bool operator!=(const ProfilerOptions &right) const
        {
            return !OpEqu(bitset);
        }
    };

    struct RecompilerOptions
    {
        BITFIELD32()
        bool EnableEE : 1, EnableIOP : 1, EnableVU0 : 1, EnableVU1 : 1;

        bool vuOverflow : 1, vuExtraOverflow : 1, vuSignOverflow : 1, vuUnderflow : 1;

        bool fpuOverflow : 1, fpuExtraOverflow : 1, fpuFullMode : 1;

        bool StackFrameChecks : 1, PreBlockCheckEE : 1, PreBlockCheckIOP : 1;
        bool EnableEECache : 1;
        BITFIELD_END

        RecompilerOptions();
        void ApplySanityCheck();

        void LoadSave(SettingsWrapper &wrap);

        bool operator==(const RecompilerOptions &right) const
        {
            return OpEqu(bitset);
        }

        bool operator!=(const RecompilerOptions &right) const
        {
            return !OpEqu(bitset);
        }
    };

    struct CpuOptions
    {
        RecompilerOptions Recompiler;

        SSE_MXCSR sseMXCSR;
        SSE_MXCSR sseVUMXCSR;

        CpuOptions();
        void LoadSave(SettingsWrapper &wrap);
        void ApplySanityCheck();

        bool operator==(const CpuOptions &right) const
        {
            return OpEqu(sseMXCSR) && OpEqu(sseVUMXCSR) && OpEqu(Recompiler);
        }

        bool operator!=(const CpuOptions &right) const
        {
            return !this->operator==(right);
        }
    };

    struct GSOptions
    {
        int VsyncQueueSize{2};

        bool SynchronousMTGS{false};
        bool FrameLimitEnable{true};
        bool FrameSkipEnable{false};

        VsyncMode VsyncEnable{VsyncMode::Off};

        int FramesToDraw{2};
        int FramesToSkip{2};

        double LimitScalar{1.0};
        double FramerateNTSC{59.94};
        double FrameratePAL{50.00};

        AspectRatioType          AspectRatio{AspectRatioType::R4_3};
        FMVAspectRatioSwitchType FMVAspectRatioSwitch{FMVAspectRatioSwitchType::Off};

        double Zoom{100.0};
        double StretchY{100.0};
        double OffsetX{0.0};
        double OffsetY{0.0};

        void LoadSave(SettingsWrapper &wrap);

        int GetVsync() const;

        bool operator==(const GSOptions &right) const
        {
            return OpEqu(SynchronousMTGS) && OpEqu(VsyncQueueSize) &&

                   OpEqu(FrameSkipEnable) && OpEqu(FrameLimitEnable) && OpEqu(VsyncEnable) &&

                   OpEqu(LimitScalar) && OpEqu(FramerateNTSC) && OpEqu(FrameratePAL) &&

                   OpEqu(FramesToDraw) && OpEqu(FramesToSkip) &&

                   OpEqu(AspectRatio) && OpEqu(FMVAspectRatioSwitch) &&

                   OpEqu(Zoom) && OpEqu(StretchY) && OpEqu(OffsetX) && OpEqu(OffsetY);
        }

        bool operator!=(const GSOptions &right) const
        {
            return !this->operator==(right);
        }
    };

    struct GamefixOptions
    {
        BITFIELD32()
        bool FpuMulHack : 1, FpuNegDivHack : 1, GoemonTlbHack : 1, SkipMPEGHack : 1, OPHFlagHack : 1, EETimingHack : 1,
            DMABusyHack : 1, GIFFIFOHack : 1, VIFFIFOHack : 1, VIF1StallHack : 1, VuAddSubHack : 1, IbitHack : 1,
            VUKickstartHack : 1, VUOverflowHack : 1, XgKickHack : 1;
        BITFIELD_END

        GamefixOptions();
        void            LoadSave(SettingsWrapper &wrap);
        GamefixOptions &DisableAll();

        void Set(const wxString &list, bool enabled = true);
        void Clear(const wxString &list)
        {
            Set(list, false);
        }

        bool Get(GamefixId id) const;
        void Set(GamefixId id, bool enabled = true);
        void Clear(GamefixId id)
        {
            Set(id, false);
        }

        bool operator==(const GamefixOptions &right) const
        {
            return OpEqu(bitset);
        }

        bool operator!=(const GamefixOptions &right) const
        {
            return !OpEqu(bitset);
        }
    };

    struct SpeedhackOptions
    {
        BITFIELD32()
        bool fastCDVD : 1, IntcStat : 1, WaitLoop : 1, vuFlagHack : 1, vuThread : 1, vu1Instant : 1;
        BITFIELD_END

        s8 EECycleRate;
        u8 EECycleSkip;

        SpeedhackOptions();
        void              LoadSave(SettingsWrapper &conf);
        SpeedhackOptions &DisableAll();

        void Set(SpeedhackId id, bool enabled = true);

        bool operator==(const SpeedhackOptions &right) const
        {
            return OpEqu(bitset) && OpEqu(EECycleRate) && OpEqu(EECycleSkip);
        }

        bool operator!=(const SpeedhackOptions &right) const
        {
            return !this->operator==(right);
        }
    };

    struct DebugOptions
    {
        BITFIELD32()
        bool ShowDebuggerOnStart : 1;
        bool AlignMemoryWindowStart : 1;
        BITFIELD_END

        u8  FontWidth;
        u8  FontHeight;
        u32 WindowWidth;
        u32 WindowHeight;
        u32 MemoryViewBytesPerRow;

        DebugOptions();
        void LoadSave(SettingsWrapper &wrap);

        bool operator==(const DebugOptions &right) const
        {
            return OpEqu(bitset) && OpEqu(FontWidth) && OpEqu(FontHeight) && OpEqu(WindowWidth) &&
                   OpEqu(WindowHeight) && OpEqu(MemoryViewBytesPerRow);
        }

        bool operator!=(const DebugOptions &right) const
        {
            return !this->operator==(right);
        }
    };

    struct FramerateOptions
    {
        bool SkipOnLimit{false};
        bool SkipOnTurbo{false};

        double NominalScalar{1.0};
        double TurboScalar{2.0};
        double SlomoScalar{0.5};

        void LoadSave(SettingsWrapper &wrap);
        void SanityCheck();

        bool operator==(const FramerateOptions &right) const
        {
            return OpEqu(SkipOnLimit) && OpEqu(SkipOnTurbo) && OpEqu(NominalScalar) && OpEqu(TurboScalar) &&
                   OpEqu(SlomoScalar);
        }

        bool operator!=(const FramerateOptions &right) const
        {
            return !this->operator==(right);
        }
    };

    struct FilenameOptions
    {
        std::string Bios;

        FilenameOptions();
        void LoadSave(SettingsWrapper &wrap);

        bool operator==(const FilenameOptions &right) const
        {
            return OpEqu(Bios);
        }

        bool operator!=(const FilenameOptions &right) const
        {
            return !this->operator==(right);
        }
    };

    struct McdOptions
    {
        std::string    Filename;
        bool           Enabled;
        MemoryCardType Type;
    };

    BITFIELD32()
    bool CdvdVerboseReads : 1, CdvdDumpBlocks : 1, CdvdShareWrite : 1, EnablePatches : 1, EnableCheats : 1,
        EnableIPC : 1, EnableWideScreenPatches : 1, UseBOOT2Injection : 1, BackupSavestate : 1, McdEnableEjection : 1,
        McdFolderAutoManage : 1,

        MultitapPort0_Enabled : 1, MultitapPort1_Enabled : 1,

        ConsoleToStdio : 1, HostFs : 1;

#ifdef __WXMSW__
    bool McdCompressNTFS;
#endif
    BITFIELD_END

    CpuOptions       Cpu;
    GSOptions        GS;
    SpeedhackOptions Speedhacks;
    GamefixOptions   Gamefixes;
    ProfilerOptions  Profiler;
    DebugOptions     Debugger;
    FramerateOptions Framerate;

    TraceLogFilters Trace;

    FilenameOptions BaseFilenames;

    McdOptions  Mcd[8];
    std::string GzipIsoIndexTemplate;

    std::string     CurrentBlockdump;
    std::string     CurrentIRX;
    std::string     CurrentGameArgs;
    AspectRatioType CurrentAspectRatio = AspectRatioType::R4_3;
    LimiterModeType LimiterMode        = LimiterModeType::Nominal;

    Pcsx2Config();
    void LoadSave(SettingsWrapper &wrap);

    wxString FullpathToBios() const;
    wxString FullpathToMcd(uint slot) const;

    bool MultitapEnabled(uint port) const;

    bool operator==(const Pcsx2Config &right) const;
    bool operator!=(const Pcsx2Config &right) const
    {
        return !this->operator==(right);
    }

    void CopyConfig(const Pcsx2Config &cfg);
};

extern Pcsx2Config EmuConfig;

namespace EmuFolders {
extern wxDirName Settings;
extern wxDirName Bios;
extern wxDirName Snapshots;
extern wxDirName Savestates;
extern wxDirName MemoryCards;
extern wxDirName Langs;
extern wxDirName Logs;
extern wxDirName Cheats;
extern wxDirName CheatsWS;
}    // namespace EmuFolders



#define THREAD_VU1   (EmuConfig.Cpu.Recompiler.EnableVU1 && EmuConfig.Speedhacks.vuThread)
#define INSTANT_VU1  (EmuConfig.Speedhacks.vu1Instant)
#define CHECK_EEREC  (EmuConfig.Cpu.Recompiler.EnableEE)
#define CHECK_CACHE  (EmuConfig.Cpu.Recompiler.EnableEECache)
#define CHECK_IOPREC (EmuConfig.Cpu.Recompiler.EnableIOP)

#define CHECK_VUADDSUBHACK   (EmuConfig.Gamefixes.VuAddSubHack)
#define CHECK_FPUMULHACK     (EmuConfig.Gamefixes.FpuMulHack)
#define CHECK_FPUNEGDIVHACK  (EmuConfig.Gamefixes.FpuNegDivHack)
#define CHECK_XGKICKHACK     (EmuConfig.Gamefixes.XgKickHack)
#define CHECK_EETIMINGHACK   (EmuConfig.Gamefixes.EETimingHack)
#define CHECK_SKIPMPEGHACK   (EmuConfig.Gamefixes.SkipMPEGHack)
#define CHECK_OPHFLAGHACK    (EmuConfig.Gamefixes.OPHFlagHack)
#define CHECK_DMABUSYHACK    (EmuConfig.Gamefixes.DMABusyHack)
#define CHECK_VIFFIFOHACK    (EmuConfig.Gamefixes.VIFFIFOHack)
#define CHECK_VIF1STALLHACK  (EmuConfig.Gamefixes.VIF1StallHack)
#define CHECK_GIFFIFOHACK    (EmuConfig.Gamefixes.GIFFIFOHack)
#define CHECK_VUOVERFLOWHACK (EmuConfig.Gamefixes.VUOverflowHack)

#define CHECK_VU_OVERFLOW       (EmuConfig.Cpu.Recompiler.vuOverflow)
#define CHECK_VU_EXTRA_OVERFLOW (EmuConfig.Cpu.Recompiler.vuExtraOverflow)
#define CHECK_VU_SIGN_OVERFLOW  (EmuConfig.Cpu.Recompiler.vuSignOverflow)
#define CHECK_VU_UNDERFLOW      (EmuConfig.Cpu.Recompiler.vuUnderflow)
#define CHECK_VU_EXTRA_FLAGS    0

#define CHECK_FPU_OVERFLOW       (EmuConfig.Cpu.Recompiler.fpuOverflow)
#define CHECK_FPU_EXTRA_OVERFLOW (EmuConfig.Cpu.Recompiler.fpuExtraOverflow)
#define CHECK_FPU_EXTRA_FLAGS    1
#define CHECK_FPU_FULL           (EmuConfig.Cpu.Recompiler.fpuFullMode)


#define SHIFT_RECOMPILE
#define BRANCH_RECOMPILE

#define ARITHMETICIMM_RECOMPILE
#define ARITHMETIC_RECOMPILE
#define MULTDIV_RECOMPILE
#define JUMP_RECOMPILE
#define LOADSTORE_RECOMPILE
#define MOVE_RECOMPILE
#define MMI_RECOMPILE
#define MMI0_RECOMPILE
#define MMI1_RECOMPILE
#define MMI2_RECOMPILE
#define MMI3_RECOMPILE
#define FPU_RECOMPILE
#define CP0_RECOMPILE
#define CP2_RECOMPILE

#ifndef ARITHMETIC_RECOMPILE
#undef ARITHMETICIMM_RECOMPILE
#endif

#define EE_CONST_PROP 1

#define PSX_EXTRALOGS 0
