/*
 *	$Id$
 */

#include "ui_event_source.h"

#include <pobl/bl_def.h> /* USE_WIN32API */

#ifndef USE_WIN32API
#include <string.h>       /* memset/memcpy */
#include <sys/time.h>     /* timeval */
#include <unistd.h>       /* select */
#include <pobl/bl_file.h> /* bl_file_set_cloexec */
#endif

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>   /* realloc/free */
#include <pobl/bl_types.h> /* u_int */
#include <vt_term_manager.h>

#include "ui_display.h"
#include "ui_screen_manager.h"

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

#ifndef USE_WIN32API
static struct {
  int fd;
  void (*handler)(void);

} * additional_fds;
static u_int num_of_additional_fds;
#endif

/* --- static functions --- */

#ifdef USE_WIN32API

static VOID CALLBACK timer_proc(HWND hwnd, UINT msg, UINT timerid, DWORD time) {
  ui_display_t** displays;
  u_int num_of_displays;
  int count;

  displays = ui_get_opened_displays(&num_of_displays);

  for (count = 0; count < num_of_displays; count++) {
    ui_display_idling(displays[count]);
  }
}

#else /* USE_WIN32API */

static void receive_next_event(void) {
  u_int count;
  vt_term_t** terms;
  u_int num_of_terms;
  int xfd;
  int ptyfd;
  int maxfd;
  int ret;
  fd_set read_fds;
  struct timeval tval;
  ui_display_t** displays;
  u_int num_of_displays;
#ifdef USE_LIBSSH2
  int* xssh_fds;
  u_int num_of_xssh_fds;

  num_of_xssh_fds = vt_pty_ssh_get_x11_fds(&xssh_fds);
#endif
  num_of_terms = vt_get_all_terms(&terms);

  while (1) {
/* on Linux tv_usec,tv_sec members are zero cleared after select() */
#if defined(__NetBSD__) && defined(USE_FRAMEBUFFER)
    static int display_idling_wait = 4;

    tval.tv_usec = 25000; /* 0.025 sec */
#else
    tval.tv_usec = 100000; /* 0.1 sec */
#endif
    tval.tv_sec = 0;

#ifdef USE_LIBSSH2
    if (vt_pty_ssh_poll(&read_fds) > 0) {
      /*
       * Call vt_pty_ssh_send_recv_x11() and vt_term_parse_vt100_sequence()
       * instead of 'break' here because 'break' here suppresses
       * checking ui_display etc if use_local_echo option which
       * stops receive_bytes in vt_term_parse_vt100_sequence() is enabled.
       */

      for (count = num_of_xssh_fds; count > 0; count--) {
        vt_pty_ssh_send_recv_x11(
            count - 1, xssh_fds[count - 1] >= 0 && FD_ISSET(xssh_fds[count - 1], &read_fds));
      }

      for (count = 0; count < num_of_terms; count++) {
        ptyfd = vt_term_get_master_fd(terms[count]);
#ifdef OPEN_PTY_ASYNC
        if (ptyfd >= 0)
#endif
        {
          if (FD_ISSET(ptyfd, &read_fds)) {
            vt_term_parse_vt100_sequence(terms[count]);
          }
        }
      }
    }
#endif

    maxfd = 0;
    FD_ZERO(&read_fds);

#ifdef USE_LIBSSH2
    for (count = 0; count < num_of_xssh_fds; count++) {
      if (xssh_fds[count] >= 0) {
        FD_SET(xssh_fds[count], &read_fds);

        if (xssh_fds[count] > maxfd) {
          maxfd = xssh_fds[count];
        }
      }
    }
#endif

    displays = ui_get_opened_displays(&num_of_displays);

    for (count = 0; count < num_of_displays; count++) {
#if defined(USE_XLIB)
      /*
       * Need to read pending events and to flush events in
       * output buffer on X11 before waiting in select().
       */
      ui_display_sync(displays[count]);
#endif

      xfd = ui_display_fd(displays[count]);

      FD_SET(xfd, &read_fds);

      if (xfd > maxfd) {
        maxfd = xfd;
      }
    }

    for (count = 0; count < num_of_terms; count++) {
      ptyfd = vt_term_get_master_fd(terms[count]);
#ifdef OPEN_PTY_ASYNC
      if (ptyfd >= 0)
#endif
      {
        FD_SET(ptyfd, &read_fds);

        if (ptyfd > maxfd) {
          maxfd = ptyfd;
        }
      }
    }

    for (count = 0; count < num_of_additional_fds; count++) {
      if (additional_fds[count].fd >= 0) {
        FD_SET(additional_fds[count].fd, &read_fds);

        if (additional_fds[count].fd > maxfd) {
          maxfd = additional_fds[count].fd;
        }
      }
    }

    if ((ret = select(maxfd + 1, &read_fds, NULL, NULL, &tval)) != 0) {
      if (ret < 0) {
/* error happened */

#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " error happened in select.\n");
#endif

        return;
      }

      break;
    }

    for (count = 0; count < num_of_additional_fds; count++) {
      if (additional_fds[count].fd < 0) {
        (*additional_fds[count].handler)();
      }
    }

#if defined(__NetBSD__) && defined(USE_FRAMEBUFFER)
    /* ui_display_idling() is called every 0.1 sec. */
    if (--display_idling_wait > 0) {
      continue;
    }
    display_idling_wait = 4;
#endif

    for (count = 0; count < num_of_displays; count++) {
      ui_display_idling(displays[count]);
    }
  }

  /*
   * Processing order should be as follows.
   *
   * X Window -> PTY -> additional_fds
   */

  for (count = 0; count < num_of_displays; count++) {
    if (FD_ISSET(ui_display_fd(displays[count]), &read_fds)) {
      ui_display_receive_next_event(displays[count]);
    }
  }

#ifdef USE_LIBSSH2
  /*
   * vt_pty_ssh_send_recv_x11() should be called before
   * vt_term_parse_vt100_sequence() where xssh_fds can be deleted.
   */
  for (count = num_of_xssh_fds; count > 0; count--) {
    vt_pty_ssh_send_recv_x11(count - 1,
                             xssh_fds[count - 1] >= 0 && FD_ISSET(xssh_fds[count - 1], &read_fds));
  }
#endif

  for (count = 0; count < num_of_terms; count++) {
    ptyfd = vt_term_get_master_fd(terms[count]);
#ifdef OPEN_PTY_ASYNC
    if (ptyfd >= 0)
#endif
    {
      if (FD_ISSET(ptyfd, &read_fds)) {
        vt_term_parse_vt100_sequence(terms[count]);
      }
    }
  }

  for (count = 0; count < num_of_additional_fds; count++) {
    if (additional_fds[count].fd >= 0) {
      if (FD_ISSET(additional_fds[count].fd, &read_fds)) {
        (*additional_fds[count].handler)();

        break;
      }
    }
  }
}

