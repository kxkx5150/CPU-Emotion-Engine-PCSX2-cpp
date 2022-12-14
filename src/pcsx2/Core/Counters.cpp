

#include "Core/PrecompiledHeader.h"

#include <time.h>
#include <cmath>

#include "gui/main.h"
#include "Common.h"
#include "R3000A.h"
#include "Counters.h"
#include "IopCounters.h"

#include "GS.h"
#include "VUmicro.h"

#include "ps2/HwInternal.h"
#include "Sio.h"


using namespace Threading;

extern u8         psxhblankgate;
static const uint EECNT_FUTURE_TARGET = 0x10000000;
static int        gates               = 0;

uint g_FrameCount = 0;

Counter     counters[4];
SyncCounter hsyncCounter;
SyncCounter vsyncCounter;

u32 nextsCounter;
s32 nextCounter;


static void rcntStartGate(bool mode, u32 sCycle);
static void rcntEndGate(bool mode, u32 sCycle);
static void rcntWcount(int index, u32 value);
static void rcntWmode(int index, u32 value);
static void rcntWtarget(int index, u32 value);
static void rcntWhold(int index, u32 value);

static bool IsInterlacedVideoMode()
{
    return (gsVideoMode == GS_VideoMode::PAL || gsVideoMode == GS_VideoMode::NTSC ||
            gsVideoMode == GS_VideoMode::DVD_NTSC || gsVideoMode == GS_VideoMode::DVD_PAL ||
            gsVideoMode == GS_VideoMode::HDTV_1080I);
}

void rcntReset(int index)
{
    counters[index].count   = 0;
    counters[index].sCycleT = cpuRegs.cycle;
}

static __fi void _rcntSet(int cntidx)
{
    s32 c;
    pxAssume(cntidx <= 4);

    const Counter &counter = counters[cntidx];

    if (!counter.mode.IsCounting || (counter.mode.ClockSource == 0x3))
        return;

    if (counter.count > 0x10000 || counter.count > counter.target) {
        nextCounter = 4;
        return;
    }


    c = ((0x10000 - counter.count) * counter.rate) - (cpuRegs.cycle - counter.sCycleT);
    c += cpuRegs.cycle - nextsCounter;
    if (c < nextCounter) {
        nextCounter = c;
        cpuSetNextEvent(nextsCounter, nextCounter);
    }


    if (counter.target & EECNT_FUTURE_TARGET) {
        return;
    } else {
        c = ((counter.target - counter.count) * counter.rate) - (cpuRegs.cycle - counter.sCycleT);
        c += cpuRegs.cycle - nextsCounter;
        if (c < nextCounter) {
            nextCounter = c;
            cpuSetNextEvent(nextsCounter, nextCounter);
        }
    }
}


static __fi void cpuRcntSet()
{
    int i;

    nextsCounter = cpuRegs.cycle;
    nextCounter  = vsyncCounter.CycleT - (cpuRegs.cycle - vsyncCounter.sCycle);

    s32 nextHsync = hsyncCounter.CycleT - (cpuRegs.cycle - hsyncCounter.sCycle);
    if (nextHsync < nextCounter)
        nextCounter = nextHsync;

    for (i = 0; i < 4; i++)
        _rcntSet(i);

    if (nextCounter < 0)
        nextCounter = 0;
    cpuSetNextEvent(nextsCounter, nextCounter);
}

void rcntInit()
{
    int i;

    g_FrameCount = 0;

    memzero(counters);

    for (i = 0; i < 4; i++) {
        counters[i].rate   = 2;
        counters[i].target = 0xffff;
    }
    counters[0].interrupt = 9;
    counters[1].interrupt = 10;
    counters[2].interrupt = 11;
    counters[3].interrupt = 12;

    hsyncCounter.Mode   = MODE_HRENDER;
    hsyncCounter.sCycle = cpuRegs.cycle;
    vsyncCounter.Mode   = MODE_VRENDER;
    vsyncCounter.sCycle = cpuRegs.cycle;

    for (i = 0; i < 4; i++)
        rcntReset(i);
    cpuRcntSet();
}


#ifndef _WIN32
#include <sys/time.h>
#endif

static s64 m_iTicks = 0;
static u64 m_iStart = 0;

struct vSyncTimingInfo
{
    double       Framerate;
    GS_VideoMode VideoMode;
    u32          Render;
    u32          Blank;

