/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <sys/types.h>

#include <pobl/bl_def.h>    /* USE_WIN32API/HAVE_WINDOWS_H */
#include <pobl/bl_unistd.h> /* bl_getuid/bl_getgid */
#include <pobl/bl_conf_io.h>
#include <pobl/bl_privilege.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_dlfcn.h>

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "main_loop.h"

#if defined(USE_WIN32API)
#define CONFIG_PATH "."
#elif defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

#ifdef USE_WIN32API
#include <stdlib.h> /* _fmode */
#include <fcntl.h>  /* _O_BINARY */
static char *dummy_argv[] = {"mlterm", NULL};
#define argv (__argv ? __argv : dummy_argv)
#define argc __argc
#endif

/* --- static functions --- */

#if defined(HAVE_WINDOWS_H) && !defined(USE_WIN32API)

#include <stdio.h> /* sprintf */
#include <sys/utsname.h>

#include <pobl/bl_util.h>

static void check_console(void) {
  int count;
  HWND conwin;
  char app_name[6 + DIGIT_STR_LEN(u_int) + 1];
  HANDLE handle;

  if (!(handle = GetStdHandle(STD_OUTPUT_HANDLE)) || handle == INVALID_HANDLE_VALUE) {
#if 0
    struct utsname name;
    char *rel;

    if (uname(&name) == 0 && (rel = alloca(strlen(name.release) + 1))) {
      char *p;

      if ((p = strchr(strcpy(rel, name.release), '.'))) {
        int major;
        int minor;

        *p = '\0';
        major = atoi(rel);
        rel = p + 1;

        if ((p = strchr(rel, '.'))) {
          *p = '\0';
          minor = atoi(rel);

          if (major >= 2 || (major == 1 && minor >= 7)) {
            /*
             * Mlterm works without console
             * in cygwin 1.7 or later.
             */
            return;
          }
        }
      }
    }

    /* AllocConsole() after starting mlterm doesn't work on MSYS. */
    if (!AllocConsole())
#endif
    {
      return;
    }
  }

  /* Hide allocated console window */

  sprintf(app_name, "mlterm%08x", (unsigned int)GetCurrentThreadId());
  LockWindowUpdate(GetDesktopWindow());
  SetConsoleTitle(app_name);

  for (count = 0; count < 20; count++) {
    if ((conwin = FindWindow(NULL, app_name))) {
      ShowWindowAsync(conwin, SW_HIDE);
      break;
    }

    Sleep(40);
  }

  LockWindowUpdate(NULL);
}

#else

#define check_console() (1)

#endif

/* --- global functions --- */

#ifdef USE_WIN32API
int PASCAL WinMain(HINSTANCE hinst, HINSTANCE hprev, char *cmdline, int cmdshow)
#else
int main(int argc, char **argv)
#endif
{
#if defined(USE_WIN32API) && defined(USE_LIBSSH2)
  WSADATA wsadata;

  WSAStartup(MAKEWORD(2, 0), &wsadata);
#endif

#ifdef USE_WIN32API
  _fmode = _O_BINARY;
#endif

  check_console();

  /* Don't call before NSApplicationMain() */
#if !defined(USE_SDL2) || !defined(__APPLE__)
  /* normal user */
  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());
#endif

  bl_set_sys_conf_dir(CONFIG_PATH);

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(USE_FRAMEBUFFER)
  /*
   * XXX
   * It performs well to read as large amount of data as possible
   * on framebuffer on old machines.
   */
  vt_set_timeout_read_pty(0xffff); /* 65535 sec */
#endif

  if (!main_loop_init(argc, argv)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_term_manager_init() failed.\n");
#endif

    return 1;
  }

  main_loop_start();

#if defined(DEBUG) || defined(USE_WIN32GUI) || defined(__CYGWIN__) || defined(__MSYS__)
  main_loop_final();
#else
/* All resources are freed on exit. */
#endif

#if defined(USE_WIN32API) && defined(USE_LIBSSH2)
  WSACleanup();
#endif

  bl_dl_close_all();

  return 0;
}
