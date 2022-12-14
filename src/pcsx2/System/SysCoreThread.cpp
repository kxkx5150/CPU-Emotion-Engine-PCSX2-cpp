
#include "Core/PrecompiledHeader.h"
#include "Common.h"
#include "gui/main.h"
#include "IopBios.h"
#include "R5900.h"

#include "common/WindowInfo.h"
// extern WindowInfo g_gs_window_info;

#include "Counters.h"
#include "GS.h"
#include "Elfheader.h"

#include "SysThreads.h"
#include "MTVU.h"
#include "IPC.h"
#include "FW.h"
#include "SPU2/spu2.h"
#include "DEV9/DEV9.h"
#include "MemoryCardFile.h"
#ifdef _WIN32
#include "PAD/Windows/PAD.h"
#else
#include "PAD/Linux/PAD.h"
#endif


#include "common/PageFaultSource.h"
#include "common/Threading.h"
#include "IopBios.h"

#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif

#include "common/emitter/x86_intrin.h"

bool g_CDVDReset = false;

namespace IPCSettings {
unsigned int slot = IPC_DEFAULT_SLOT;
};


SysCoreThread::SysCoreThread()
{
    m_name                = L"EE Core";
    m_resetRecompilers    = true;
    m_resetProfilers      = true;
    m_resetVsyncTimers    = true;
    m_resetVirtualMachine = true;

    m_hasActiveMachine = false;
}

SysCoreThread::~SysCoreThread()
{
    try {
        SysCoreThread::Cancel();
    }
    DESTRUCTOR_CATCHALL
}

void SysCoreThread::Cancel(bool isBlocking)
{
    m_hasActiveMachine = false;
    R3000A::ioman::reset();
    _parent::Cancel();
}

bool SysCoreThread::Cancel(const wxTimeSpan &span)
{
    m_hasActiveMachine = false;
    R3000A::ioman::reset();
    return _parent::Cancel(span);
}

void SysCoreThread::OnStart()
{
    _parent::OnStart();
}

void SysCoreThread::OnSuspendInThread()
{
    TearDownSystems(static_cast<SystemsMask>(-1));
    GetMTGS().Suspend();
}

void SysCoreThread::Start()
{
    GSinit();
    SPU2init();
    PADinit();
    DEV9init();
    _parent::Start();
}

void SysCoreThread::OnResumeReady()
{
    if (m_resetVirtualMachine)
        m_hasActiveMachine = false;

    if (!m_hasActiveMachine)
        m_resetRecompilers = true;
}

void SysCoreThread::SetElfOverride(const wxString &elf)
{
    m_elf_override = elf;


    Hle_SetElfPath(elf.ToUTF8());
}

void SysCoreThread::ResetQuick()
{
    Suspend();

    m_resetVirtualMachine = true;
    m_hasActiveMachine    = false;
    R3000A::ioman::reset();
}

void SysCoreThread::Reset()
{
    ResetQuick();
    GetVmMemory().DecommitAll();
    SysClearExecutionCache();
    g_FrameCount = 0;
}


void SysCoreThread::ApplySettings(const Pcsx2Config &src)
{
    if (src == EmuConfig)
        return;

    if (!pxAssertDev(IsPaused() | IsSelf(), "CoreThread is not paused; settings cannot be applied."))
        return;

    m_resetRecompilers = (src.Cpu != EmuConfig.Cpu) || (src.Gamefixes != EmuConfig.Gamefixes) ||
                         (src.Speedhacks != EmuConfig.Speedhacks);
    m_resetProfilers   = (src.Profiler != EmuConfig.Profiler);
    m_resetVsyncTimers = (src.GS != EmuConfig.GS);

    EmuConfig.CopyConfig(src);
}

bool SysCoreThread::HasPendingStateChangeRequest() const
{
    return !m_hasActiveMachine || GetMTGS().HasPendingException() || _parent::HasPendingStateChangeRequest();
}

