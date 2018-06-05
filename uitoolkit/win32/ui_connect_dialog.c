/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <pobl/bl_def.h> /* USE_WIN32API */

/* Note that protocols except ssh aren't supported if USE_LIBSSH2 is defined.*/
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)

#include "ui_connect_dialog.h"

#include <stdio.h>       /* sprintf */
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_debug.h>
#include <pobl/bl_path.h> /* bl_parse_uri */

/* --- static variables --- */

static int selected_proto = -1;

static char *default_server;

/* These variables are set in IDOK. If empty string is input, nothing is set
 * (==NULL). */
static char *selected_server;
static char *selected_port;
static char *selected_user;
static char *selected_pass;
static char *selected_encoding;
static char *selected_exec_cmd;
static char *selected_ssh_privkey;
static int use_x11_forwarding;

/* --- static functions --- */

/*
 * Parsing "<proto>://<user>@<host>:<port>:<encoding>".
 */
static int parse(int *protoid, /* If seq doesn't have proto, -1 is set. */
                 char **user,  /* If seq doesn't have user, NULL is set. */
                 char **host, char **port,
                 char **encoding, /* If seq doesn't have encoding, NULL is set. */
                 char *seq        /* broken in this function. */
                 ) {
  char *proto;

  if (!bl_parse_uri(&proto, user, host, port, NULL, encoding, seq)) {
    return 0;
  }

  if (proto) {
    if (strcmp(proto, "ssh") == 0) {
      *protoid = IDD_SSH;
    } else if (strcmp(proto, "telnet") == 0) {
      *protoid = IDD_TELNET;
    } else if (strcmp(proto, "rlogin") == 0) {
      *protoid = IDD_RLOGIN;
    } else {
      *protoid = -1;
    }
  } else {
    *protoid = -1;
  }

  return 1;
}

/* Return UTF-8 or MB text */
static char *get_window_text(HWND win) {
  WCHAR *wstr;
  int wlen;

  if ((wlen = GetWindowTextLengthW(win)) > 0 && (wstr = alloca((wlen + 1) * sizeof(WCHAR)))) {
    if (GetWindowTextW(win, wstr, wlen + 1) > 0) {
      int nbytes;
      char *str;
      UINT cp;

#ifdef USE_WIN32API
      cp = CP_THREAD_ACP;
#else
      cp = CP_UTF8;
#endif

      nbytes = WideCharToMultiByte(cp, 0, wstr, wlen, NULL, 0, NULL, NULL);
      if (nbytes > 0 && (str = malloc(nbytes + 1))) {
        if ((nbytes = WideCharToMultiByte(cp, 0, wstr, wlen, str, nbytes, NULL, NULL)) > 0) {
          str[nbytes] = '\0';

          return str;
        }

        free(str);
      }
    }
  }

  return NULL;
}

static void set_window_text(HWND win, char *str /* utf8 */) {
  WCHAR *wstr;
  int wlen;
  size_t len = strlen(str);

  wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, NULL, 0);
  if (wlen > 0 && (wstr = alloca((wlen + 1) * sizeof(WCHAR)))) {
    if ((wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, wstr, wlen)) > 0) {
      wstr[wlen] = 0;
      SetWindowTextW(win, wstr);
    }
  }
}

