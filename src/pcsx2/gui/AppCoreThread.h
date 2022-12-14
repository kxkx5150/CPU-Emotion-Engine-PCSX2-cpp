#pragma once
#include "System/SysThreads.h"
#include <memory>
#include "common/PersistentThread.h"
#include <wx/timer.h>
#include <wx/app.h>
#include <wx/event.h>
#include <stack>
#include <string>
#include "common/Threading.h"

class SysExecEvent;

typedef void                      FnType_Void();
typedef std::list<SysExecEvent *> pxEvtList;
typedef std::list<wxEvent *>      wxEventList;


class SynchronousActionState {

  protected:
    bool                 m_posted;
    Threading::Semaphore m_sema;
    ScopedExcept         m_exception;

  public:
    sptr return_value;
    SynchronousActionState()
    {
        m_posted     = false;
        return_value = 0;
    }
    virtual ~SynchronousActionState() = default;

    Threading::Semaphore &GetSemaphore()
    {
        return m_sema;
    }
    const Threading::Semaphore &GetSemaphore() const
    {
        return m_sema;
    }

    void SetException(const BaseException &ex)
    {
        m_exception = ScopedExcept(ex.Clone());
    }
    void SetException(BaseException *ex)
    {
        if (!m_posted) {
            m_exception = ScopedExcept(ex);
        } else if (wxTheApp) {
        }
    }
    void RethrowException() const
    {
        if (m_exception)
            m_exception->Rethrow();
    }
    int WaitForResult()
    {
        m_sema.WaitNoCancel();
        RethrowException();
        return return_value;
    }
    void PostResult(int res)
    {
        return_value = res;
        PostResult();
    }
    void ClearResult()
    {
        m_posted    = false;
        m_exception = NULL;
    }
    void PostResult()
    {
        if (m_posted)
            return;
        m_posted = true;
        m_sema.Post();
    }
};
class SysExecEvent : public ICloneable {
  protected:
    SynchronousActionState *m_sync;

  public:
    virtual ~SysExecEvent() = default;
    SysExecEvent *Clone() const
    {
        return new SysExecEvent(*this);
    }
    SysExecEvent(SynchronousActionState *sync = NULL)
    {
        m_sync = sync;
    }
    SysExecEvent(SynchronousActionState &sync)
    {
        m_sync = &sync;
    }
    const SynchronousActionState *GetSyncState() const
    {
        return m_sync;
    }
    SynchronousActionState *GetSyncState()
    {
        return m_sync;
    }
    SysExecEvent &SetSyncState(SynchronousActionState *obj)
    {
        m_sync = obj;
        return *this;
    }
    SysExecEvent &SetSyncState(SynchronousActionState &obj)
    {
        m_sync = &obj;
        return *this;
    }
    virtual bool IsCriticalEvent() const
    {
        return false;
    }
    virtual bool AllowCancelOnExit() const
    {
        return true;
    }
    virtual void     _DoInvokeEvent();
    virtual void     PostResult() const;
    virtual wxString GetEventName() const;
    virtual int      GetResult()
    {
        if (!pxAssertDev(m_sync != NULL, "SysEvent: Expecting return value, but no sync object provided."))
            return 0;
        return m_sync->return_value;
    }
    virtual void SetException(BaseException *ex);
    void         SetException(const BaseException &ex);

  protected:
    virtual void InvokeEvent();
    virtual void CleanupEvent();
};
class pxEvtQueue {
  protected:
    pxEvtList                 m_pendingEvents;
    pxEvtList                 m_idleEvents;
    Threading::MutexRecursive m_mtx_pending;
    Threading::Semaphore      m_wakeup;
    wxThreadIdType            m_OwnerThreadId;
    std::atomic<bool>         m_Quitting;
    u64                       m_qpc_Start;

  public:
    pxEvtQueue();
    virtual ~pxEvtQueue() = default;
    virtual wxString GetEventHandlerName() const
    {
        return L"pxEvtQueue";
    }
    virtual void ShutdownQueue();
    bool         IsShuttingDown() const
    {
        return !!m_Quitting;
    }
    void ProcessEvents(pxEvtList &list, bool isIdle = false);
    void ProcessPendingEvents();
    void ProcessIdleEvents();
    void Idle();
    void AddPendingEvent(SysExecEvent &evt);
    void PostEvent(SysExecEvent *evt);
    void PostEvent(const SysExecEvent &evt);
    void PostIdleEvent(SysExecEvent *evt);
    void PostIdleEvent(const SysExecEvent &evt);
    void ProcessEvent(SysExecEvent *evt);
    void ProcessEvent(SysExecEvent &evt);
    bool Rpc_TryInvokeAsync(FnType_Void *method, const wxChar *traceName = NULL);
    bool Rpc_TryInvoke(FnType_Void *method, const wxChar *traceName = NULL);
    void SetActiveThread();

  protected:
};
class ExecutorThread : public Threading::pxThread {
    typedef Threading::pxThread _parent;

  protected:
    std::unique_ptr<wxTimer>    m_ExecutorTimer;
    std::unique_ptr<pxEvtQueue> m_EvtHandler;

  public:
    ExecutorThread(pxEvtQueue *evtandler = NULL);
    virtual ~ExecutorThread() = default;
    virtual void ShutdownQueue();

    bool IsRunning() const;
    void PostEvent(SysExecEvent *evt);
    void PostEvent(const SysExecEvent &evt);
    void PostIdleEvent(SysExecEvent *evt);
    void PostIdleEvent(const SysExecEvent &evt);
    void ProcessEvent(SysExecEvent *evt);
    void ProcessEvent(SysExecEvent &evt);

    bool Rpc_TryInvokeAsync(void (*evt)(), const wxChar *traceName = NULL)
    {
        return m_EvtHandler ? m_EvtHandler->Rpc_TryInvokeAsync(evt, traceName) : false;
    }
    bool Rpc_TryInvoke(void (*evt)(), const wxChar *traceName = NULL)
    {
        return m_EvtHandler ? m_EvtHandler->Rpc_TryInvoke(evt, traceName) : false;
    }

  protected:
    void OnStart();
    void ExecuteTaskInThread();
    void OnCleanupInThread();
};
class AppCoreThread : public SysCoreThread {
    typedef SysCoreThread _parent;

  protected:
    std::atomic<bool> m_resetCdvd;

  public:
    AppCoreThread();
    virtual ~AppCoreThread();
    void ResetCdvd()
    {
        m_resetCdvd = true;
    }
    virtual void Suspend(bool isBlocking = false) override;
    virtual void Resume() override;
    virtual void Reset() override;
    virtual void ResetQuick() override;
    virtual void Cancel(bool isBlocking = true) override;
    virtual bool StateCheckInThread() override;
    virtual void ApplySettings(const Pcsx2Config &src) override;

  protected:
    virtual void DoCpuExecute() override;
    virtual void OnResumeReady() override;
    virtual void OnPause() override;
    virtual void OnResumeInThread(SystemsMask systemsToReinstate) override;
    virtual void OnSuspendInThread() override;
    virtual void OnCleanupInThread() override;
    virtual void VsyncInThread() override;
    virtual void GameStartingInThread() override;
    virtual void ExecuteTaskInThread() override;
    virtual void DoCpuReset() override;
};
class SysExecEvent_Execute : public SysExecEvent {
  public:
    virtual ~SysExecEvent_Execute() = default;

    SysExecEvent_Execute *Clone() const
    {
        return new SysExecEvent_Execute(*this);
    }

    wxString GetEventName() const
    {
        return L"SysExecute";
    }

  protected:
    void InvokeEvent();
};