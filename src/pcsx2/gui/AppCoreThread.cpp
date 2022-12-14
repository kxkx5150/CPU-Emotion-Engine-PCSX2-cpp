#include "Core/PrecompiledHeader.h"
#include "main.h"
#include "common/Threading.h"
#include "GS.h"
#include "CDVD/CDVD.h"

#include "R5900Exceptions.h"
#include "Sio.h"
#include "main.h"
#include <memory>
#include "common/EventSource.inl"


__aligned16 SysMtgsThread mtgsThread;
__aligned16 AppCoreThread CoreThread;

extern Pcsx2App *ps2app;

typedef void (AppCoreThread::*FnPtr_CoreThreadMethod)();
static uint m_except_threshold = 0;


ExecutorThread &GetSysExecutorThread()
{
    return ps2app->SysExecutorThread;
}
SysCoreThread &GetCoreThread()
{
    return CoreThread;
}
SysMtgsThread &GetMTGS()
{
    return mtgsThread;
}
static void _Cancel()
{
    GetCoreThread().Cancel();
}
static void _Suspend()
{
    GetCoreThread().Suspend(true);
}


AppCoreThread::AppCoreThread() : SysCoreThread()
{
    m_resetCdvd = false;
}
AppCoreThread::~AppCoreThread()
{
    try {
        _parent::Cancel();
    }
    DESTRUCTOR_CATCHALL
}
void AppCoreThread::Cancel(bool isBlocking)
{
    if (GetSysExecutorThread().IsRunning() && !GetSysExecutorThread().Rpc_TryInvoke(_Cancel, L"AppCoreThread::Cancel"))
        _parent::Cancel(wxTimeSpan(0, 0, 4, 0));
}
void AppCoreThread::Reset()
{
    if (!GetSysExecutorThread().IsSelf()) {
        return;
    }
    _parent::Reset();
}
void AppCoreThread::ResetQuick()
{
    if (!GetSysExecutorThread().IsSelf()) {
        return;
    }
    _parent::ResetQuick();
}
void AppCoreThread::Suspend(bool isBlocking)
{
    if (IsClosed())
        return;
    if (IsSelf()) {
        bool result = GetSysExecutorThread().Rpc_TryInvokeAsync(_Suspend, L"AppCoreThread::Suspend");
        pxAssert(result);
    } else if (!GetSysExecutorThread().Rpc_TryInvoke(_Suspend, L"AppCoreThread::Suspend"))
        _parent::Suspend(true);
}
void AppCoreThread::Resume()
{
    if (!GetSysExecutorThread().IsSelf()) {
        return;
    }
    _parent::Resume();
}
void AppCoreThread::OnResumeReady()
{
    _parent::OnResumeReady();
}
void AppCoreThread::OnPause()
{
    _parent::OnPause();
}
void AppCoreThread::ApplySettings(const Pcsx2Config &src)
{
    Pcsx2Config fixup;
    fixup.CopyConfig(src);
    fixup.Gamefixes.DisableAll();
    gsUpdateFrequency(fixup);

    static int     localc = 0;
    RecursionGuard guard(localc);
    if (guard.IsReentrant())
        return;
    if (fixup == EmuConfig)
        return;
    if (m_ExecMode >= ExecMode_Opened && !IsSelf()) {
        _parent::ApplySettings(fixup);
    } else {
        _parent::ApplySettings(fixup);
    }
    if (m_ExecMode >= ExecMode_Paused)
        GSsetVsync(EmuConfig.GS.GetVsync());
}
void AppCoreThread::DoCpuReset()
{
    _parent::DoCpuReset();
}
void AppCoreThread::OnResumeInThread(SystemsMask systemsToReinstate)
{
    if (m_resetCdvd) {
        CDVDsys_ChangeSource(CDVD_SourceType::Iso);
        DoCDVDopen();
        cdvdCtrlTrayOpen();
        m_resetCdvd = false;
    } else if (systemsToReinstate & System_CDVD)
        DoCDVDopen();
    _parent::OnResumeInThread(systemsToReinstate);
}
void AppCoreThread::OnSuspendInThread()
{
    _parent::OnSuspendInThread();
}
void AppCoreThread::OnCleanupInThread()
{
    m_ExecMode = ExecMode_Closing;
    _parent::OnCleanupInThread();
}
void AppCoreThread::VsyncInThread()
{
    ps2app->LogicalVsync();
    _parent::VsyncInThread();
}
void AppCoreThread::GameStartingInThread()
{
    m_ExecMode = ExecMode_Paused;
    OnResumeReady();
    _reset_stuff_as_needed();
    ClearMcdEjectTimeoutNow();
    m_ExecMode = ExecMode_Opened;
    _parent::GameStartingInThread();
}
bool AppCoreThread::StateCheckInThread()
{
    return _parent::StateCheckInThread();
}
void AppCoreThread::ExecuteTaskInThread()
{
    m_except_threshold = 0;
    _parent::ExecuteTaskInThread();
}
void AppCoreThread::DoCpuExecute()
{
    try {
        _parent::DoCpuExecute();
    } catch (BaseR5900Exception &ex) {
        Console.Error(ex.FormatMessage());
        if (++m_except_threshold > 6) {
            m_except_threshold = 0;
            Console.Error("Too many execution errors.  VM execution has been suspended!");
            m_ExecMode = ExecMode_Closing;
        }
    }
}



