/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */
 *	Note that protocols except ssh aren't supported if USE_LIBSSH2 is
 *defined.
 */

#include <pobl/bl_def.h> /* USE_WIN32API */

#if defined(USE_WIN32API) || defined(USE_LIBSSH2)

#include "../ui_connect_dialog.h"

#include <stdio.h>       /* sprintf */
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_debug.h>
#include <pobl/bl_path.h> /* bl_parse_uri */

/* --- static variables --- */

static int selected_proto = -1;

static char **server_list;
static char *default_server;

/* These variables are set in IDOK. If empty string is input, nothing is set
 * (==NULL). */
static char *selected_server;
static char *selected_port;
static char *selected_user;
static char *selected_pass;
static char *selected_encoding;
static char *selected_exec_cmd;
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

static char *get_window_text(HWND win) {
  char *p;
  int len;

  if ((len = GetWindowTextLength(win)) > 0 && (p = malloc(len + 1))) {
    if (GetWindowText(win, p, len + 1) > 0) {
      return p;
    }

    free(p);
  }

  return NULL;
}

LRESULT CALLBACK dialog_proc(HWND dlgwin, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_INITDIALOG: {
      HWND win;
      HWND focus_win;
      char *user_env;

      focus_win = None;
      win = GetDlgItem(dlgwin, IDD_LIST);

      if (server_list) {
        int count;

        for (count = 0; server_list[count]; count++) {
          SendMessage(win, CB_ADDSTRING, 0, (LPARAM)server_list[count]);
        }

        if (count > 1) {
          focus_win = win;
        }
      } else {
        EnableWindow(win, FALSE);
      }

      selected_proto = IDD_SSH;
      user_env = getenv("USERNAME");

      if (default_server) {
        LRESULT res;
        char *user;
        int proto;
        char *server;
        char *port;
        char *encoding;

        res = SendMessage(win, CB_FINDSTRINGEXACT, 0, (LPARAM)default_server);
        if (res != CB_ERR) {
          SendMessage(win, CB_SETCURSEL, res, 0);
        }

        if (parse(&proto, &user, &server, &port, &encoding, bl_str_alloca_dup(default_server))) {
          SetWindowText(GetDlgItem(dlgwin, IDD_SERVER), server);

          if (port) {
            SetWindowText(GetDlgItem(dlgwin, IDD_PORT), port);
          }

          if (user || (user = user_env)) {
            SetWindowText(GetDlgItem(dlgwin, IDD_USER), user);
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

      if (focus_win) {
        SetFocus(focus_win);
      } else {
        SetFocus(GetDlgItem(dlgwin, selected_proto));
      }

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

          EndDialog(dlgwin, IDOK);

          break;

        case IDCANCEL:
          selected_proto = -1;
          EndDialog(dlgwin, IDCANCEL);

          break;

        case IDD_LIST:
          if (HIWORD(wparam) == CBN_SELCHANGE) {
            Window win;
            LRESULT idx;
            LRESULT len;
            char *seq;
            char *user;
            int proto;
            char *server;
            char *port;
            char *encoding;

            win = GetDlgItem(dlgwin, IDD_LIST);

            if ((idx = SendMessage(win, CB_GETCURSEL, 0, 0)) == CB_ERR ||
                (len = SendMessage(win, CB_GETLBTEXTLEN, idx, 0)) == CB_ERR ||
                (seq = alloca(len + 1)) == NULL ||
                (SendMessage(win, CB_GETLBTEXT, idx, seq)) == CB_ERR) {
              seq = NULL;
            }

            if (seq && parse(&proto, &user, &server, &port, &encoding, seq)) {
              SetWindowText(GetDlgItem(dlgwin, IDD_SERVER), server);

              if (port) {
                SetWindowText(GetDlgItem(dlgwin, IDD_PORT), port);
              }

              if (user || (user = getenv("USERNAME")) || (user = "")) {
                SetWindowText(GetDlgItem(dlgwin, IDD_USER), user);
              }

              if (proto == -1) {
                selected_proto = IDD_SSH;
              } else {
                selected_proto = proto;
              }

              if (encoding) {
                SetWindowText(GetDlgItem(dlgwin, IDD_ENCODING), encoding);
              }

              CheckRadioButton(dlgwin, IDD_SSH, IDD_RLOGIN, selected_proto);
            }
          }

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

/* --- global functions --- */

int ui_connect_dialog(char **uri,      /* Should be free'ed by those who call this. */
                      char **pass,     /* Same as uri. If pass is not input, "" is set. */
                      char **exec_cmd, /* Same as uri. If exec_cmd is not input, NULL is set. */
                      int *x11_fwd,    /* in/out */
                      char *display_name, Window parent_window, char **sv_list,
                      char *def_server /* (<user>@)(<proto>:)<server address>(:<encoding>). */
                      ) {
  int ret;
  char *proto;

  server_list = sv_list;
  default_server = def_server;

  use_x11_forwarding = *x11_fwd;

#ifdef DEBUG
  {
    char **p;
    bl_debug_printf("DEFAULT server %s\n", default_server);
    if (server_list) {
      bl_debug_printf("SERVER LIST ");
      p = server_list;
      while (*p) {
        bl_msg_printf("%s ", *p);
        p++;
      }
      bl_msg_printf("\n");
    }
  }
#endif

  DialogBox(GetModuleHandle(NULL), "ConnectDialog", parent_window, (DLGPROC)dialog_proc);

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

  *x11_fwd = use_x11_forwarding;

  /* Successfully */
  ret = 1;

end:
  selected_proto = -1;

  server_list = NULL;
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
  }

  selected_pass = NULL;
  selected_exec_cmd = NULL;

  return ret;
}

#endif