LRESULT CALLBACK dialog_proc(HWND dlgwin, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_INITDIALOG: {
      char *user_env;
      int item;

      item = selected_proto = IDD_SSH;
      user_env = getenv("USERNAME");

      if (default_server) {
        char *user;
        int proto;
        char *server;
        char *port;
        char *encoding;

#if 1
        if (selected_exec_cmd) {
          SetWindowText(GetDlgItem(dlgwin, IDD_EXEC_CMD), selected_exec_cmd);
          selected_exec_cmd = NULL;
        }

        if (selected_ssh_privkey) {
          set_window_text(GetDlgItem(dlgwin, IDD_SSH_PRIVKEY), selected_ssh_privkey);
          selected_ssh_privkey = NULL;
        }
#endif

        if (parse(&proto, &user, &server, &port, &encoding, bl_str_alloca_dup(default_server))) {
          SetWindowText(GetDlgItem(dlgwin, IDD_SERVER), server);
          item = IDD_USER;

          if (port) {
            SetWindowText(GetDlgItem(dlgwin, IDD_PORT), port);
          }

          if (user || (user = user_env)) {
            SetWindowText(GetDlgItem(dlgwin, IDD_USER), user);
            item = IDD_PASS;
          }

#ifndef USE_LIBSSH2
          if (proto != -1) {
            selected_proto = proto;
          }
#endif

          if (encoding) {
            SetWindowText(GetDlgItem(dlgwin, IDD_ENCODING), encoding);
          }
        }
      } else if (user_env) {
        SetWindowText(GetDlgItem(dlgwin, IDD_USER), user_env);
      }

#ifdef USE_LIBSSH2
      EnableWindow(GetDlgItem(dlgwin, IDD_TELNET), FALSE);
      EnableWindow(GetDlgItem(dlgwin, IDD_RLOGIN), FALSE);
      CheckRadioButton(dlgwin, IDD_SSH, IDD_RLOGIN, IDD_SSH);
#else
      CheckRadioButton(dlgwin, IDD_SSH, IDD_RLOGIN, selected_proto);
#endif

#ifdef USE_LIBSSH2
      if (use_x11_forwarding) {
        SendMessage(GetDlgItem(dlgwin, IDD_X11), BM_SETCHECK, BST_CHECKED, 0);
      }
#else
      EnableWindow(GetDlgItem(dlgwin, IDD_X11), FALSE);
#endif

      if (item == IDD_SSH) {
        item = selected_proto;
      }

      SetFocus(GetDlgItem(dlgwin, item));

      return FALSE;
    }

    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case IDOK:
          selected_server = get_window_text(GetDlgItem(dlgwin, IDD_SERVER));
          selected_port = get_window_text(GetDlgItem(dlgwin, IDD_PORT));
          selected_user = get_window_text(GetDlgItem(dlgwin, IDD_USER));
          selected_pass = get_window_text(GetDlgItem(dlgwin, IDD_PASS));
          selected_encoding = get_window_text(GetDlgItem(dlgwin, IDD_ENCODING));
          selected_exec_cmd = get_window_text(GetDlgItem(dlgwin, IDD_EXEC_CMD));
          selected_ssh_privkey = get_window_text(GetDlgItem(dlgwin, IDD_SSH_PRIVKEY));

          EndDialog(dlgwin, IDOK);

          break;

        case IDCANCEL:
          selected_proto = -1;
          EndDialog(dlgwin, IDCANCEL);

          break;

        case IDD_SSH:
        case IDD_TELNET:
        case IDD_RLOGIN:
          selected_proto = LOWORD(wparam);
          CheckRadioButton(dlgwin, IDD_SSH, IDD_RLOGIN, selected_proto);

          break;

        case IDD_X11:
          use_x11_forwarding =
              (SendMessage(GetDlgItem(dlgwin, IDD_X11), BM_GETCHECK, 0, 0) == BST_CHECKED);
          break;

        default:
          return FALSE;
      }

    default:
      return FALSE;
  }

  return TRUE;
}

