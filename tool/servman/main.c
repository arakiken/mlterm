/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <dirent.h> /* mkdir on win32 */
#include <errno.h>

#define ID_SERVMAN 100

#define IDM_CONNECT    1
#define IDM_EXIT       2
#define IDM_EDIT       3
#define IDM_DEL        4
#define IDM_ADD        5
#define IDM_NEW_FOLDER 6
#define IDM_SORT       7
#define IDD_FOLDER    10
#define IDD_SSH       20
#define IDD_TELNET    21
#define IDD_RLOGIN    22
#define IDD_SERVER    23
#define IDD_PORT      24
#define IDD_USER      25
#define IDD_ENCODING  26
#define IDD_EXEC_CMD  27
#define IDD_X11       28

#define WM_TREEVIEW_LBUTTONUP (WM_USER + 1)
#define WM_TREEVIEW_MOUSEMOVE (WM_USER + 2)

#define MAXBUFLEN 1024

static char return_text[MAXBUFLEN]; /* Return value from dialog */

static char *default_server;

static char *class_name = "servman";
static HINSTANCE instance;

static WNDPROC default_tree_proc;
static HWND main_window;
static HTREEITEM drag_src;

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
  char *p;
  size_t len = strlen(seq);

  if (len > 6 && (strncmp(seq, "ssh://", 6) == 0 || strncmp(seq, "ftp://", 6) == 0)) {
    seq = (p = seq) + 6;
    *(seq - 3) = '\0';
  } else if (len > 9 && (strncmp(seq, "telnet://", 9) == 0 || strncmp(seq, "rlogin://", 9) == 0)) {
    seq = (p = seq) + 9;
    *(seq - 3) = '\0';
  } else {
    p = NULL;
  }

  if (p) {
    if (strcmp(p, "ssh") == 0) {
      *protoid = IDD_SSH;
    } else if (strcmp(p, "telnet") == 0) {
      *protoid = IDD_TELNET;
    } else if (strcmp(p, "rlogin") == 0) {
      *protoid = IDD_RLOGIN;
    } else {
      *protoid = -1;
    }
  } else {
    *protoid = -1;
  }

  if ((p = strchr(seq, '@'))) {
    *p = '\0';
    *user = seq;
    seq = p + 1;
  } else {
    *user = NULL;
  }

  *host = seq;

  if ((p = strchr(seq, ':'))) {
    *(p++) = '\0';

    if (isdigit((int)*p)) {
      seq = p;
      while (isdigit((int)(*(++p))));
      if (*p == '\0') {
        p = NULL;
      } else {
        *(p++) = '\0';
      }
    } else {
      seq = NULL;
    }
  } else {
    seq = NULL;
  }

  *port = seq;
  *encoding = p;

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

static size_t copy_str(char *dst, size_t dst_len /* >= 1 */, char prepend, char *src,
                       size_t src_len, char append) {
  size_t pre_len;

  if (prepend && dst_len >= 2) {
    dst[0] = prepend;
    dst++;
    dst_len--;
    pre_len = 1;
  } else {
    pre_len = 0;
  }

  if (dst_len < src_len) {
    src_len = dst_len - 1;
  }

  strncpy(dst, src, src_len);

  if (append && dst_len >= src_len + 2) {
    dst[src_len] = append;
    src_len++;
  }

  dst[src_len] = '\0';

  return pre_len + src_len;
}