    u32 GSBlank;

    u32 hSyncError;
    u32 hRender;
    u32 hBlank;
    u32 hScanlinesPerFrame;
};


static vSyncTimingInfo vSyncInfo;


static void vSyncInfoCalc(vSyncTimingInfo *info, double framesPerSecond, u32 scansPerFrame)
{
    constexpr double clock = static_cast<double>(PS2CLK);

    const u64 Frame    = clock * 10000ULL / framesPerSecond;
    const u64 Scanline = Frame / scansPerFrame;

    const u64 HalfFrame = Frame / 2;
    const u64 Blank     = Scanline * (gsVideoMode == GS_VideoMode::NTSC ? 22 : 26);
    const u64 Render    = HalfFrame - Blank;
    const u64 GSBlank   = Scanline * 3.5;


    u64 hBlank  = Scanline / 2;
    u64 hRender = Scanline - hBlank;

    if (!IsInterlacedVideoMode()) {
        hBlank /= 2;
        hRender /= 2;
    }

    info->Framerate = framesPerSecond;
    info->GSBlank   = (u32)(GSBlank / 10000);
    info->Render    = (u32)(Render / 10000);
    info->Blank     = (u32)(Blank / 10000);

    info->hRender            = (u32)(hRender / 10000);
    info->hBlank             = (u32)(hBlank / 10000);
    info->hScanlinesPerFrame = scansPerFrame;

    if ((Render % 10000) >= 5000)
        info->Render++;
    if ((Blank % 10000) >= 5000)
        info->Blank++;

    if ((hRender % 10000) >= 5000)
        info->hRender++;
    if ((hBlank % 10000) >= 5000)
        info->hBlank++;

    if (IsInterlacedVideoMode()) {
        u32 hSyncCycles  = ((info->hRender + info->hBlank) * scansPerFrame) / 2;
        u32 vSyncCycles  = (info->Render + info->Blank);
        info->hSyncError = vSyncCycles - hSyncCycles;
    } else
        info->hSyncError = 0;
}

const char *ReportVideoMode()
{
    switch (gsVideoMode) {
        case GS_VideoMode::PAL:
            return "PAL";
        case GS_VideoMode::NTSC:
            return "NTSC";
        case GS_VideoMode::DVD_NTSC:
            return "DVD NTSC";
        case GS_VideoMode::DVD_PAL:
            return "DVD PAL";
        case GS_VideoMode::VESA:
            return "VESA";
        case GS_VideoMode::SDTV_480P:
            return "SDTV 480p";
        case GS_VideoMode::SDTV_576P:
            return "SDTV 576p";
        case GS_VideoMode::HDTV_720P:
            return "HDTV 720p";
        case GS_VideoMode::HDTV_1080I:
            return "HDTV 1080i";
        case GS_VideoMode::HDTV_1080P:
            return "HDTV 1080p";
        default:
            return "Unknown";
    }
}

double GetVerticalFrequency()
{
    switch (gsVideoMode) {
        case GS_VideoMode::Uninitialized:
            return 60.00;
        case GS_VideoMode::PAL:
        case GS_VideoMode::DVD_PAL:
            return gsIsInterlaced ? EmuConfig.GS.FrameratePAL : EmuConfig.GS.FrameratePAL - 0.24f;
        case GS_VideoMode::NTSC:
        case GS_VideoMode::DVD_NTSC:
            return gsIsInterlaced ? EmuConfig.GS.FramerateNTSC : EmuConfig.GS.FramerateNTSC - 0.11f;
        case GS_VideoMode::SDTV_480P:
            return 59.94;
        case GS_VideoMode::HDTV_1080P:
        case GS_VideoMode::HDTV_1080I:
        case GS_VideoMode::HDTV_720P:
        case GS_VideoMode::SDTV_576P:
        case GS_VideoMode::VESA:
            return 60.00;
        default:
            return FRAMERATE_NTSC * 2;
    }
}

