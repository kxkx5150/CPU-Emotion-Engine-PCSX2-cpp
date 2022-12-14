
#include "Core/PrecompiledHeader.h"
#include "Common.h"

#include <list>
#include <wx/datetime.h>

#include "GS.h"
#include "Gif_Unit.h"
#include "MTVU.h"
#include "Elfheader.h"
#include "gui/main.h"
// #include "gui/AppCoreThread.h"
#include "common/WindowInfo.h"
extern WindowInfo g_gs_window_info;


using namespace Threading;

#if 0
#define MTGS_LOG Console.WriteLn
#else
#define MTGS_LOG(...)                                                                                                  \
    do {                                                                                                               \
    } while (0)
#endif


__aligned(32) MTGS_BufferedData RingBuffer;

#ifdef RINGBUF_DEBUG_STACK
#include <list>
std::list<uint> ringposStack;
#endif

SysMtgsThread::SysMtgsThread()
    : SysThreadBase()
#ifdef RINGBUF_DEBUG_STACK
      ,
      m_lock_Stack()
#endif
{
    m_name = L"MTGS";
}

typedef void (SysMtgsThread::*FnPtr_MtgsThreadMethod)();

class SysExecEvent_InvokeMtgsThreadMethod : public SysExecEvent {
  protected:
    FnPtr_MtgsThreadMethod m_method;
    bool                   m_IsCritical;

  public:
    wxString GetEventName() const
    {
        return L"MtgsThreadMethod";
    }
    virtual ~SysExecEvent_InvokeMtgsThreadMethod() = default;
    SysExecEvent_InvokeMtgsThreadMethod *Clone() const
    {
        return new SysExecEvent_InvokeMtgsThreadMethod(*this);
    }

    bool AllowCancelOnExit() const
    {
        return false;
    }
    bool IsCriticalEvent() const
    {
        return m_IsCritical;
    }

    SysExecEvent_InvokeMtgsThreadMethod(FnPtr_MtgsThreadMethod method, bool critical = false)
    {
        m_method     = method;
        m_IsCritical = critical;
    }

    SysExecEvent_InvokeMtgsThreadMethod &Critical()
    {
        m_IsCritical = true;
        return *this;
    }

  protected:
    void InvokeEvent()
    {
        if (m_method)
            (mtgsThread.*m_method)();
    }
};

void SysMtgsThread::OnStart()
{
    m_Opened = false;

    m_ReadPos          = 0;
    m_WritePos         = 0;
    m_RingBufferIsBusy = false;
    m_packet_size      = 0;
    m_packet_writepos  = 0;

    m_QueuedFrameCount    = 0;
    m_VsyncSignalListener = false;
    m_SignalRingEnable    = false;
    m_SignalRingPosition  = 0;

    m_CopyDataTally = 0;

    _parent::OnStart();
}

SysMtgsThread::~SysMtgsThread()
{
    try {
        _parent::Cancel();
    }
    DESTRUCTOR_CATCHALL
}

void SysMtgsThread::OnResumeReady()
{
    m_sem_OpenDone.Reset();
}

void SysMtgsThread::ResetGS()
{
    pxAssertDev(!IsOpen() || (m_ReadPos == m_WritePos), "Must close or terminate the GS thread prior to gsReset.");


    m_ReadPos             = m_WritePos.load();
    m_QueuedFrameCount    = 0;
    m_VsyncSignalListener = 0;

    MTGS_LOG("MTGS: Sending Reset...");
    SendSimplePacket(GS_RINGTYPE_RESET, 0, 0, 0);
    SendSimplePacket(GS_RINGTYPE_FRAMESKIP, 0, 0, 0);
    SetEvent();
}

struct RingCmdPacket_Vsync
{
    u8           regset1[0x0f0];
    u32          csr;
    u32          imr;
    GSRegSIGBLID siglblid;
};