void SysCoreThread::_reset_stuff_as_needed()
{

    GetVmMemory().CommitAll();

    if (m_resetVirtualMachine || m_resetRecompilers || m_resetProfilers) {
        SysClearExecutionCache();
        memBindConditionalHandlers();
        SetCPUState(EmuConfig.Cpu.sseMXCSR, EmuConfig.Cpu.sseVUMXCSR);

        m_resetRecompilers = false;
        m_resetProfilers   = false;
    }

    if (m_resetVirtualMachine) {
        DoCpuReset();

        m_resetVirtualMachine = false;
        m_resetVsyncTimers    = false;
    }

    if (m_resetVsyncTimers) {
        UpdateVSyncRate();
        frameLimitReset();

        m_resetVsyncTimers = false;
    }
}

void SysCoreThread::DoCpuReset()
{
    AffinityAssert_AllowFromSelf(pxDiagSpot);
    cpuReset();
}

void SysCoreThread::VsyncInThread()
{
}

void SysCoreThread::GameStartingInThread()
{
    GetMTGS().SendGameCRC(ElfCRC);

    if (EmuConfig.EnableIPC && m_IpcState == OFF) {
        m_IpcState  = ON;
        m_socketIpc = std::make_unique<SocketIPC>(this, IPCSettings::slot);
    }
    if (m_IpcState == ON && m_socketIpc->m_end)
        m_socketIpc->Start();
}

bool SysCoreThread::StateCheckInThread()
{
    GetMTGS().RethrowException();
    return _parent::StateCheckInThread() && (_reset_stuff_as_needed(), true);
}

void SysCoreThread::DoCpuExecute()
{
    m_hasActiveMachine = true;
    // UI_EnableSysActions();
    Cpu->Execute();
}

void SysCoreThread::ExecuteTaskInThread()
{
    Threading::EnableHiresScheduler();
    m_sem_event.WaitWithoutYield();

    m_mxcsr_saved.bitmask = _mm_getcsr();

    PCSX2_PAGEFAULT_PROTECT
    {
        while (true) {
            StateCheckInThread();
            DoCpuExecute();
        }
    }
    PCSX2_PAGEFAULT_EXCEPT;
}

void SysCoreThread::TearDownSystems(SystemsMask systemsToTearDown)
{
    if (systemsToTearDown & System_DEV9)
        DEV9close();
    if (systemsToTearDown & System_CDVD)
        DoCDVDclose();
    if (systemsToTearDown & System_FW)
        FWclose();
    if (systemsToTearDown & System_PAD)
        PADclose();
    if (systemsToTearDown & System_SPU2)
        SPU2close();
    if (systemsToTearDown & System_MCD)
        FileMcd_EmuClose();
}

void SysCoreThread::OnResumeInThread(SystemsMask systemsToReinstate)
{
    GetMTGS().WaitForOpen();
    if (systemsToReinstate & System_DEV9)
        DEV9open();
    if (systemsToReinstate & System_FW)
        FWopen();
    if (systemsToReinstate & System_SPU2)
        SPU2open();
    // if (systemsToReinstate & System_PAD)
    //     PADopen(g_gs_window_info);
    if (systemsToReinstate & System_MCD)
        FileMcd_EmuOpen();
}


void SysCoreThread::OnCleanupInThread()
{
    m_ExecMode = ExecMode_Closing;

    m_hasActiveMachine    = false;
    m_resetVirtualMachine = true;

    R3000A::ioman::reset();
    vu1Thread.WaitVU();
    SPU2close();
    PADclose();
    DEV9close();
    DoCDVDclose();
    FWclose();
    FileMcd_EmuClose();
    GetMTGS().Suspend();
    SPU2shutdown();
    PADshutdown();
    DEV9shutdown();
    GetMTGS().Cancel();

    _mm_setcsr(m_mxcsr_saved.bitmask);
    Threading::DisableHiresScheduler();
    _parent::OnCleanupInThread();

    m_ExecMode = ExecMode_NoThreadYet;
}
