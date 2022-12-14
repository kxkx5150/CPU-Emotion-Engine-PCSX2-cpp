#include "MTVU.h"
#include "common/AppTrait.h"

#include <cstdio>
#include <string>
#include <algorithm>
#include <SDL2/SDL.h>
#include <wx/stdpaths.h>
#include <memory>

#include "common/StringUtil.h"
#include "main.h"
#include "Host.h"
#include "PAD/Linux/PAD.h"


Pcsx2App        *ps2app;
SDL_Window      *sdlwindow = NULL;
WindowInfo       g_gs_window_info;
extern wxDirName g_fullBaseDirName;
wxString         appIniPath;


Pcsx2App &wxGetApp()
{
    return *static_cast<Pcsx2App *>(ps2app);
}
SysMainMemory &GetVmMemory()
{
    return *ps2app->m_VmReserve;
}
SysCpuProviderPack &GetCpuProviders()
{
    return *ps2app->m_CpuProviders;
}



void Pcsx2App::LogicalVsync()
{
    if (!CoreThread.HasActiveMachine())
        return;
    PADupdate(0);

    SDL_Event events;
    while (SDL_PollEvent(&events)) {
        switch (events.type) {
            case SDL_QUIT:
                ps2app->CloseGsPanel();
                break;

            case SDL_KEYUP: {
                HostKeyEvent evt;
                evt.key  = events.key.keysym.sym;
                evt.type = static_cast<HostKeyEvent::Type>(3);
                PADWriteEvent(evt);
                break;
            }
            case SDL_KEYDOWN: {
                HostKeyEvent evt;
                evt.key  = events.key.keysym.sym;
                evt.type = static_cast<HostKeyEvent::Type>(2);
                PADWriteEvent(evt);
                break;
            }
            default:
                break;
        }
    }
}
Pcsx2App::Pcsx2App() : SysExecutorThread(new pxEvtQueue())
{
}
Pcsx2App::~Pcsx2App()
{
    try {
        vu1Thread.Cancel();
    }
    DESTRUCTOR_CATCHALL
}
void Pcsx2App::OpenGsPanel()
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_LoadLibrary(NULL);
    sdlwindow = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL);

    g_gs_window_info.display_connection = nullptr;
    g_gs_window_info.window_handle      = sdlwindow;
    g_gs_window_info.surface_width      = 640;
    g_gs_window_info.surface_height     = 480;
    g_gs_window_info.type               = WindowInfo::Type::X11;
    g_gs_window_info.surface_scale      = 1;
}
void Pcsx2App::CloseGsPanel()
{
    CoreThread.Suspend();
    CoreThread.Cancel();
    SysExecutorThread.ShutdownQueue();
    ps2app->runnning = false;

    try {
        CoreThread.Cancel();
        SysExecutorThread.ShutdownQueue();

        while (wxGetLocale() != NULL)
            delete wxGetLocale();
    } catch (Exception::CancelEvent &) {
        throw;
    } catch (Exception::RuntimeError &ex) {
        Console.Error(L"Runtime exception handled:\n");
        Console.Indent().Error(ex.FormatDiagnosticMessage());
    }
    Console_SetActiveHandler(ConsoleWriter_Stdout);
    exit(0);
}
bool Pcsx2App::OnInit()
{
    wxFileName f(wxStandardPaths::Get().GetExecutablePath());
    wxString   appPath(f.GetPath() + "/ini");
    appIniPath        = appPath;
    g_fullBaseDirName = appPath;

    SysExecutorThread.Start();
    x86caps.Identify();
    x86caps.CountCores();
    x86caps.SIMD_EstablishMXCSRmask();

    std::unique_ptr<AppConfig> g_Conf;
    g_Conf = std::make_unique<AppConfig>();

    m_VmReserve = std::unique_ptr<SysMainMemory>(new SysMainMemory());
    m_VmReserve->ReserveAll();
    m_CpuProviders = std::make_unique<SysCpuProviderPack>();
    m_CpuProviders->HadSomeFailures(g_Conf->EmuOptions.Cpu.Recompiler);

    EmuConfig.CurrentIRX.clear();
    g_Conf->EmuOptions.BaseFilenames.Bios = m_biosfile;
    g_Conf->CurrentIso                    = m_gamefile;

    CoreThread.ApplySettings(g_Conf->EmuOptions);
    const std::string currentIso(StringUtil::wxStringToUTF8String(g_Conf->CurrentIso));
    CDVDsys_SetFile(CDVD_SourceType::Iso, currentIso);
    SysExecutorThread.PostEvent(new SysExecEvent_Execute());
    return true;
}
int main(int argc, char **argv)
{
    ps2app             = new Pcsx2App();
    ps2app->m_biosfile = wxString(argv[1]);
    ps2app->m_gamefile = wxString(argv[2]);
    ps2app->OnInit();
    while (ps2app->runnning) {
        SDL_Delay(1000);
    }

    return 0;
}