#endif

/* --- global functions --- */

int ui_event_source_init(void) {
#ifdef USE_WIN32API
  /* ui_window_manager_idling() called in 0.1sec. */
  SetTimer(NULL, 0, 100, timer_proc);
#endif

  return 1;
}

int ui_event_source_final(void) {
#ifndef USE_WIN32API
  free(additional_fds);
#endif

  return 1;
}

int ui_event_source_process(void) {
#ifdef USE_WIN32API
  u_int num_of_displays;
  ui_display_t** displays;
  vt_term_t** terms;
  u_int num_of_terms;
  int* xssh_fds;
  u_int count;
#endif

#ifdef USE_WIN32API
  displays = ui_get_opened_displays(&num_of_displays);
  for (count = 0; count < num_of_displays; count++) {
    ui_display_receive_next_event(displays[count]);
  }
#else
  receive_next_event();
#endif

  vt_close_dead_terms();

#ifdef USE_WIN32API
/*
 * XXX
 * If pty is closed after vt_close_dead_terms() ...
 */

#ifdef USE_LIBSSH2
  for (count = vt_pty_ssh_get_x11_fds(&xssh_fds); count > 0; count--) {
    vt_pty_ssh_send_recv_x11(count - 1, 1);
  }
#endif

  num_of_terms = vt_get_all_terms(&terms);

  for (count = 0; count < num_of_terms; count++) {
    vt_term_parse_vt100_sequence(terms[count]);
  }
#endif

  ui_close_dead_screens();

  if (ui_get_all_screens(NULL) == 0) {
    return 0;
  }

  return 1;
}

/*
 * fd >= 0  -> Normal file descriptor. handler is invoked if fd is ready.
 * fd < 0 -> Special ID. handler is invoked at interval of 0.1 sec.
 */
int ui_event_source_add_fd(int fd, void (*handler)(void)) {
#ifndef USE_WIN32API

  void* p;

  if (!handler) {
    return 0;
  }

  if ((p = realloc(additional_fds, sizeof(*additional_fds) * (num_of_additional_fds + 1))) ==
      NULL) {
    return 0;
  }

  additional_fds = p;
  additional_fds[num_of_additional_fds].fd = fd;
  additional_fds[num_of_additional_fds++].handler = handler;
  if (fd >= 0) {
    bl_file_set_cloexec(fd);
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %d is added to additional fds.\n", fd);
#endif

  return 1;

#else /* USE_WIN32API */

  return 0;

#endif /* USE_WIN32API */
}

int ui_event_source_remove_fd(int fd) {
#ifndef USE_WIN32API
  u_int count;

  for (count = 0; count < num_of_additional_fds; count++) {
    if (additional_fds[count].fd == fd) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Additional fd %d is removed.\n", fd);
#endif

      additional_fds[count] = additional_fds[--num_of_additional_fds];

      return 1;
    }
  }
#endif /* USE_WIN32API */

  return 0;
}