u32 UpdateVSyncRate()
{

    const double vertical_frequency = GetVerticalFrequency();

    const double frames_per_second = vertical_frequency / 2.0;
    const double frame_limit       = frames_per_second * EmuConfig.GS.LimitScalar;

    const double tick_rate = GetTickFrequency() / 2.0;
    const s64    ticks     = static_cast<s64>(tick_rate / std::max(frame_limit, 1.0));

    u32  total_scanlines = 0;
    bool custom          = false;

    switch (gsVideoMode) {
        case GS_VideoMode::Uninitialized:
            total_scanlines = SCANLINES_TOTAL_NTSC;
            break;
        case GS_VideoMode::PAL:
        case GS_VideoMode::DVD_PAL:
            custom          = (EmuConfig.GS.FrameratePAL != 50.0);
            total_scanlines = SCANLINES_TOTAL_PAL;
            break;
        case GS_VideoMode::NTSC:
        case GS_VideoMode::DVD_NTSC:
            custom          = (EmuConfig.GS.FramerateNTSC != 59.94);
            total_scanlines = SCANLINES_TOTAL_NTSC;
            break;
        case GS_VideoMode::SDTV_480P:
        case GS_VideoMode::SDTV_576P:
        case GS_VideoMode::HDTV_720P:
        case GS_VideoMode::VESA:
            total_scanlines = SCANLINES_TOTAL_NTSC;
            break;
        case GS_VideoMode::HDTV_1080P:
        case GS_VideoMode::HDTV_1080I:
            total_scanlines = SCANLINES_TOTAL_1080;
            break;
        case GS_VideoMode::Unknown:
        default:
            total_scanlines = SCANLINES_TOTAL_NTSC;
            Console.Error("PCSX2-Counters: Unknown video mode detected");
            pxAssertDev(false, "Unknown video mode detected via SetGsCrt");
    }

    const bool video_mode_initialized = gsVideoMode != GS_VideoMode::Uninitialized;

    if (vSyncInfo.Framerate != frames_per_second || vSyncInfo.VideoMode != gsVideoMode) {
        vSyncInfo.VideoMode = gsVideoMode;

        vSyncInfoCalc(&vSyncInfo, frames_per_second, total_scanlines);

        if (video_mode_initialized)
            Console.WriteLn(Color_Green, "(UpdateVSyncRate) Mode Changed to %s.", ReportVideoMode());

        if (custom && video_mode_initialized)
            Console.Indent().WriteLn(Color_StrongGreen, "... with user configured refresh rate: %.02f Hz",
                                     vertical_frequency);

        hsyncCounter.CycleT = vSyncInfo.hRender;
        vsyncCounter.CycleT = vSyncInfo.Render;

        cpuRcntSet();
    }

    if (m_iTicks != ticks)
        m_iTicks = ticks;

    m_iStart = GetCPUTicks();

    return static_cast<u32>(m_iTicks);
}

void frameLimitReset()
{
    m_iStart = GetCPUTicks();
}

static __fi void frameLimitUpdateCore()
{
    GetCoreThread().VsyncInThread();
    Cpu->CheckExecutionState();
}

static __fi void frameLimit()
{
    if (!EmuConfig.GS.FrameLimitEnable) {
        frameLimitUpdateCore();
        return;
    }

    const u64 uExpectedEnd = m_iStart + m_iTicks;
    const u64 iEnd         = GetCPUTicks();
    const s64 sDeltaTime   = iEnd - uExpectedEnd;

    if (sDeltaTime >= m_iTicks) {
        m_iStart += (sDeltaTime / m_iTicks) * m_iTicks;
        frameLimitUpdateCore();
        return;
    }

    s32 msec = (int)((sDeltaTime * -1000) / (s64)GetTickFrequency());

    if (msec > 1) {
        Threading::Sleep(msec - 1);
    }

    while (GetCPUTicks() < uExpectedEnd) {
    }

    m_iStart = uExpectedEnd;
    frameLimitUpdateCore();
}

static __fi void VSyncStart(u32 sCycle)
{

    frameLimit();
    gsPostVsyncStart();


    vSyncDebugStuff(g_FrameCount);

    CpuVU0->Vsync();
    CpuVU1->Vsync();

    hwIntcIrq(INTC_VBLANK_S);
    psxVBlankStart();

    if (gates)
        rcntStartGate(true, sCycle);
}

static __fi void GSVSync()
{
    CSRreg.SwapField();

    if (!CSRreg.VSINT) {
        CSRreg.VSINT = true;
        if (!GSIMR.VSMSK)
            gsIrq();
    }
}

