
#define WXINTL_NO_GETTEXT_MACRO

#include <wx/app.h>
#if defined(__UNIX__)
#include <signal.h>
#endif

#include "common/Dependencies.h"
#include "common/Threading.h"
#include "common/General.h"

Fnptr_OutOfMemory pxDoOutOfMemory = NULL;

#ifdef PCSX2_DEVBUILD
#define DEVASSERT_INLINE __noinline
#else
#define DEVASSERT_INLINE __fi
#endif

static DeclareTls(int) s_assert_guard(0);

pxDoAssertFnType *pxDoAssert = pxAssertImpl_LogIt;

wxString DiagnosticOrigin::ToString(const wxChar *msg) const
{
    FastFormatUnicode message;

    message.Write(L"%ls(%d) : assertion failed:\n", srcfile, line);

    if (function != NULL)
        message.Write("    Function:  %s\n", function);

    message.Write(L"    Thread:    %s\n", WX_STR(Threading::pxGetCurrentThreadName()));

    if (condition != NULL)
        message.Write(L"    Condition: %ls\n", condition);

    if (msg != NULL)
        message.Write(L"    Message:   %ls\n", msg);

    return message;
}


void pxTrap()
{
#if defined(__WXMSW__) && !defined(__WXMICROWIN__)
    __debugbreak();
#elif defined(__WXMAC__) && !defined(__DARWIN__)
#if __powerc
    Debugger();
#else
    SysBreak();
#endif
#elif defined(_MSL_USING_MW_C_HEADERS) && _MSL_USING_MW_C_HEADERS
    Debugger();
#elif defined(__UNIX__)
    raise(SIGTRAP);
#else
#endif
}


bool pxAssertImpl_LogIt(const DiagnosticOrigin &origin, const wxChar *msg)
{
    wxMessageOutputDebug().Printf(L"%s", origin.ToString(msg).c_str());
    pxTrap();
    return false;
}


DEVASSERT_INLINE void pxOnAssert(const DiagnosticOrigin &origin, const wxString &msg)
{

    RecursionGuard guard(s_assert_guard);
    if (guard.Counter > 2) {
        return pxTrap();
    }


    bool trapit;

    if (pxDoAssert == NULL) {
        trapit = pxAssertImpl_LogIt(origin, msg.wc_str());
    } else {
        trapit = pxDoAssert(origin, msg.wc_str());
    }

    if (trapit) {
        pxTrap();
    }
}


BaseException &BaseException::SetBothMsgs(const wxChar *msg_diag)
{
    m_message_user = msg_diag ? wxString(wxGetTranslation(msg_diag)) : wxString("");
    return SetDiagMsg(msg_diag);
}

BaseException &BaseException::SetDiagMsg(const wxString &msg_diag)
{
    m_message_diag = msg_diag;
    return *this;
}

BaseException &BaseException::SetUserMsg(const wxString &msg_user)
{
    m_message_user = msg_user;
    return *this;
}

wxString BaseException::FormatDiagnosticMessage() const
{
    return m_message_diag;
}

wxString BaseException::FormatDisplayMessage() const
{
    return m_message_diag;
}

Exception::RuntimeError::RuntimeError(const std::runtime_error &ex, const wxString &prefix)
{
    IsSilent = false;

    SetDiagMsg(pxsFmt(L"STL Runtime Error%s: %s", (prefix.IsEmpty() ? L"" : pxsFmt(L" (%s)", WX_STR(prefix)).c_str()),
                      WX_STR(fromUTF8(ex.what()))));
}

Exception::RuntimeError::RuntimeError(const std::exception &ex, const wxString &prefix)
{
    IsSilent = false;

    SetDiagMsg(pxsFmt(L"STL Exception%s: %s", (prefix.IsEmpty() ? L"" : pxsFmt(L" (%s)", WX_STR(prefix)).c_str()),
                      WX_STR(fromUTF8(ex.what()))));
}