void SysMtgsThread::PostVsyncStart()
{

    uint packsize = sizeof(RingCmdPacket_Vsync) / 16;
    PrepDataPacket(GS_RINGTYPE_VSYNC, packsize);
    MemCopy_WrappedDest((u128 *)PS2MEM_GS, RingBuffer.m_Ring, m_packet_writepos, RingBufferSize, 0xf);

    u32 *remainder               = (u32 *)GetDataPacketPtr();
    remainder[0]                 = GSCSRr;
    remainder[1]                 = GSIMR._u32;
    (GSRegSIGBLID &)remainder[2] = GSSIGLBLID;
    m_packet_writepos            = (m_packet_writepos + 1) & RingBufferMask;

    SendDataPacket();

    if (m_CopyDataTally != 0)
        SetEvent();



    if ((m_QueuedFrameCount.fetch_add(1) < EmuConfig.GS.VsyncQueueSize))
        return;

    m_VsyncSignalListener.store(true, std::memory_order_release);

    m_sem_event.Post();

    m_sem_Vsync.WaitNoCancel();
}

union PacketTagType
{
    struct
    {
        u32 command;
        u32 data[3];
    };
    struct
    {
        u32  _command;
        u32  _data[1];
        uptr pointer;
    };
};


extern Pcsx2App *ps2app;

void SysMtgsThread::OpenGS()
{
    if (m_Opened)
        return;

    // if (init_gspanel)
    ps2app->OpenGsPanel();

    memcpy(RingBuffer.Regs, PS2MEM_GS, sizeof(PS2MEM_GS));
    GSsetBaseMem(RingBuffer.Regs);
    GSopen2(g_gs_window_info, 0);
    GSsetVsync(EmuConfig.GS.GetVsync());

    m_Opened = true;
    m_sem_OpenDone.Post();

    GSsetGameCRC(ElfCRC, 0);
}

class RingBufferLock {
    ScopedLock     m_lock1;
    ScopedLock     m_lock2;
    SysMtgsThread &m_mtgs;

  public:
    RingBufferLock(SysMtgsThread &mtgs)
        : m_lock1(mtgs.m_mtx_RingBufferBusy), m_lock2(mtgs.m_mtx_RingBufferBusy2), m_mtgs(mtgs)
    {
        m_mtgs.m_RingBufferIsBusy.store(true, std::memory_order_relaxed);
    }
    virtual ~RingBufferLock()
    {
        m_mtgs.m_RingBufferIsBusy.store(false, std::memory_order_relaxed);
    }
    void Acquire()
    {
        m_lock1.Acquire();
        m_lock2.Acquire();
        m_mtgs.m_RingBufferIsBusy.store(true, std::memory_order_relaxed);
    }
    void Release()
    {
        m_mtgs.m_RingBufferIsBusy.store(false, std::memory_order_relaxed);
        m_lock2.Release();
        m_lock1.Release();
    }
    void PartialAcquire()
    {
        m_lock2.Acquire();
    }
    void PartialRelease()
    {
        m_lock2.Release();
    }
};

