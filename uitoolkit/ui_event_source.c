/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

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

#ifdef USE_BEOS
void beos_lock(void);
void beos_unlock(void);
#endif

/* --- static variables --- */

#ifndef USE_WIN32API
static struct {
  int fd;
  void (*handler)(void);

} * additional_fds;
static u_int num_additional_fds;
#endif

/* --- static functions --- */

#ifndef NO_DISPLAY_FD

static void receive_next_event(void) {
  u_int count;
  vt_term_t **terms;
  u_int num_terms;
  int xfd;
  int ptyfd;
  int maxfd;
  int ret;
  fd_set read_fds;
  struct timeval tval;
  int is_sending_data = 0;
  ui_display_t **displays;
  u_int num_displays;
#ifdef USE_LIBSSH2
  int *xssh_fds;
  u_int num_xssh_fds;

  num_xssh_fds = vt_pty_ssh_get_x11_fds(&xssh_fds);
#endif
  num_terms = vt_get_all_terms(&terms);

#ifdef USE_BEOS
  beos_lock();
#endif

  while (1) {
/* on Linux tv_usec,tv_sec members are zero cleared after select() */
#ifdef KEY_REPEAT_BY_MYSELF
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

      for (count = num_xssh_fds; count > 0; count--) {
        vt_pty_ssh_send_recv_x11(
            count - 1, xssh_fds[count - 1] >= 0 && FD_ISSET(xssh_fds[count - 1], &read_fds));
      }

      for (count = 0; count < num_terms; count++) {
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

    maxfd = -1;
    FD_ZERO(&read_fds);

#ifdef USE_LIBSSH2
    for (count = 0; count < num_xssh_fds; count++) {
      if (xssh_fds[count] >= 0) {
        FD_SET(xssh_fds[count], &read_fds);

        if (xssh_fds[count] > maxfd) {
          maxfd = xssh_fds[count];
        }
      }
    }
#endif

    displays = ui_get_opened_displays(&num_displays);

    for (count = 0; count < num_displays; count++) {
#ifdef NEED_DISPLAY_SYNC_EVERY_TIME
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

    for (count = 0; count < num_terms; count++) {
      ptyfd = vt_term_get_master_fd(terms[count]);
#ifdef OPEN_PTY_ASYNC
      if (ptyfd >= 0)
#endif
      {
        FD_SET(ptyfd, &read_fds);

        if (ptyfd > maxfd) {
          maxfd = ptyfd;
        }

        if (vt_term_is_sending_data(terms[count])) {
          tval.tv_usec = 0;
          is_sending_data = 1;
          vt_term_parse_vt100_sequence(terms[count]);
        }
      }
    }

    for (count = 0; count < num_additional_fds; count++) {
      if (additional_fds[count].fd >= 0) {
        FD_SET(additional_fds[count].fd, &read_fds);

        if (additional_fds[count].fd > maxfd) {
          maxfd = additional_fds[count].fd;
        }
      }
    }

#ifdef USE_BEOS
    beos_unlock();
#endif

#ifdef __HAIKU__
    if (vt_check_sig_child()) {
      return;
    } else
#endif
    if ((ret = select(maxfd + 1, &read_fds, NULL, NULL, &tval)) != 0) {
      if (ret < 0) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " error happened in select.\n");
#endif

        return;
      }

      break;
    }

#ifdef USE_BEOS
    /* UI thread might create a new vt_term by pressing Ctrl+F1 key and so on. */
    num_terms = vt_get_all_terms(&terms);
    beos_lock();
#endif

    if (is_sending_data) {
      is_sending_data = 0;

      continue;
    }

#ifdef KEY_REPEAT_BY_MYSELF
    /* ui_display_idling() is called every 0.1 sec. */
    if (--display_idling_wait > 0) {
      goto additional_minus_fds;
    }
    display_idling_wait = 4;
#endif

    for (count = 0; count < num_displays; count++) {
      ui_display_idling(displays[count]);
    }

#ifdef KEY_REPEAT_BY_MYSELF
  additional_minus_fds:
#endif
    /*
     * additional_fds.handler (-> update_preedit_text -> cand_screen->destroy) of
     * ibus may destroy ui_display on wayland.
     */
    for (count = 0; count < num_additional_fds; count++) {
      if (additional_fds[count].fd < 0) {
        (*additional_fds[count].handler)();
      }
    }
  }

#ifdef USE_BEOS
  /* UI thread might create a new vt_term by pressing Ctrl+F1 key and so on. */
  num_terms = vt_get_all_terms(&terms);
  beos_lock();
#endif

  /*
   * Processing order should be as follows.
   *
   * X Window -> PTY -> additional_fds
   */

  for (count = 0; count < num_displays; count++) {
    if (FD_ISSET(ui_display_fd(displays[count]), &read_fds)) {
      ui_display_receive_next_event(displays[count]);
      /* XXX displays pointer may be changed (realloced) */
      displays = ui_get_opened_displays(&num_displays);
    }
  }

#ifdef USE_LIBSSH2
  /*
   * vt_pty_ssh_send_recv_x11() should be called before
   * vt_term_parse_vt100_sequence() where xssh_fds can be destroyed.
   */
  for (count = num_xssh_fds; count > 0; count--) {
    vt_pty_ssh_send_recv_x11(count - 1,
                             xssh_fds[count - 1] >= 0 && FD_ISSET(xssh_fds[count - 1], &read_fds));
  }
#endif

  for (count = 0; count < num_terms; count++) {
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

  for (count = 0; count < num_additional_fds; count++) {
    if (additional_fds[count].fd >= 0) {
      if (FD_ISSET(additional_fds[count].fd, &read_fds)) {
        (*additional_fds[count].handler)();
      }
    }
  }

#ifdef USE_BEOS
  beos_unlock();
#endif
}

#endif

/* --- global functions --- */

void ui_event_source_init(void) {
#ifdef USE_WIN32API
  vt_pty_ssh_set_pty_read_trigger(ui_display_trigger_pty_read);
  vt_pty_mosh_set_pty_read_trigger(ui_display_trigger_pty_read);
  vt_pty_win32_set_pty_read_trigger(ui_display_trigger_pty_read);
#endif
}

void ui_event_source_final(void) {
#ifndef NO_DISPLAY_FD
  free(additional_fds);
#endif
}

int ui_event_source_process(void) {
#ifdef NO_DISPLAY_FD
  u_int num_displays;
  ui_display_t **displays;
  vt_term_t **terms;
  u_int num_terms;
  int *xssh_fds;
  u_int count;

  displays = ui_get_opened_displays(&num_displays);
  for (count = 0; count < num_displays; count++) {
    ui_display_receive_next_event(displays[count]);
  }
#else
  receive_next_event();
#endif

  vt_close_dead_terms();

#ifdef NO_DISPLAY_FD
/*
 * XXX
 * If pty is closed after vt_close_dead_terms() ...
 */

#ifdef USE_LIBSSH2
  for (count = vt_pty_ssh_get_x11_fds(&xssh_fds); count > 0; count--) {
    vt_pty_ssh_send_recv_x11(count - 1, 1);
  }
#endif

  num_terms = vt_get_all_terms(&terms);

  for (count = 0; count < num_terms; count++) {
    vt_term_parse_vt100_sequence(terms[count]);
    if (vt_term_is_sending_data(terms[count])) {
      ui_display_trigger_pty_read();
    }
  }

#ifndef USE_WIN32API
  for (count = 0; count < num_additional_fds; count++) {
    (*additional_fds[count].handler)();
  }
#endif
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

  void *p;

  if (!handler) {
    return 0;
  }

  if ((p = realloc(additional_fds, sizeof(*additional_fds) * (num_additional_fds + 1))) ==
      NULL) {
    return 0;
  }

  additional_fds = p;
  additional_fds[num_additional_fds].fd = fd;
  additional_fds[num_additional_fds++].handler = handler;
  if (fd >= 0) {
    bl_file_set_cloexec(fd);
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %d is added to additional fds.\n", fd);
#endif

  return 1;

#else /* NO_DISPLAY_FD */

  return 0;

#endif /* NO_DISPLAY_FD */
}

void ui_event_source_remove_fd(int fd) {
#ifndef USE_WIN32API
  u_int count;

  for (count = 0; count < num_additional_fds; count++) {
    if (additional_fds[count].fd == fd) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Additional fd %d is removed.\n", fd);
#endif

      additional_fds[count] = additional_fds[--num_additional_fds];

      return;
    }
  }
#endif /* NO_DISPLAY_FD */
}