Exception::OutOfMemory::OutOfMemory(const wxString &allocdesc)
{
    AllocDescription = allocdesc;
}

wxString Exception::OutOfMemory::FormatDiagnosticMessage() const
{
    FastFormatUnicode retmsg;


    return retmsg;
}

wxString Exception::OutOfMemory::FormatDisplayMessage() const
{
    FastFormatUnicode retmsg;


    return retmsg;
}


Exception::VirtualMemoryMapConflict::VirtualMemoryMapConflict(const wxString &allocdesc)
{
    AllocDescription = allocdesc;
    // m_message_user = _("Virtual memory mapping failure!  Your system may have conflicting device drivers, services,
    // or "
    //                    "may simply have insufficient memory or resources to meet PCSX2's lofty needs.");
}

wxString Exception::VirtualMemoryMapConflict::FormatDiagnosticMessage() const
{
    FastFormatUnicode retmsg;


    return retmsg;
}

wxString Exception::VirtualMemoryMapConflict::FormatDisplayMessage() const
{
    FastFormatUnicode retmsg;


    return retmsg;
}


wxString Exception::CancelEvent::FormatDiagnosticMessage() const
{
    return L"Action canceled: " + m_message_diag;
}

wxString Exception::CancelEvent::FormatDisplayMessage() const
{
    return L"Action canceled: " + m_message_diag;
}

wxString Exception::BadStream::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    _formatDiagMsg(retval);
    return retval;
}

wxString Exception::BadStream::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    _formatUserMsg(retval);
    return retval;
}

void Exception::BadStream::_formatDiagMsg(FastFormatUnicode &dest) const
{
    dest.Write(L"Path: ");
    if (!StreamName.IsEmpty())
        dest.Write(L"%s", WX_STR(StreamName));
    else
        dest.Write(L"[Unnamed or unknown]");

    if (!m_message_diag.IsEmpty())
        dest.Write(L"\n%s", WX_STR(m_message_diag));
}

void Exception::BadStream::_formatUserMsg(FastFormatUnicode &dest) const
{
}

wxString Exception::CannotCreateStream::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    return retval;
}

wxString Exception::CannotCreateStream::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    return retval;
}

wxString Exception::FileNotFound::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    return retval;
}

wxString Exception::FileNotFound::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    return retval;
}

wxString Exception::AccessDenied::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    return retval;
}

wxString Exception::AccessDenied::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    return retval;
}

wxString Exception::EndOfStream::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    retval.Write("Unexpected end of file or stream.\n");
    _formatDiagMsg(retval);
    return retval;
}

wxString Exception::EndOfStream::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    // retval.Write(_("Unexpected end of file or stream encountered.  File is probably truncated or corrupted."));
    // retval.Write("\n");
    // _formatUserMsg(retval);
    return retval;
}


BaseException *Exception::FromErrno(const wxString &streamname, int errcode)
{
    pxAssumeDev(errcode != 0, "Invalid NULL error code?  (errno)");

    switch (errcode) {
        case EINVAL:
            pxFailDev(L"Invalid argument");
            return &(new Exception::BadStream(streamname))
                        ->SetDiagMsg(L"Invalid argument? (likely caused by an unforgivable programmer error!)");

        case EACCES:
            return new Exception::AccessDenied(streamname);

        case EMFILE:
            return &(new Exception::CannotCreateStream(streamname))->SetDiagMsg(L"Too many open files");

        case EEXIST:
            return &(new Exception::CannotCreateStream(streamname))->SetDiagMsg(L"File already exists");

        case ENOENT:
            return new Exception::FileNotFound(streamname);

        case EPIPE:
            return &(new Exception::BadStream(streamname))->SetDiagMsg(L"Broken pipe");

        case EBADF:
            return &(new Exception::BadStream(streamname))->SetDiagMsg(L"Bad file number");

        default:
            return &(new Exception::BadStream(streamname))
                        ->SetDiagMsg(pxsFmt(L"General file/stream error [errno: %d]", errcode));
    }
}