LRESULT CALLBACK edit_dialog_proc(HWND dialog, UINT msg, WPARAM wparam, LPARAM lparam) {
  static int selected_proto = -1;
  static char *selected_server;
  static char *selected_port;
  static char *selected_user;
  static char *selected_encoding;
  static char *selected_exec_cmd;
  static int use_x11_forwarding;

  switch (msg) {
    case WM_INITDIALOG: {
      char *user_env;
      int item;

      item = selected_proto = IDD_SSH;
      user_env = getenv("USERNAME");

      use_x11_forwarding = 0;

      if (default_server) {
        char *str;
        char *user;
        int proto;
        char *server;
        char *port;
        char *encoding;
        char *p;
        size_t len;

        len = strlen(default_server);
        if (len > 4 && strcmp(default_server + len - 4, " x11") == 0) {
          default_server[(len -= 4)] = '\0';
          use_x11_forwarding = 1;
        }

        if ((p = strchr(default_server, ' '))) {
          *p = '\0';
          SetWindowText(GetDlgItem(dialog, IDD_EXEC_CMD), p + 1);
        }

        if ((str = alloca(strlen(default_server) + 1)) &&
            parse(&proto, &user, &server, &port, &encoding, strcpy(str, default_server))) {
          SetWindowText(GetDlgItem(dialog, IDD_SERVER), server);
          item = IDD_USER;

          if (port) {
            SetWindowText(GetDlgItem(dialog, IDD_PORT), port);
          }

          if (user || (user = user_env)) {
            SetWindowText(GetDlgItem(dialog, IDD_USER), user);
            item = IDD_ENCODING;
          }

          if (proto != -1) {
            selected_proto = proto;
          }

          if (encoding) {
            SetWindowText(GetDlgItem(dialog, IDD_ENCODING), encoding);
            item = IDD_EXEC_CMD;
          }
        }
      } else if (user_env) {
        SetWindowText(GetDlgItem(dialog, IDD_USER), user_env);
      }

      EnableWindow(GetDlgItem(dialog, IDD_TELNET), FALSE);
      EnableWindow(GetDlgItem(dialog, IDD_RLOGIN), FALSE);
      CheckRadioButton(dialog, IDD_SSH, IDD_RLOGIN, IDD_SSH);

      if (use_x11_forwarding) {
        SendMessage(GetDlgItem(dialog, IDD_X11), BM_SETCHECK, BST_CHECKED, 0);
      }

      if (item == IDD_SSH) {
        item = selected_proto;
      }

      SetFocus(GetDlgItem(dialog, item));

      return FALSE;
    }

    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case IDOK:
          selected_server = get_window_text(GetDlgItem(dialog, IDD_SERVER));
          selected_port = get_window_text(GetDlgItem(dialog, IDD_PORT));
          selected_user = get_window_text(GetDlgItem(dialog, IDD_USER));
          selected_encoding = get_window_text(GetDlgItem(dialog, IDD_ENCODING));
          selected_exec_cmd = get_window_text(GetDlgItem(dialog, IDD_EXEC_CMD));

          if (selected_server) {
            size_t total_len;
#if 0
            if (selected_proto == IDD_SSH) {
              strcpy(return_text, "ssh://");
              total_len = 6;
            } else
#endif
            if (selected_proto == IDD_TELNET) {
              strcpy(return_text, "telnet://");
              total_len = 9;
            } else if (selected_proto == IDD_RLOGIN) {
              strcpy(return_text, "rlogin://");
              total_len = 9;
            } else {
              return_text[0] = '\0';
              total_len = 0;
            }

            if (selected_user) {
              total_len += copy_str(return_text + total_len, sizeof(return_text) - total_len,
                                    0, selected_user, strlen(selected_user), '@');
            }

            total_len += copy_str(return_text + total_len, sizeof(return_text) - total_len,
                                  0, selected_server, strlen(selected_server), 0);

            if (selected_port) {
              total_len += copy_str(return_text + total_len,
                                    sizeof(return_text) - total_len,
                                    ':', selected_port, strlen(selected_port), 0);
            }

            if (selected_encoding) {
              total_len += copy_str(return_text + total_len,
                                    sizeof(return_text) - total_len,
                                    ':', selected_encoding, strlen(selected_encoding), 0);
            }

            if (selected_exec_cmd) {
              total_len += copy_str(return_text + total_len,
                                    sizeof(return_text) - total_len,
                                    ' ', selected_exec_cmd, strlen(selected_exec_cmd), 0);
            }

            if (use_x11_forwarding) {
              total_len += copy_str(return_text + total_len,
                                    sizeof(return_text) - total_len, ' ', "x11", 3, 0);
            }

            EndDialog(dialog, IDOK);
          } else {
            EndDialog(dialog, IDCANCEL);
          }

          break;

        case IDCANCEL:
          EndDialog(dialog, IDCANCEL);

          break;

        case IDD_SSH:
        case IDD_TELNET:
        case IDD_RLOGIN:
          selected_proto = LOWORD(wparam);
          CheckRadioButton(dialog, IDD_SSH, IDD_RLOGIN, selected_proto);

          break;

        case IDD_X11:
          use_x11_forwarding =
              (SendMessage(GetDlgItem(dialog, IDD_X11), BM_GETCHECK, 0, 0) == BST_CHECKED);
          break;

        default:
          return FALSE;
      }

    default:
      return FALSE;
  }

  return TRUE;
}