wxString SysExecEvent::GetEventName() const
{
    pxFail("Warning: Unnamed SysExecutor Event!  Please overload GetEventName() in all SysExecEvent derived classes.");
    return wxEmptyString;
}
void SysExecEvent::InvokeEvent()
{
}
void SysExecEvent::CleanupEvent()
{
    PostResult();
}
void SysExecEvent::SetException(BaseException *ex)
{
    if (!ex)
        return;
    ex->DiagMsg() += pxsFmt(L"(%s) ", WX_STR(GetEventName()));
}
void SysExecEvent::SetException(const BaseException &ex)
{
    SetException(ex.Clone());
}
void SysExecEvent::_DoInvokeEvent()
{
    try {
        InvokeEvent();
    } catch (BaseException &ex) {
        SetException(ex);
    } catch (std::runtime_error &ex) {
        SetException(new Exception::RuntimeError(ex));
    }
    try {
        CleanupEvent();
    } catch (BaseException &ex) {
        SetException(ex);
    } catch (std::runtime_error &ex) {
        SetException(new Exception::RuntimeError(ex));
    }
}
void SysExecEvent::PostResult() const
{
    if (m_sync)
        m_sync->PostResult();
}


pxEvtQueue::pxEvtQueue() : m_OwnerThreadId(), m_Quitting(false), m_qpc_Start(0)
{
}
void pxEvtQueue::ShutdownQueue()
{
    if (m_Quitting)
        return;
    m_Quitting = true;
    m_wakeup.Post();
}
void pxEvtQueue::ProcessEvents(pxEvtList &list, bool isIdle)
{
    ScopedLock          synclock(m_mtx_pending);
    pxEvtList::iterator node;

    while (node = list.begin(), node != list.end()) {
        std::unique_ptr<SysExecEvent> deleteMe(*node);
        list.erase(node);
        if (!m_Quitting || deleteMe->IsCriticalEvent()) {
            m_qpc_Start = GetCPUTicks();
            synclock.Release();
            if (deleteMe->AllowCancelOnExit())
                deleteMe->_DoInvokeEvent();
            else {
                deleteMe->_DoInvokeEvent();
            }
            synclock.Acquire();
            m_qpc_Start = 0;
        } else {
            deleteMe->PostResult();
        }
    }
}
void pxEvtQueue::ProcessIdleEvents()
{
    ProcessEvents(m_idleEvents, true);
}
void pxEvtQueue::ProcessPendingEvents()
{
    ProcessEvents(m_pendingEvents);
}
void pxEvtQueue::AddPendingEvent(SysExecEvent &evt)
{
    PostEvent(evt);
}
void pxEvtQueue::PostEvent(SysExecEvent *evt)
{
    std::unique_ptr<SysExecEvent> sevt(evt);
    if (!sevt)
        return;
    if (m_Quitting) {
        sevt->PostResult();
        return;
    }
    ScopedLock synclock(m_mtx_pending);
    m_pendingEvents.push_back(sevt.release());
    if (m_pendingEvents.size() == 1)
        m_wakeup.Post();
}
void pxEvtQueue::PostEvent(const SysExecEvent &evt)
{
    PostEvent(evt.Clone());
}
void pxEvtQueue::PostIdleEvent(SysExecEvent *evt)
{
    if (!evt)
        return;
    if (m_Quitting) {
        evt->PostResult();
        return;
    }
    ScopedLock synclock(m_mtx_pending);

    if (m_pendingEvents.empty()) {
        m_pendingEvents.push_back(evt);
        m_wakeup.Post();
    } else
        m_idleEvents.push_back(evt);
}
void pxEvtQueue::PostIdleEvent(const SysExecEvent &evt)
{
    PostIdleEvent(evt.Clone());
}
void pxEvtQueue::ProcessEvent(SysExecEvent &evt)
{
    if (wxThread::GetCurrentId() != m_OwnerThreadId) {
        SynchronousActionState sync;
        evt.SetSyncState(sync);
        PostEvent(evt);
        sync.WaitForResult();
    } else
        evt._DoInvokeEvent();
}
void pxEvtQueue::ProcessEvent(SysExecEvent *evt)
{
    if (!evt)
        return;
    if (wxThread::GetCurrentId() != m_OwnerThreadId) {
        SynchronousActionState sync;
        evt->SetSyncState(sync);
        PostEvent(evt);
        sync.WaitForResult();
    } else {
        std::unique_ptr<SysExecEvent> deleteMe(evt);
        deleteMe->_DoInvokeEvent();
    }
}
bool pxEvtQueue::Rpc_TryInvokeAsync(FnType_Void *method, const wxChar *traceName)
{
    if (wxThread::GetCurrentId() != m_OwnerThreadId) {
        return true;
    }
    return false;
}
bool pxEvtQueue::Rpc_TryInvoke(FnType_Void *method, const wxChar *traceName)
{
    if (wxThread::GetCurrentId() != m_OwnerThreadId) {
        return true;
    }
    return false;
}
void pxEvtQueue::Idle()
{
    ProcessIdleEvents();
    m_wakeup.WaitWithoutYield();
}
void pxEvtQueue::SetActiveThread()
{
    m_OwnerThreadId = wxThread::GetCurrentId();
}