static __fi void VSyncEnd(u32 sCycle)
{


    g_FrameCount++;

    hwIntcIrq(INTC_VBLANK_E);
    psxVBlankEnd();
    if (gates)
        rcntEndGate(true, sCycle);

    if (!(g_FrameCount % 60))
        sioNextFrame();
}

#ifdef VSYNC_DEBUG
static u32 hsc       = 0;
static int vblankinc = 0;
#endif

__fi void rcntUpdate_hScanline()
{
    if (!cpuTestCycle(hsyncCounter.sCycle, hsyncCounter.CycleT))
        return;

    if (hsyncCounter.Mode & MODE_HBLANK) {
        rcntStartGate(false, hsyncCounter.sCycle);
        psxCheckStartGate16(0);

        hsyncCounter.sCycle += vSyncInfo.hBlank;
        hsyncCounter.CycleT = vSyncInfo.hRender;
        hsyncCounter.Mode   = MODE_HRENDER;
    } else {
        if (!CSRreg.HSINT) {
            CSRreg.HSINT = true;
            if (!GSIMR.HSMSK)
                gsIrq();
        }
        if (gates)
            rcntEndGate(false, hsyncCounter.sCycle);
        if (psxhblankgate)
            psxCheckEndGate16(0);

        hsyncCounter.sCycle += vSyncInfo.hRender;
        hsyncCounter.CycleT = vSyncInfo.hBlank;
        hsyncCounter.Mode   = MODE_HBLANK;

#ifdef VSYNC_DEBUG
        hsc++;
#endif
    }
}

__fi void rcntUpdate_vSync()
{
    s32 diff = (cpuRegs.cycle - vsyncCounter.sCycle);
    if (diff < vsyncCounter.CycleT)
        return;

    if (vsyncCounter.Mode == MODE_VSYNC) {
        VSyncEnd(vsyncCounter.sCycle);

        vsyncCounter.sCycle += vSyncInfo.Blank;
        vsyncCounter.CycleT = vSyncInfo.Render;
        vsyncCounter.Mode   = MODE_VRENDER;
    } else if (vsyncCounter.Mode == MODE_GSBLANK) {
        GSVSync();

        vsyncCounter.Mode   = MODE_VSYNC;
        vsyncCounter.CycleT = vSyncInfo.Blank;
    } else {
        VSyncStart(vsyncCounter.sCycle);

        vsyncCounter.sCycle += vSyncInfo.Render;
        vsyncCounter.CycleT = vSyncInfo.GSBlank;
        vsyncCounter.Mode   = MODE_GSBLANK;

        hsyncCounter.sCycle += vSyncInfo.hSyncError;

#ifdef VSYNC_DEBUG
        vblankinc++;
        if (vblankinc > 1) {
            if (hsc != vSyncInfo.hScanlinesPerFrame)
                Console.WriteLn(" ** vSync > Abnormal Scanline Count: %d", hsc);
            hsc       = 0;
            vblankinc = 0;
        }
#endif
    }
}

static __fi void _cpuTestTarget(int i)
{
    if (counters[i].count < counters[i].target)
        return;

    if (counters[i].mode.TargetInterrupt) {
        if (!counters[i].mode.TargetReached) {
            counters[i].mode.TargetReached = 1;
            hwIntcIrq(counters[i].interrupt);
        }
    }

    if (counters[i].mode.ZeroReturn)
        counters[i].count -= counters[i].target;
    else
        counters[i].target |= EECNT_FUTURE_TARGET;
}

static __fi void _cpuTestOverflow(int i)
{
    if (counters[i].count <= 0xffff)
        return;

    if (counters[i].mode.OverflowInterrupt) {
        if (!counters[i].mode.OverflowReached) {
            counters[i].mode.OverflowReached = 1;
            hwIntcIrq(counters[i].interrupt);
        }
    }

    counters[i].count -= 0x10000;
    counters[i].target &= 0xffff;
}


__fi void rcntUpdate()
{
    rcntUpdate_vSync();


    for (int i = 0; i <= 3; i++) {


        if (!counters[i].mode.IsCounting)
            continue;

        if (counters[i].mode.ClockSource != 0x3) {
            s32 change = cpuRegs.cycle - counters[i].sCycleT;
            if (change < 0)
                change = 0;

            counters[i].count += change / counters[i].rate;
            change -= (change / counters[i].rate) * counters[i].rate;
            counters[i].sCycleT = cpuRegs.cycle - change;

            _cpuTestTarget(i);
            _cpuTestOverflow(i);
        } else
            counters[i].sCycleT = cpuRegs.cycle;
    }

    cpuRcntSet();
}

