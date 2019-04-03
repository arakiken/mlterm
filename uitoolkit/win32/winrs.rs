/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

MLTERM_ICON ICON "mlterm-icon-win32.ico"

#include <pobl/bl_def.h>		/* USE_WIN32API */

#if defined(USE_WIN32API) || defined(USE_LIBSSH2)

#ifdef USE_SDL2
#define USE_WIN32GUI
#undef USE_SDL2
#include "../win32/ui_connect_dialog.h"
#else
#include "ui_connect_dialog.h"
#endif

ConnectDialog DIALOG 20, 20, 151, 138
  STYLE WS_POPUP | WS_DLGFRAME | DS_CENTER
  {
    GROUPBOX "Protocol", -1, 4, 4, 143, 24
    RADIOBUTTON "&SSH", IDD_SSH, 8, 14, 28, 12
    RADIOBUTTON "&MOSH", IDD_MOSH, 36, 14, 35, 12
    RADIOBUTTON "&TELNET", IDD_TELNET, 71, 14, 35, 12
    RADIOBUTTON "&RLOGIN", IDD_RLOGIN, 106, 14, 35, 12
    LTEXT "Server", -1, 4, 30, 30, 8
    EDITTEXT IDD_SERVER, 38, 30, 96, 10, ES_AUTOHSCROLL
    LTEXT "Port", -1, 4, 41, 30, 8
    EDITTEXT IDD_PORT, 38, 41, 96, 10, ES_AUTOHSCROLL
    LTEXT "User", -1, 4, 52, 30, 8
    EDITTEXT IDD_USER, 38, 52, 96, 10, ES_AUTOHSCROLL
    LTEXT "Pass", -1, 4, 63, 30, 8
    EDITTEXT IDD_PASS, 38, 63, 96, 10, ES_PASSWORD | ES_AUTOHSCROLL
    LTEXT "Encoding", -1, 4, 74, 30, 8
    EDITTEXT IDD_ENCODING, 38, 74, 96, 10, ES_AUTOHSCROLL
    LTEXT "ExecCmd",-1, 4, 85, 30, 8
    EDITTEXT IDD_EXEC_CMD, 38, 85, 96, 10, ES_AUTOHSCROLL
    LTEXT "PrivKey", -1, 4, 96, 30, 8
    EDITTEXT IDD_SSH_PRIVKEY, 38, 96, 96, 10, ES_AUTOHSCROLL
    CONTROL "X11 forwarding", IDD_X11, "Button", BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 4, 107, 126, 8
    DEFPUSHBUTTON "OK", IDOK, 20, 119, 40, 12
    PUSHBUTTON "Cancel", IDCANCEL, 80, 119, 40, 12
  }

#endif /* USE_WIN32API */
