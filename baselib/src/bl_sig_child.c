/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_sig_child.h"

#ifndef USE_WIN32API

#include <errno.h> /* EINTR */
#include <signal.h>
#include <sys/wait.h>

#endif

#include "bl_debug.h"
#include "bl_mem.h" /* realloc/free */

typedef struct sig_child_event_listener {
  void *self;
  void (*exited)(void *, pid_t);

} sig_child_event_listener_t;

/* --- static variables --- */

static sig_child_event_listener_t *listeners;
static u_int num_of_listeners;
static int is_init;

/* --- static functions --- */

#ifndef USE_WIN32API

static void sig_child(int sig) {
  pid_t pid;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " SIG CHILD received.\n");
#endif

  while (1) {
    if ((pid = waitpid(-1, NULL, WNOHANG)) <= 0) {
      if (pid == -1 && errno == EINTR) {
        errno = 0;
        continue;
      }

      break;
    }

    bl_trigger_sig_child(pid);
  }

  /* reset */
  signal(SIGCHLD, sig_child);
}

#endif

/* --- global functions --- */

int bl_sig_child_init(void) {
#ifndef USE_WIN32API
  signal(SIGCHLD, sig_child);
#endif

  is_init = 1;

  return 1;
}

int bl_sig_child_final(void) {
  if (listeners) {
    free(listeners);
  }

  is_init = 0;

  return 1;
}

int bl_sig_child_suspend(void) {
  if (is_init) {
#ifndef USE_WIN32API
    signal(SIGCHLD, SIG_DFL);
#endif
  }

  return 1;
}

int bl_sig_child_resume(void) {
  if (is_init) {
#ifndef USE_WIN32API
    signal(SIGCHLD, sig_child);
#endif
  }

  return 1;
}

int bl_add_sig_child_listener(void *self, void (*exited)(void *, pid_t)) {
  void *p;

/*
 * #if 0 - #endif is for mlterm-libvte.
 */
#if 0
  if (!is_init) {
    return 0;
  }
#endif

  if ((p = realloc(listeners, sizeof(*listeners) * (num_of_listeners + 1))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " realloc failed.\n");
#endif

    return 0;
  }

  listeners = p;

  listeners[num_of_listeners].self = self;
  listeners[num_of_listeners].exited = exited;

  num_of_listeners++;

  return 1;
}

int bl_remove_sig_child_listener(void *self, void (*exited)(void *, pid_t)) {
  u_int count;

  for (count = 0; count < num_of_listeners; count++) {
    if (listeners[count].self == self && listeners[count].exited == exited) {
      listeners[count] = listeners[--num_of_listeners];

      /*
       * memory area of listener is not shrunk.
       */

      return 1;
    }
  }

  return 0;
}

void bl_trigger_sig_child(pid_t pid) {
  u_int count;

  for (count = 0; count < num_of_listeners; count++) {
    (*listeners[count].exited)(listeners[count].self, pid);
  }
}