LRESULT CALLBACK folder_dialog_proc(HWND dialog, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_INITDIALOG:
    SetFocus(GetDlgItem(dialog, IDD_FOLDER));
    break;

  case WM_COMMAND:
    switch (LOWORD(wp)) {
    case IDOK:
      Edit_GetText(GetDlgItem(dialog, IDD_FOLDER), return_text, sizeof(return_text));
      EndDialog(dialog, IDOK);
      break;

    case IDCANCEL:
      EndDialog(dialog, IDCANCEL);
      break;
    }
    break;
  }
  return FALSE;
}

static char *get_item_info(HWND tree, HTREEITEM item, char *str, size_t len, int *is_dir) {
  TVITEM item_info;

  memset(&item_info, 0, sizeof(TVITEM));
  item_info.hItem = item;
  item_info.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_PARAM;
  item_info.pszText = str;
  item_info.cchTextMax = len;
  TreeView_GetItem(tree, &item_info);

  if (is_dir) {
    *is_dir = item_info.lParam;
  }

  return str;
}

static HTREEITEM find_item(HWND tree, HTREEITEM item, char *name, int *is_dir) {
  {
    char str[MAXBUFLEN];

    if (get_item_info(tree, item, str, sizeof(str), is_dir) &&
        strcmp(str, name) == 0) {
      return item;
    }
  }

  if ((item = TreeView_GetNextSibling(tree, item))) {
    return find_item(tree, item, name, is_dir);
  } else {
    return NULL;
  }
}

static int is_dir_item(HWND tree, HTREEITEM item) {
  TVITEM item_info;

  memset(&item_info, 0, sizeof(TVITEM));
  item_info.hItem = item;
  item_info.mask = TVIF_HANDLE | TVIF_PARAM;
  TreeView_GetItem(tree, &item_info);

  return item_info.lParam;
}

static void change_item_text(HWND tree, HTREEITEM item, char *text) {
  TVITEM item_info;

  memset(&item_info, 0, sizeof(TVITEM));
  item_info.hItem = item;
  item_info.mask = TVIF_HANDLE | TVIF_TEXT;
  item_info.pszText = text;
  item_info.cchTextMax = strlen(text);
  TreeView_SetItem(tree, &item_info);
}

static HTREEITEM add_item(HWND tree, HTREEITEM parent, char *name, int is_dir) {
  TV_INSERTSTRUCT ins;
  HTREEITEM item;
  int is_dir2;

  if (strcmp(name, "../") == 0) {
    return TreeView_GetParent(tree, parent);
  } else if (strcmp(name, "./") == 0) {
    return parent;
  }

  if ((item = TreeView_GetChild(tree, parent)) &&
      (item = find_item(tree, item, name, &is_dir2))) {
    if (is_dir == is_dir2) {
      return item;
    }

    TreeView_DeleteItem(tree, item);
  }

  memset(&ins, 0, sizeof(ins));
  ins.hParent = parent;
  ins.hInsertAfter = TVI_LAST;
  ins.item.mask = TVIF_TEXT | TVIF_PARAM;
  ins.item.pszText = name;
  ins.item.lParam = is_dir;

  return TreeView_InsertItem(tree, &ins);
}

static void add_path(HWND tree, char *path) {
  char *name;
  HTREEITEM parent = TVI_ROOT;

  if (*path == '/') {
    path++;
  }
  name = path;

  while (1) {
    if (*path == '/') {
      char orig = *(++path);
      *path = '\0';
      parent = add_item(tree, parent, name, 1);
      *path = orig;
      name = path;
    } else if (*path == '\0') {
      if (*name != '\0') {
        parent = add_item(tree, parent, name, 0);
      }
      break;
    } else {
      path++;
    }
  }
}