static char *exec_servman(char *server, DWORD server_len) {
  HANDLE input_write;
  HANDLE input_read_tmp;
  HANDLE input_read;
  SECURITY_ATTRIBUTES sa;
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
  DWORD len;

  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;

  if (!CreatePipe(&input_read_tmp, &input_write, &sa, 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreatePipe() failed.\n");
#endif

    return NULL;
  }

  if (!DuplicateHandle(GetCurrentProcess(), input_read_tmp, GetCurrentProcess(),
                       &input_read /* Address of new handle. */,
                       0, FALSE /* Make it uninheritable. */,
                       DUPLICATE_SAME_ACCESS)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " DuplicateHandle() failed.\n");
#endif

    CloseHandle(input_read_tmp);

    goto error;
  }

  CloseHandle(input_read_tmp);

  ZeroMemory(&si, sizeof(STARTUPINFO));
  si.cb = sizeof(STARTUPINFO);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
  si.hStdOutput = input_write;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  /*
   * Use this if you want to hide the child:
   *  si.wShowWindow = SW_HIDE;
   * Note that dwFlags must include STARTF_USESHOWWINDOW if you want to
   * use the wShowWindow flags.
   */

  if (!CreateProcess("servman.exe", "servman.exe", NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreateProcess() failed.\n");
#endif

    goto error;
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  CloseHandle(input_write);

  if (!ReadFile(input_read, server, server_len - 1, &len, NULL)) {
    CloseHandle(input_read);

    return NULL;
  }
  server[len] = '\0';

  CloseHandle(input_read);

  return server;

error:
  CloseHandle(input_write);
  CloseHandle(input_read);

  return NULL;
}

/* --- global functions --- */

int ui_connect_dialog(char **uri,      /* Should be free'ed by those who call this. */
                      char **pass,     /* Same as uri. If pass is not input, "" is set. */
                      char **exec_cmd, /* Same as uri. If exec_cmd is not input, NULL is set. */
                      char **privkey,  /* in/out */
                      int *x11_fwd,    /* in/out */
                      char *display_name, Window parent_window,
                      char *def_server /* (<user>@)(<proto>:)<server address>(:<encoding>). */
                      ) {
  int ret;
  char *proto;

  use_x11_forwarding = *x11_fwd;

  if ((!def_server || *def_server == '\0' || strcmp(def_server, "?") == 0) &&
      (default_server = alloca(1024))) {
    if ((default_server = exec_servman(default_server, 1024))) {
      size_t len = strlen(default_server);
      char *p;

      if (len > 4 && strcmp(default_server + len - 4, " x11") == 0) {
        default_server[(len -= 4)] = '\0';
        use_x11_forwarding = 1;
      }

#if 1
      if ((p = strchr(default_server, ' '))) {
        *p = '\0';
        selected_exec_cmd = p + 1;
      }
#endif
    }
  } else {
    default_server = def_server;
  }

  if (!(selected_ssh_privkey = *privkey)) {
    char *home;
    char *p;

    if ((home = bl_get_home_dir()) && (p = alloca(strlen(home) + 14 + 1))) {
#ifdef USE_WIN32API
      sprintf(p, "%s\\mlterm\\id_rsa", home);
#else
      sprintf(p, "%s/.ssh/id_rsa", home);
#endif

      selected_ssh_privkey = p;
    }
  }

#ifdef DEBUG
  bl_debug_printf("DEFAULT server %s\n", default_server);
#endif

  DialogBox(GetModuleHandle(NULL), "ConnectDialog",
#ifdef USE_SDL2
            NULL,
#else
            parent_window,
#endif
            (DLGPROC)dialog_proc);

  ret = 0;

  if (selected_server == NULL) {
    goto end;
  } else if (selected_proto == IDD_SSH) {
    proto = "ssh://";
  } else if (selected_proto == IDD_TELNET) {
    proto = "telnet://";
  } else if (selected_proto == IDD_RLOGIN) {
    proto = "rlogin://";
  } else {
    goto end;
  }

  if (!(*uri =
            malloc(strlen(proto) + (selected_user ? strlen(selected_user) + 1 : 0) +
                   strlen(selected_server) + 1 + (selected_port ? strlen(selected_port) + 1 : 0) +
                   (selected_encoding ? strlen(selected_encoding) + 1 : 0)))) {
    goto end;
  }

  (*uri)[0] = '\0';

  strcat(*uri, proto);

  if (selected_user) {
    strcat(*uri, selected_user);
    strcat(*uri, "@");
  }

  strcat(*uri, selected_server);

  if (selected_port) {
    strcat(*uri, ":");
    strcat(*uri, selected_port);
  }

  if (selected_encoding) {
    strcat(*uri, ":");
    strcat(*uri, selected_encoding);
  }

  *pass = selected_pass ? selected_pass : strdup("");
  *exec_cmd = selected_exec_cmd;
  *privkey = selected_ssh_privkey;
  *x11_fwd = use_x11_forwarding;

  /* Successfully */
  ret = 1;

end:
  selected_proto = -1;

  default_server = NULL;

  free(selected_server);
  selected_server = NULL;
  free(selected_port);
  selected_port = NULL;
  free(selected_user);
  selected_user = NULL;
  free(selected_encoding);
  selected_encoding = NULL;

  if (ret == 0) {
    free(selected_pass);
    free(selected_exec_cmd);
    free(selected_ssh_privkey);
  }

  selected_pass = NULL;
  selected_exec_cmd = NULL;
  selected_ssh_privkey = NULL;

  return ret;
}

#endif