void SysMtgsThread::ExecuteTaskInThread()
{

#ifdef RINGBUF_DEBUG_STACK
    PacketTagType prevCmd;
#endif

    RingBufferLock busy(*this);

    while (true) {
        busy.Release();


        m_sem_event.WaitWithoutYield();
        StateCheckInThread();
        busy.Acquire();

        while (m_ReadPos.load(std::memory_order_relaxed) != m_WritePos.load(std::memory_order_acquire)) {
            const unsigned int local_ReadPos = m_ReadPos.load(std::memory_order_relaxed);

            pxAssert(local_ReadPos < RingBufferSize);

            const PacketTagType &tag        = (PacketTagType &)RingBuffer[local_ReadPos];
            u32                  ringposinc = 1;

#ifdef RINGBUF_DEBUG_STACK

            m_lock_Stack.Lock();
            uptr stackpos = ringposStack.back();
            if (stackpos != local_ReadPos) {
                Console.Error("MTGS Ringbuffer Critical Failure ---> %x to %x (prevCmd: %x)\n", stackpos, local_ReadPos,
                              prevCmd.command);
            }
            pxAssert(stackpos == local_ReadPos);
            prevCmd = tag;
            ringposStack.pop_back();
            m_lock_Stack.Release();
#endif

            switch (tag.command) {
#if COPY_GS_PACKET_TO_MTGS == 1
                case GS_RINGTYPE_P1: {
                    uint        datapos = (local_ReadPos + 1) & RingBufferMask;
                    const int   qsize   = tag.data[0];
                    const u128 *data    = &RingBuffer[datapos];

                    MTGS_LOG("(MTGS Packet Read) ringtype=P1, qwc=%u", qsize);

                    uint endpos = datapos + qsize;
                    if (endpos >= RingBufferSize) {
                        uint firstcopylen = RingBufferSize - datapos;
                        GSgifTransfer((u8 *)data, firstcopylen);
                        datapos = endpos & RingBufferMask;
                        GSgifTransfer((u8 *)RingBuffer.m_Ring, datapos);
                    } else {
                        GSgifTransfer((u8 *)data, qsize);
                    }

                    ringposinc += qsize;
                } break;

                case GS_RINGTYPE_P2: {
                    uint        datapos = (local_ReadPos + 1) & RingBufferMask;
                    const int   qsize   = tag.data[0];
                    const u128 *data    = &RingBuffer[datapos];

                    MTGS_LOG("(MTGS Packet Read) ringtype=P2, qwc=%u", qsize);

                    uint endpos = datapos + qsize;
                    if (endpos >= RingBufferSize) {
                        uint firstcopylen = RingBufferSize - datapos;
                        GSgifTransfer2((u32 *)data, firstcopylen);
                        datapos = endpos & RingBufferMask;
                        GSgifTransfer2((u32 *)RingBuffer.m_Ring, datapos);
                    } else {
                        GSgifTransfer2((u32 *)data, qsize);
                    }

                    ringposinc += qsize;
                } break;

                case GS_RINGTYPE_P3: {
                    uint        datapos = (local_ReadPos + 1) & RingBufferMask;
                    const int   qsize   = tag.data[0];
                    const u128 *data    = &RingBuffer[datapos];

                    MTGS_LOG("(MTGS Packet Read) ringtype=P3, qwc=%u", qsize);

                    uint endpos = datapos + qsize;
                    if (endpos >= RingBufferSize) {
                        uint firstcopylen = RingBufferSize - datapos;
                        GSgifTransfer3((u32 *)data, firstcopylen);
                        datapos = endpos & RingBufferMask;
                        GSgifTransfer3((u32 *)RingBuffer.m_Ring, datapos);
                    } else {
                        GSgifTransfer3((u32 *)data, qsize);
                    }

                    ringposinc += qsize;
                } break;
#endif
                case GS_RINGTYPE_GSPACKET: {
                    Gif_Path &path   = gifUnit.gifPath[tag.data[2]];
                    u32       offset = tag.data[0];
                    u32       size   = tag.data[1];
                    if (offset != ~0u)
                        GSgifTransfer((u8 *)&path.buffer[offset], size / 16);
                    path.readAmount.fetch_sub(size, std::memory_order_acq_rel);
                    break;
                }

                case GS_RINGTYPE_MTVU_GSPACKET: {
                    MTVU_LOG("MTGS - Waiting on semaXGkick!");
                    vu1Thread.KickStart(true);
                    if (!vu1Thread.semaXGkick.TryWait()) {
                        busy.PartialRelease();
                        vu1Thread.semaXGkick.WaitWithoutYield();
                        busy.PartialAcquire();
                    }
                    Gif_Path &path   = gifUnit.gifPath[GIF_PATH_1];
                    GS_Packet gsPack = path.GetGSPacketMTVU();
                    if (gsPack.size)
                        GSgifTransfer((u8 *)&path.buffer[gsPack.offset], gsPack.size / 16);
                    path.readAmount.fetch_sub(gsPack.size + gsPack.readAmount, std::memory_order_acq_rel);
                    path.PopGSPacketMTVU();
                    break;
                }

                default: {
                    switch (tag.command) {
                        case GS_RINGTYPE_VSYNC: {
                            const int qsize = tag.data[0];
                            ringposinc += qsize;

                            MTGS_LOG("(MTGS Packet Read) ringtype=Vsync, field=%u, skip=%s",
                                     !!(((u32 &)RingBuffer.Regs[0x1000]) & 0x2000) ? 0 : 1,
                                     tag.data[1] ? "true" : "false");


                            uint datapos = (local_ReadPos + 1) & RingBufferMask;
                            MemCopy_WrappedSrc(RingBuffer.m_Ring, datapos, RingBufferSize, (u128 *)RingBuffer.Regs,
                                               0xf);

                            u32 *remainder                            = (u32 *)&RingBuffer[datapos];
                            ((u32 &)RingBuffer.Regs[0x1000])          = remainder[0];
                            ((u32 &)RingBuffer.Regs[0x1010])          = remainder[1];
                            ((GSRegSIGBLID &)RingBuffer.Regs[0x1080]) = (GSRegSIGBLID &)remainder[2];

                            GSvsync(((u32 &)RingBuffer.Regs[0x1000]) & 0x2000);
                            gsFrameSkip();

                            m_QueuedFrameCount.fetch_sub(1);
                            if (m_VsyncSignalListener.exchange(false))
                                m_sem_Vsync.Post();

                        } break;

                        case GS_RINGTYPE_FRAMESKIP:
                            MTGS_LOG("(MTGS Packet Read) ringtype=Frameskip");
                            _gs_ResetFrameskip();
                            break;

                        case GS_RINGTYPE_FREEZE: {
                            MTGS_FreezeData *data = (MTGS_FreezeData *)tag.pointer;
                            int              mode = tag.data[0];
                            data->retval          = GSfreeze((FreezeAction)mode, (freezeData *)data->fdata);
                        } break;

                        case GS_RINGTYPE_RESET:
                            MTGS_LOG("(MTGS Packet Read) ringtype=Reset");
                            GSreset();
                            break;

                        case GS_RINGTYPE_SOFTRESET: {
                            int mask = tag.data[0];
                            MTGS_LOG("(MTGS Packet Read) ringtype=SoftReset");
                            GSgifSoftReset(mask);
                        } break;

                        case GS_RINGTYPE_CRC:
                            GSsetGameCRC(tag.data[0], 0);
                            break;

                        case GS_RINGTYPE_INIT_READ_FIFO1:
                            MTGS_LOG("(MTGS Packet Read) ringtype=Fifo1");
                            GSinitReadFIFO((u8 *)tag.pointer);
                            break;

                        case GS_RINGTYPE_INIT_READ_FIFO2:
                            MTGS_LOG("(MTGS Packet Read) ringtype=Fifo2, size=%d", tag.data[0]);
                            GSinitReadFIFO2((u8 *)tag.pointer, tag.data[0]);
                            break;

#ifdef PCSX2_DEVBUILD
                        default:
                            Console.Error("GSThreadProc, bad packet (%x) at m_ReadPos: %x, m_WritePos: %x", tag.command,
                                          local_ReadPos, m_WritePos.load());
                            pxFail("Bad packet encountered in the MTGS Ringbuffer.");
                            m_ReadPos.store(m_WritePos.load(std::memory_order_acquire), std::memory_order_release);
                            continue;
#else
                            jNO_DEFAULT;
#endif
                    }
                }
            }

            uint newringpos = (m_ReadPos.load(std::memory_order_relaxed) + ringposinc) & RingBufferMask;

            if (EmuConfig.GS.SynchronousMTGS) {
                pxAssert(m_WritePos == newringpos);
            }

            m_ReadPos.store(newringpos, std::memory_order_release);

            if (m_SignalRingEnable.load(std::memory_order_acquire)) {
                if (m_SignalRingPosition.fetch_sub(ringposinc) <= 0) {
                    m_SignalRingEnable.store(false, std::memory_order_release);
                    m_sem_OnRingReset.Post();
                    continue;
                }
            }
        }

        busy.Release();

        if (m_SignalRingEnable.exchange(false)) {
            m_SignalRingPosition.store(0, std::memory_order_release);
            m_sem_OnRingReset.Post();
        }

        if (m_VsyncSignalListener.exchange(false))
            m_sem_Vsync.Post();
    }
}