static void save_tree_intern(HWND tree, HTREEITEM item, char *str, char *substr, size_t len,
                             FILE *fp) {
  TVITEM item_info;
  HTREEITEM child;
  memset(&item_info, 0, sizeof(TVITEM));
  item_info.hItem = item;
  item_info.mask = TVIF_HANDLE | TVIF_TEXT;
  item_info.pszText = substr;
  item_info.cchTextMax = len;
  TreeView_GetItem(tree, &item_info);

  if ((child = TreeView_GetChild(tree, item))) {
    size_t l = strlen(substr);
    substr += l;
    len -= l;

    do {
      save_tree_intern(tree, child, str, substr, len, fp);
    } while ((child = TreeView_GetNextSibling(tree, child)));
  } else {
    fprintf(fp, "%s\n", str);
  }
}

static char *get_conf_path(int ensure_dir) {
  char *path;
  char *home;

  if ((home = getenv("HOMEPATH"))) {
    if ((path = malloc(strlen(home) + 7 + 8 + 1))) {
      sprintf(path, "%s\\mlterm", home);
      if (!ensure_dir || mkdir(path) == 0 || errno == EEXIST) {
        strcat(path, "\\servers");

        return path;
      }

      if (ensure_dir) {
        MessageBox(main_window, "Failed to save", "Failure", MB_OK);
      }

      free(path);
    }
  }

  return NULL;
}

static void save_tree(HWND tree) {
  HTREEITEM item = TreeView_GetRoot(tree);
  char *path;

  if (item && (path = get_conf_path(1))) {
    FILE *fp = fopen(path, "wb");

    if (fp) {
      char str[MAXBUFLEN];

      str[0] = '/';
      do {
        save_tree_intern(tree, item, str, str + 1, sizeof(str) - 1, fp);
      } while ((item = TreeView_GetNextSibling(tree, item)));

      fclose(fp);
    }

    free(path);
  }
}

static void create_tree(HWND tree) {
  char *path;

  if ((path = get_conf_path(0))) {
    FILE *fp = fopen(path, "rb");

    if (fp) {
      char buf[MAXBUFLEN];

      while (fgets(buf, sizeof(buf), fp) && buf[0] != '\0') {
        if (buf[strlen(buf) - 1] == '\n') {
          buf[strlen(buf) - 1] = '\0';
        }
        add_path(tree, buf);
      }

      fclose(fp);
    }

    free(path);
  }
}

static void copy_tree(HWND tree, HTREEITEM dst, HTREEITEM src) {
  HTREEITEM child;

  {
    TV_INSERTSTRUCT ins;
    TVITEM item_info;
    char str[1024];

    memset(&item_info, 0, sizeof(TVITEM));
    item_info.hItem = src;
    item_info.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_PARAM;
    item_info.pszText = str;
    item_info.cchTextMax = sizeof(str);
    TreeView_GetItem(tree, &item_info);

    memset(&ins, 0, sizeof(ins));
    ins.hParent = dst;
    ins.item = item_info;
    ins.hInsertAfter = TVI_LAST;

    dst = TreeView_InsertItem(tree, &ins);
  }

  if ((child = TreeView_GetChild(tree, src))) {
    do {
      copy_tree(tree, dst, child);
    } while ((child = TreeView_GetNextSibling(tree, child)));
  }
}

static void output_server(HWND tree) {
  HTREEITEM item;

  if ((item = TreeView_GetSelection(tree))) {
    int is_dir;
    char str[MAXBUFLEN];

    if (get_item_info(tree, item, str, sizeof(str), &is_dir) && !is_dir) {
      fputs(str, stdout);
      fflush(stdout);

      save_tree(tree);
      DestroyWindow(main_window);
    }
  }
}

