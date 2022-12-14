
#include "gui/AppCoreThread.h"
#include "common/WindowInfo.h"
#include "Global.h"
#include "Device.h"
#include "keyboard.h"
#include "state_management.h"
// #include "wx_dialog/dialog.h"
#ifndef __APPLE__
Display *GSdsp;
Window   GSwin;
#endif
static void SysMessage(const char *fmt, ...)
{
    va_list list;
    char    msg[512];
    va_start(list, fmt);
    vsprintf(msg, fmt, list);
    va_end(list);
    if (msg[strlen(msg) - 1] == '\n')
        msg[strlen(msg) - 1] = 0;
    // wxMessageDialog dialog(nullptr, msg, "Info", wxOK);
    // dialog.ShowModal();
}
s32 _PADopen(const WindowInfo &wi)
{
#ifndef __APPLE__
    if (wi.type != WindowInfo::Type::X11)
        return -1;
    GSdsp = static_cast<Display *>(wi.display_connection);
    GSwin = reinterpret_cast<Window>(wi.window_handle);
#endif
    return 0;
}
void _PADclose()
{
    device_manager.devices.clear();
}
void PADupdate(int pad)
{
#ifndef __APPLE__
    static int count = 0;
    count++;
    if ((count & 0xFFF) == 0 && GSdsp) {
        // XResetScreenSaver(GSdsp);
    }
#endif
    device_manager.Update();
}
void PADconfigure()
{
    // ScopedCoreThreadPause paused_core;
    PADLoadConfig();
    // DisplayDialog();
    // paused_core.AllowResume();
    return;
}