static __fi void _rcntSetGate(int index)
{
    if (counters[index].mode.EnableGate) {

        if (!(counters[index].mode.GateSource == 0 && counters[index].mode.ClockSource == 3)) {

            gates |= (1 << index);
            counters[index].mode.IsCounting = 0;
            rcntReset(index);
            return;
        }
    }

    gates &= ~(1 << index);
}

static __fi void rcntStartGate(bool isVblank, u32 sCycle)
{
    int i;

    for (i = 0; i <= 3; i++) {
        if (!isVblank && counters[i].mode.IsCounting && (counters[i].mode.ClockSource == 3)) {


            counters[i].count += HBLANK_COUNTER_SPEED;
            _cpuTestTarget(i);
            _cpuTestOverflow(i);
        }

        if (!(gates & (1 << i)))
            continue;
        if ((!!counters[i].mode.GateSource) != isVblank)
            continue;

        switch (counters[i].mode.GateMode) {
            case 0x0:


                counters[i].count           = rcntRcount(i);
                counters[i].mode.IsCounting = 0;
                counters[i].sCycleT         = sCycle;
                break;

            case 0x2:
                break;

            case 0x1:
            case 0x3:
                counters[i].mode.IsCounting = 1;
                counters[i].count           = 0;
                counters[i].target &= 0xffff;
                counters[i].sCycleT = sCycle;
                break;
        }
    }
}

static __fi void rcntEndGate(bool isVblank, u32 sCycle)
{
    int i;

    for (i = 0; i <= 3; i++) {
        if (!(gates & (1 << i)))
            continue;
        if ((!!counters[i].mode.GateSource) != isVblank)
            continue;

        switch (counters[i].mode.GateMode) {
            case 0x0:

                counters[i].mode.IsCounting = 1;
                counters[i].sCycleT         = cpuRegs.cycle;

                break;

            case 0x1:
                break;

            case 0x2:
            case 0x3:
                counters[i].mode.IsCounting = 1;
                counters[i].count           = 0;
                counters[i].target &= 0xffff;
                counters[i].sCycleT = sCycle;
                break;
        }
    }
}