void SysMtgsThread::CloseGS()
{
    if (!m_Opened)
        return;
    m_Opened = false;
    GSclose();
    // if (init_gspanel)
    //     sApp.CloseGsPanel();
}

void SysMtgsThread::OnSuspendInThread()
{
    CloseGS();
    _parent::OnSuspendInThread();
}

void SysMtgsThread::OnResumeInThread(SystemsMask systemsToReinstate)
{
    if (systemsToReinstate & System_GS)
        OpenGS();

    _parent::OnResumeInThread(systemsToReinstate);
}

void SysMtgsThread::OnCleanupInThread()
{
    CloseGS();
    m_ReadPos.store(m_WritePos.load(std::memory_order_acquire), std::memory_order_relaxed);
    _parent::OnCleanupInThread();
}

void SysMtgsThread::WaitGS(bool syncRegs, bool weakWait, bool isMTVU)
{
    pxAssertDev(!IsSelf(), "This method is only allowed from threads *not* named MTGS.");

    if (m_ExecMode == ExecMode_NoThreadYet || !IsRunning())
        return;
    if (!pxAssertDev(IsOpen(), "MTGS Warning!  WaitGS issued on a closed thread."))
        return;

    Gif_Path &path         = gifUnit.gifPath[GIF_PATH_1];
    u32       startP1Packs = weakWait ? path.GetPendingGSPackets() : 0;


    if (isMTVU || m_ReadPos.load(std::memory_order_relaxed) != m_WritePos.load(std::memory_order_relaxed)) {
        SetEvent();
        RethrowException();
        for (;;) {
            if (weakWait)
                m_mtx_RingBufferBusy2.Wait();
            else
                m_mtx_RingBufferBusy.Wait();
            RethrowException();
            if (!isMTVU && m_ReadPos.load(std::memory_order_relaxed) == m_WritePos.load(std::memory_order_relaxed))
                break;
            u32 curP1Packs = weakWait ? path.GetPendingGSPackets() : 0;
            if (weakWait && ((startP1Packs - curP1Packs) || !curP1Packs))
                break;
        }
    }

    if (syncRegs) {
        ScopedLock lock(m_mtx_WaitGS);
        memcpy(RingBuffer.Regs, PS2MEM_GS, sizeof(RingBuffer.Regs));
    }
}