static LRESULT CALLBACK tree_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
  switch(msg) {
  case WM_MOUSEMOVE:
    if (drag_src) {
      SendMessage(main_window, WM_TREEVIEW_MOUSEMOVE, wp, lp);
    }
    break;

  case WM_LBUTTONUP:
    if (drag_src) {
      SendMessage(main_window, WM_TREEVIEW_LBUTTONUP, wp, lp);
    }
    break;
  }

  return CallWindowProc((WNDPROC)default_tree_proc, window, msg, wp, lp);
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
  static HWND tree;
  TV_DISPINFO *ptv_disp;
  HTREEITEM item;
  TV_HITTESTINFO tvhtst;

  switch (msg) {
  case WM_CREATE:
    InitCommonControls();
    tree = CreateWindowEx(0, WC_TREEVIEW, "",
                           WS_CHILD | WS_BORDER | WS_VISIBLE | TVS_HASLINES |
                           TVS_HASBUTTONS | TVS_LINESATROOT | TVS_EDITLABELS,
                           0, 0, 0, 0, window, (HMENU)ID_SERVMAN, instance, NULL);
    default_tree_proc = (WNDPROC)GetWindowLongPtr(tree, GWLP_WNDPROC);
    SetWindowLongPtr(tree, GWLP_WNDPROC, (LONG_PTR)tree_proc);
    SetFocus(tree);
    create_tree(tree);

    break;

  case WM_CONTEXTMENU:
    tvhtst.pt.x = LOWORD(lp);
    tvhtst.pt.y = HIWORD(lp);
    ScreenToClient(tree, &tvhtst.pt);
    item = TreeView_HitTest(tree, &tvhtst);
    if (item) {
      HMENU menu, submenu;

      TreeView_SelectItem(tree, item);
      menu = GetMenu(window);
      submenu = GetSubMenu(menu, 1);
      TrackPopupMenu(submenu, TPM_CENTERALIGN, (int)LOWORD(lp), (int)HIWORD(lp), 0, window, NULL);
    }
    break;

  case WM_NOTIFY:
    if (wp == ID_SERVMAN) {
      ptv_disp = (TV_DISPINFO *)lp;
      if (ptv_disp->hdr.code == NM_DBLCLK || ptv_disp->hdr.code == NM_RETURN) {
        output_server(tree);
      } else if (ptv_disp->hdr.code == TVN_ENDLABELEDIT) {
        TreeView_SetItem(tree, &ptv_disp->item);
      } else if (ptv_disp->hdr.code == TVN_KEYDOWN) {
        if (((NMTVKEYDOWN*)lp)->wVKey == VK_F2) {
          if ((item = TreeView_GetSelection(tree))) {
            TreeView_EditLabel(tree, item);
          }
        } else if (((NMTVKEYDOWN*)lp)->wVKey == VK_DELETE) {
          if ((item = TreeView_GetSelection(tree))) {
            TreeView_DeleteItem(tree, item);
          }
        }
      } else if (ptv_disp->hdr.code == TVN_BEGINDRAG) {
        drag_src = ((NMTREEVIEW*)lp)->itemNew.hItem;
      } else if (ptv_disp->hdr.code == NM_CLICK) {
        TreeView_SelectItem(tree, NULL);
      }
    }
    break;

  case WM_COMMAND:
    switch (LOWORD(wp)) {
    case IDM_SORT:
      if (!(item = TreeView_GetSelection(tree))) {
        item = TVI_ROOT;
      }

      if(TreeView_SortChildren(tree, item, 0) == FALSE) {
        MessageBox(window, "Failed to sort", "Failure", MB_OK);
      }
      break;

    case IDM_CONNECT:
      output_server(tree);
      break;

    case IDM_EXIT:
      save_tree(tree);
      DestroyWindow(window);
      break;

    case IDM_EDIT:
      item = TreeView_GetSelection(tree);
      if (item) {
        char str[MAXBUFLEN];
        int is_dir;

        if (get_item_info(tree, item, str, sizeof(str), &is_dir)) {
          if (is_dir) {
            TreeView_EditLabel(tree, item);
          } else {
            default_server = str;

            if (DialogBox(GetModuleHandle(NULL), "SM_EDIT_DIALOG", window,
                          (DLGPROC)edit_dialog_proc) == IDOK) {
              change_item_text(tree, item, return_text);
            }
          }
        }
      } else {
        MessageBox(window, "No item is selected", "Unselected", MB_OK);
      }
      break;

    case IDM_DEL:
      item = TreeView_GetSelection(tree);
      if (item) {
        TreeView_DeleteItem(tree, item);
      } else {
        MessageBox(window, "No item is selected", "Unselected", MB_OK);
      }
      break;

    case IDM_ADD:
      if (!(item = TreeView_GetSelection(tree))) {
        item = TVI_ROOT;
      } else if (!is_dir_item(tree, item)) {
        MessageBox(window, "Selected item is not a directory", "Failure", MB_OK);
        break;
      }

      default_server = NULL;
      if (DialogBox(GetModuleHandle(NULL), "SM_EDIT_DIALOG", window,
                    (DLGPROC)edit_dialog_proc) == IDOK) {
        item = add_item(tree, item, return_text, 0);
        TreeView_EnsureVisible(tree, item);
      }
      break;

    case IDM_NEW_FOLDER:
      if (!(item = TreeView_GetSelection(tree))) {
        item = TVI_ROOT;
      } else if (!is_dir_item(tree, item)) {
        MessageBox(window, "Selected item is not a directory", "Failure", MB_OK);
        break;
      }

      if (DialogBox(instance, "SM_FOLDER_DIALOG", window, (DLGPROC)folder_dialog_proc) == IDOK) {
        char *p;
        if ((p = alloca(strlen(return_text) + 2))) {
          strcpy(p, return_text);
          strcat(p, "/");
          item = add_item(tree, item, p, 1);
          TreeView_EnsureVisible(tree, item);
        }
      }
      break;
    }
    break;

  case WM_TREEVIEW_MOUSEMOVE:
    {
      POINT pt;
      TVHITTESTINFO info;

      GetCursorPos(&pt);
      info.pt = pt;
      ScreenToClient(tree, &info.pt);
      TreeView_HitTest(tree, &info);
      if (info.flags & TVHT_ONITEM) {
        SendMessage(tree, TVM_SELECTITEM, TVGN_DROPHILITE, (LPARAM)info.hItem);
      } else {
        SendMessage(tree, TVM_SELECTITEM, TVGN_DROPHILITE, 0L);
      }
    }
    break;

  case WM_TREEVIEW_LBUTTONUP:
    {
      HTREEITEM drag_dst;
      TVHITTESTINFO info;

      info.flags = TVHT_ONITEM;
      info.pt.x = LOWORD(lp);
      info.pt.y = HIWORD(lp);

      if (!(drag_dst = TreeView_HitTest(tree, &info))) {
        drag_dst = TVI_ROOT;
      } else if (!is_dir_item(tree, drag_dst)) {
        drag_dst = TreeView_GetParent(tree, drag_dst);
      }

      copy_tree(tree, drag_dst, drag_src);
      TreeView_DeleteItem(tree, drag_src);

      TreeView_SelectItem(tree, NULL);
      drag_src = NULL;
    }
    break;

  case WM_SIZE:
    MoveWindow(tree, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
    break;

  case WM_CLOSE:
    save_tree(tree);
    DestroyWindow(window);
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  default:
    return (DefWindowProc(window, msg, wp, lp));
  }

  return 0L;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev_inst, LPSTR cmdline, int cmd_show) {
  WNDCLASSEX wc;
  MSG msg;

  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = window_proc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = instance;
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszMenuName = "SM_MENU";
  wc.lpszClassName = (LPCSTR)class_name;
  wc.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

  if (!RegisterClassEx(&wc)) {
    return -1;
  }

  if (!(main_window = CreateWindow(class_name, "Server Manager",
                                   WS_OVERLAPPEDWINDOW,
                                   (GetSystemMetrics(SM_CXSCREEN) - 400) / 2,
                                   (GetSystemMetrics(SM_CYSCREEN) - 300) / 2,
                                   400, 300, NULL, NULL, inst, NULL))) {
    return -1;
  }

  instance = inst;
  ShowWindow(main_window, cmd_show);
  UpdateWindow(main_window);

  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return msg.wParam;
}