ExecutorThread::ExecutorThread(pxEvtQueue *evthandler) : m_EvtHandler(evthandler)
{
}
bool ExecutorThread::IsRunning() const
{
    if (!m_EvtHandler)
        return false;
    return (!m_EvtHandler->IsShuttingDown());
}
void ExecutorThread::ShutdownQueue()
{
    if (!m_EvtHandler)
        return;
    if (!m_EvtHandler->IsShuttingDown())
        m_EvtHandler->ShutdownQueue();
    Block();
}
void ExecutorThread::PostEvent(SysExecEvent *evt)
{
    if (!pxAssert(m_EvtHandler)) {
        delete evt;
        return;
    }
    m_EvtHandler->PostEvent(evt);
}
void ExecutorThread::PostEvent(const SysExecEvent &evt)
{
    if (!pxAssert(m_EvtHandler))
        return;
    m_EvtHandler->PostEvent(evt);
}
void ExecutorThread::PostIdleEvent(SysExecEvent *evt)
{
    if (!pxAssert(m_EvtHandler))
        return;
    m_EvtHandler->PostIdleEvent(evt);
}
void ExecutorThread::PostIdleEvent(const SysExecEvent &evt)
{
    if (!pxAssert(m_EvtHandler))
        return;
    m_EvtHandler->PostIdleEvent(evt);
}
void ExecutorThread::ProcessEvent(SysExecEvent *evt)
{
    if (m_EvtHandler)
        m_EvtHandler->ProcessEvent(evt);
    else {
        std::unique_ptr<SysExecEvent> deleteMe(evt);
        deleteMe->_DoInvokeEvent();
    }
}
void ExecutorThread::ProcessEvent(SysExecEvent &evt)
{
    if (m_EvtHandler)
        m_EvtHandler->ProcessEvent(evt);
    else
        evt._DoInvokeEvent();
}
void ExecutorThread::OnStart()
{
    m_name = L"SysExecutor";
    _parent::OnStart();
}
void ExecutorThread::ExecuteTaskInThread()
{
    if (!pxAssertDev(m_EvtHandler, "Gimme a damn Event Handler first, object whore."))
        return;
    m_EvtHandler->SetActiveThread();
    while (true) {
        if (!pxAssertDev(m_EvtHandler, "Event handler has been deallocated during SysExecutor thread execution."))
            return;
        m_EvtHandler->Idle();
        m_EvtHandler->ProcessPendingEvents();
        if (m_EvtHandler->IsShuttingDown())
            break;
    }
}
void ExecutorThread::OnCleanupInThread()
{
    _parent::OnCleanupInThread();
}

void SysExecEvent_Execute::InvokeEvent()
{
    CoreThread.ResetQuick();
    CDVDsys_ChangeSource(CDVD_SourceType::Iso);
    CoreThread.Resume();
}