void SysMtgsThread::SetEvent()
{
    if (!m_RingBufferIsBusy.load(std::memory_order_relaxed))
        m_sem_event.Post();

    m_CopyDataTally = 0;
}

u8 *SysMtgsThread::GetDataPacketPtr() const
{
    return (u8 *)&RingBuffer[m_packet_writepos & RingBufferMask];
}

void SysMtgsThread::SendDataPacket()
{
    pxAssert(m_packet_size != 0);

    uint actualSize = ((m_packet_writepos - m_packet_startpos) & RingBufferMask) - 1;
    pxAssert(actualSize <= m_packet_size);
    pxAssert(m_packet_writepos < RingBufferSize);

    PacketTagType &tag = (PacketTagType &)RingBuffer[m_packet_startpos];
    tag.data[0]        = actualSize;

    m_WritePos.store(m_packet_writepos, std::memory_order_release);

    if (EmuConfig.GS.SynchronousMTGS) {
        WaitGS();
    } else if (!m_RingBufferIsBusy.load(std::memory_order_relaxed)) {
        m_CopyDataTally += m_packet_size;
        if (m_CopyDataTally > 0x2000)
            SetEvent();
    }

    m_packet_size = 0;
}

void SysMtgsThread::GenericStall(uint size)
{
    const uint writepos = m_WritePos.load(std::memory_order_relaxed);

    pxAssert(size < RingBufferSize);
    pxAssert(writepos < RingBufferSize);


    uint readpos = m_ReadPos.load(std::memory_order_acquire);
    uint freeroom;

    if (writepos < readpos)
        freeroom = readpos - writepos;
    else
        freeroom = RingBufferSize - (writepos - readpos);

    if (freeroom <= size) {


        uint somedone = (RingBufferSize - freeroom) / 4;
        if (somedone < size + 1)
            somedone = size + 1;


        if (somedone > 0x80) {
            pxAssertDev(m_SignalRingEnable == 0, "MTGS Thread Synchronization Error");
            m_SignalRingPosition.store(somedone, std::memory_order_release);


            while (true) {
                m_SignalRingEnable.store(true, std::memory_order_release);
                SetEvent();
                m_sem_OnRingReset.WaitWithoutYield();
                readpos = m_ReadPos.load(std::memory_order_acquire);

                if (writepos < readpos)
                    freeroom = readpos - writepos;
                else
                    freeroom = RingBufferSize - (writepos - readpos);

                if (freeroom > size)
                    break;
            }

            pxAssertDev(m_SignalRingPosition <= 0, "MTGS Thread Synchronization Error");
        } else {
            SetEvent();
            while (true) {
                SpinWait();
                readpos = m_ReadPos.load(std::memory_order_acquire);

                if (writepos < readpos)
                    freeroom = readpos - writepos;
                else
                    freeroom = RingBufferSize - (writepos - readpos);

                if (freeroom > size)
                    break;
            }
        }
    }
}