static __fi u32 rcntCycle(int index)
{
    if (counters[index].mode.IsCounting && (counters[index].mode.ClockSource != 0x3))
        return counters[index].count + ((cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate);
    else
        return counters[index].count;
}

static __fi void rcntWmode(int index, u32 value)
{
    if (counters[index].mode.IsCounting) {
        if (counters[index].mode.ClockSource != 0x3) {

            u32 change = cpuRegs.cycle - counters[index].sCycleT;
            if (change > 0) {
                counters[index].count += change / counters[index].rate;
                change -= (change / counters[index].rate) * counters[index].rate;
                counters[index].sCycleT = cpuRegs.cycle - change;
            }
        }
    } else
        counters[index].sCycleT = cpuRegs.cycle;


    counters[index].modeval &= ~(value & 0xc00);
    counters[index].modeval = (counters[index].modeval & 0xc00) | (value & 0x3ff);

    switch (counters[index].mode.ClockSource) {
        case 0:
            counters[index].rate = 2;
            break;
        case 1:
            counters[index].rate = 32;
            break;
        case 2:
            counters[index].rate = 512;
            break;
        case 3:
            counters[index].rate = vSyncInfo.hBlank + vSyncInfo.hRender;
            break;
    }

    _rcntSetGate(index);
    _rcntSet(index);
}

static __fi void rcntWcount(int index, u32 value)
{

    counters[index].count = value & 0xffff;

    counters[index].target &= 0xffff;
    if (counters[index].count > counters[index].target)
        counters[index].target |= EECNT_FUTURE_TARGET;

    if (counters[index].mode.IsCounting) {
        if (counters[index].mode.ClockSource != 0x3) {
            s32 change = cpuRegs.cycle - counters[index].sCycleT;
            if (change > 0) {
                change -= (change / counters[index].rate) * counters[index].rate;
                counters[index].sCycleT = cpuRegs.cycle - change;
            }
        }
    } else
        counters[index].sCycleT = cpuRegs.cycle;

    _rcntSet(index);
}

static __fi void rcntWtarget(int index, u32 value)
{

    counters[index].target = value & 0xffff;


    if (counters[index].mode.IsCounting) {
        if (counters[index].mode.ClockSource != 0x3) {

            u32 change = cpuRegs.cycle - counters[index].sCycleT;
            if (change > 0) {
                counters[index].count += change / counters[index].rate;
                change -= (change / counters[index].rate) * counters[index].rate;
                counters[index].sCycleT = cpuRegs.cycle - change;
            }
        }
    }

    if (counters[index].target <= rcntCycle(index))
        counters[index].target |= EECNT_FUTURE_TARGET;

    _rcntSet(index);
}

static __fi void rcntWhold(int index, u32 value)
{
    counters[index].hold = value;
}

__fi u32 rcntRcount(int index)
{
    u32 ret;

    if (counters[index].mode.IsCounting && (counters[index].mode.ClockSource != 0x3))
        ret = counters[index].count + ((cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate);
    else
        ret = counters[index].count;

    return ret;
}

template <uint page> __fi u16 rcntRead32(u32 mem)
{

    iswitch(mem)
    {
        icase(RCNT0_COUNT) return (u16)rcntRcount(0);
        icase(RCNT0_MODE) return (u16)counters[0].modeval;
        icase(RCNT0_TARGET) return (u16)counters[0].target;
        icase(RCNT0_HOLD) return (u16)counters[0].hold;

        icase(RCNT1_COUNT) return (u16)rcntRcount(1);
        icase(RCNT1_MODE) return (u16)counters[1].modeval;
        icase(RCNT1_TARGET) return (u16)counters[1].target;
        icase(RCNT1_HOLD) return (u16)counters[1].hold;

        icase(RCNT2_COUNT) return (u16)rcntRcount(2);
        icase(RCNT2_MODE) return (u16)counters[2].modeval;
        icase(RCNT2_TARGET) return (u16)counters[2].target;

        icase(RCNT3_COUNT) return (u16)rcntRcount(3);
        icase(RCNT3_MODE) return (u16)counters[3].modeval;
        icase(RCNT3_TARGET) return (u16)counters[3].target;
    }

    return psHu16(mem);
}

template <uint page> __fi bool rcntWrite32(u32 mem, mem32_t &value)
{
    pxAssume(mem >= RCNT0_COUNT && mem < 0x10002000);


    iswitch(mem)
    {
        icase(RCNT0_COUNT) return rcntWcount(0, value), false;
        icase(RCNT0_MODE) return rcntWmode(0, value), false;
        icase(RCNT0_TARGET) return rcntWtarget(0, value), false;
        icase(RCNT0_HOLD) return rcntWhold(0, value), false;

        icase(RCNT1_COUNT) return rcntWcount(1, value), false;
        icase(RCNT1_MODE) return rcntWmode(1, value), false;
        icase(RCNT1_TARGET) return rcntWtarget(1, value), false;
        icase(RCNT1_HOLD) return rcntWhold(1, value), false;

        icase(RCNT2_COUNT) return rcntWcount(2, value), false;
        icase(RCNT2_MODE) return rcntWmode(2, value), false;
        icase(RCNT2_TARGET) return rcntWtarget(2, value), false;

        icase(RCNT3_COUNT) return rcntWcount(3, value), false;
        icase(RCNT3_MODE) return rcntWmode(3, value), false;
        icase(RCNT3_TARGET) return rcntWtarget(3, value), false;
    }

    return true;
}

template u16 rcntRead32<0x00>(u32 mem);
template u16 rcntRead32<0x01>(u32 mem);

template bool rcntWrite32<0x00>(u32 mem, mem32_t &value);
template bool rcntWrite32<0x01>(u32 mem, mem32_t &value);

void SaveStateBase::rcntFreeze()
{
    Freeze(counters);
    Freeze(hsyncCounter);
    Freeze(vsyncCounter);
    Freeze(nextCounter);
    Freeze(nextsCounter);
    Freeze(vSyncInfo);
    Freeze(gsVideoMode);
    Freeze(gsIsInterlaced);
    Freeze(gates);

    if (IsLoading())
        cpuRcntSet();
}
