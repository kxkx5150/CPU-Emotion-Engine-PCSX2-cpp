#pragma once
#include "common/IniInterface.h"
#include <memory>
#include "common/General.h"
#include "common/Path.h"

#include "Config.h"
#include "CDVD/CDVDaccess.h"
#include "AppCoreThread.h"
#include <optional>
#include <list>
#include "GS.h"
#include "System.h"


class AppConfig {
  public:
    wxString CurrentIso;
    wxString CurrentELF;

    CDVD_SourceType CdvdSource = CDVD_SourceType::Iso;
    Pcsx2Config     EmuOptions;

  public:
    void LoadSave(IniInterface &ini, SettingsWrapper &wrap);
};
class Pcsx2App {

  public:
    wxString m_gamefile;
    wxString m_biosfile;
    bool     runnning = true;

    ExecutorThread                      SysExecutorThread;
    std::unique_ptr<SysCpuProviderPack> m_CpuProviders;
    std::unique_ptr<SysMainMemory>      m_VmReserve;

  public:
    Pcsx2App();
    virtual ~Pcsx2App();

    void OpenGsPanel();
    void CloseGsPanel();
    bool OnInit();
    void LogicalVsync();
};

extern __aligned16 AppCoreThread CoreThread;
extern __aligned16 SysMtgsThread mtgsThread;