void SysMtgsThread::PrepDataPacket(MTGS_RingCommand cmd, u32 size)
{
    m_packet_size = size;
    ++size;
    GenericStall(size);

    const unsigned int local_WritePos = m_WritePos.load(std::memory_order_relaxed);

    PacketTagType &tag = (PacketTagType &)RingBuffer[local_WritePos];
    tag.command        = cmd;
    tag.data[0]        = m_packet_size;
    m_packet_startpos  = local_WritePos;
    m_packet_writepos  = (local_WritePos + 1) & RingBufferMask;
}

void SysMtgsThread::PrepDataPacket(GIF_PATH pathidx, u32 size)
{

    PrepDataPacket((MTGS_RingCommand)pathidx, size);
}

__fi void SysMtgsThread::_FinishSimplePacket()
{
    uint future_writepos = (m_WritePos.load(std::memory_order_relaxed) + 1) & RingBufferMask;
    pxAssert(future_writepos != m_ReadPos.load(std::memory_order_acquire));
    m_WritePos.store(future_writepos, std::memory_order_release);

    if (EmuConfig.GS.SynchronousMTGS)
        WaitGS();
    else
        ++m_CopyDataTally;
}

void SysMtgsThread::SendSimplePacket(MTGS_RingCommand type, int data0, int data1, int data2)
{

    GenericStall(1);
    PacketTagType &tag = (PacketTagType &)RingBuffer[m_WritePos.load(std::memory_order_relaxed)];

    tag.command = type;
    tag.data[0] = data0;
    tag.data[1] = data1;
    tag.data[2] = data2;

    _FinishSimplePacket();
}

void SysMtgsThread::SendSimpleGSPacket(MTGS_RingCommand type, u32 offset, u32 size, GIF_PATH path)
{
    SendSimplePacket(type, (int)offset, (int)size, (int)path);

    if (!EmuConfig.GS.SynchronousMTGS) {
        if (!m_RingBufferIsBusy.load(std::memory_order_relaxed)) {
            m_CopyDataTally += size / 16;
            if (m_CopyDataTally > 0x2000)
                SetEvent();
        }
    }
}

void SysMtgsThread::SendPointerPacket(MTGS_RingCommand type, u32 data0, void *data1)
{

    GenericStall(1);
    PacketTagType &tag = (PacketTagType &)RingBuffer[m_WritePos.load(std::memory_order_relaxed)];

    tag.command = type;
    tag.data[0] = data0;
    tag.pointer = (uptr)data1;

    _FinishSimplePacket();
}

void SysMtgsThread::SendGameCRC(u32 crc)
{
    SendSimplePacket(GS_RINGTYPE_CRC, crc, 0, 0);
}

void SysMtgsThread::WaitForOpen()
{
    if (m_Opened)
        return;
    Resume();


    if (!m_sem_OpenDone.Wait(wxTimeSpan(0, 0, 2, 0))) {
        RethrowException();

        if (!m_sem_OpenDone.Wait(wxTimeSpan(0, 0, 12, 0))) {
            RethrowException();

            // pxAssert(_("The MTGS thread has become unresponsive while waiting for GS to open."));
        }
    }

    RethrowException();
}

void SysMtgsThread::Freeze(FreezeAction mode, MTGS_FreezeData &data)
{
    pxAssertDev(!IsSelf(), "This method is only allowed from threads *not* named MTGS.");
    SendPointerPacket(GS_RINGTYPE_FREEZE, (int)mode, &data);
    Resume();
    WaitForOpen();
    WaitGS();
}
