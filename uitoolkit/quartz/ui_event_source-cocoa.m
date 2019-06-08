/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_event_source.h"

#ifdef COCOA_TOUCH
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

#include <sys/select.h>
#include <errno.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_file.h>
#include <vt_term_manager.h>
#include "../ui_display.h"

/* --- static variables --- */

static struct {
  int fd;
  void (*handler)(void);

} * additional_fds;

static u_int num_additional_fds;

/* --- global functions --- */

void ui_event_source_init(void) {}

void ui_event_source_final(void) {}

int ui_event_source_process(void) {
  static int ret = -1;
  static fd_set read_fds;
  static int maxfd;
  static int is_cont_read;
  static struct timeval tval;

  for (;;) {
    tval.tv_usec = 100000; /* 0.1 sec */
    tval.tv_sec = 0;

    dispatch_sync(dispatch_get_main_queue(), ^{
        vt_close_dead_terms();

        vt_term_t **terms;
        u_int num_terms = vt_get_all_terms(&terms);

        int count;
        int ptyfd;
        if (ret > 0) {
          for (count = 0; count < num_additional_fds; count++) {
            if (additional_fds[count].fd < 0) {
              (*additional_fds[count].handler)();
            }
          }

          for (count = 0; count < num_terms; count++) {
            if ((ptyfd = vt_term_get_master_fd(terms[count])) >= 0 &&
                FD_ISSET(ptyfd, &read_fds)) {
              vt_term_parse_vt100_sequence(terms[count]);
            }
          }

          if (++is_cont_read >= 2) {
            [[NSRunLoop currentRunLoop]
             runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
          }
        } else if (ret == 0) {
          ui_display_idling(NULL);
          is_cont_read = 0;
        }

        FD_ZERO(&read_fds);
        maxfd = -1;

        for (count = 0; count < num_terms; count++) {
          if ((ptyfd = vt_term_get_master_fd(terms[count])) >= 0) {
            FD_SET(ptyfd, &read_fds);

            if (ptyfd > maxfd) {
              maxfd = ptyfd;
            }

            if (vt_term_is_sending_data(terms[count])) {
              tval.tv_usec = 0;
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
      });

    if (maxfd == -1 ||
        ((ret = select(maxfd + 1, &read_fds, NULL, NULL, &tval)) < 0 &&
         errno != EAGAIN && errno != EINTR)) {
      break;
    }
  }

  return 1;
}

/*
 * fd >= 0  -> Normal file descriptor. handler is invoked if fd is ready.
 * fd < 0 -> Special ID. handler is invoked at interval of 0.1 sec.
 */
int ui_event_source_add_fd(int fd, void (*handler)(void)) {
  void *p;

  if (!handler) {
    return 0;
  }

  if ((p = realloc(additional_fds, sizeof(*additional_fds) *
                                       (num_additional_fds + 1))) == NULL) {
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
}

void ui_event_source_remove_fd(int fd) {
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
}
