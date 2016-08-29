/*
 *	$Id$
 */

#include "bl_pty.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h> /* memcpy */
#include <unistd.h>

#include "bl_def.h" /* HAVE_SETSID, LINE_MAX */
#include "bl_debug.h"
#include "bl_mem.h" /* realloc/free */

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef LIBEXECDIR
#define LIBEXECDIR "/usr/local/libexec"
#endif

#if 0
#define __DEBUG
#endif

typedef enum {
  GNOME_PTY_OPEN_PTY_UTMP = 1,
  GNOME_PTY_OPEN_PTY_UWTMP,
  GNOME_PTY_OPEN_PTY_WTMP,
  GNOME_PTY_OPEN_PTY_LASTLOG,
  GNOME_PTY_OPEN_PTY_LASTLOGUTMP,
  GNOME_PTY_OPEN_PTY_LASTLOGUWTMP,
  GNOME_PTY_OPEN_PTY_LASTLOGWTMP,
  GNOME_PTY_OPEN_NO_DB_UPDATE,
  GNOME_PTY_RESET_TO_DEFAULTS,
  GNOME_PTY_CLOSE_PTY,
  GNOME_PTY_SYNCH

} GnomePtyOps;

typedef struct {
  int pty;
  void* tag;

} pty_helper_tag_t;

/* --- static variables --- */

static pid_t myself = -1;
static pid_t pty_helper_pid = -1;
static int pty_helper_tunnel = -1;
static pty_helper_tag_t* pty_helper_tags = NULL;
static u_int num_of_pty_helper_tags;
static GnomePtyOps pty_helper_open_ops = GNOME_PTY_OPEN_PTY_UTMP;

/* --- static functions --- */

static void setup_child(int fd) {
  char* tty;

  tty = ttyname(fd);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Setting up child pty(name:%s, fd:%d)\n", tty ? tty : "(none)", fd);
#endif

  /* Try to reopen the pty to acquire it as our controlling terminal. */
  if (tty != NULL) {
    int _fd;

    if ((_fd = open(tty, O_RDWR)) != -1) {
      if (fd != -1) {
        close(fd);
      }

      fd = _fd;
    }
  }

  if (fd == -1) {
    exit(EXIT_FAILURE);
  }

/* Start a new session and become process group leader. */
#if defined(HAVE_SETSID) && defined(HAVE_SETPGID)
  setsid();
  setpgid(0, 0);
#endif

#ifdef TIOCSCTTY
  ioctl(fd, TIOCSCTTY, fd);
#endif

#if defined(HAVE_ISASTREAM) && defined(I_PUSH)
  if (isastream(fd) == 1) {
    ioctl(fd, I_PUSH, "ptem");
    ioctl(fd, I_PUSH, "ldterm");
    ioctl(fd, I_PUSH, "ttcompat");
  }
#endif

  if (fd != STDIN_FILENO) {
    dup2(fd, STDIN_FILENO);
  }

  if (fd != STDOUT_FILENO) {
    dup2(fd, STDOUT_FILENO);
  }

  if (fd != STDERR_FILENO) {
    dup2(fd, STDERR_FILENO);
  }

  if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
    close(fd);
  }

  close(pty_helper_tunnel);
}

#ifdef HAVE_RECVMSG
static void read_ptypair(int tunnel, int* master, int* slave) {
  int count;
  int ret;
  char control[LINE_MAX];
  char iobuf[LINE_MAX];
  struct cmsghdr* cmsg;
  struct msghdr msg;
  struct iovec vec;

  for (count = 0; count < 2; count++) {
    vec.iov_base = iobuf;
    vec.iov_len = sizeof(iobuf);
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    if ((ret = recvmsg(tunnel, &msg, MSG_NOSIGNAL)) == -1) {
      return;
    }

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      if (cmsg->cmsg_type == SCM_RIGHTS) {
        memcpy(&ret, CMSG_DATA(cmsg), sizeof(ret));

        if (count == 0) {
          /* Without this, pty master is blocked in poll. */
          fcntl(ret, F_SETFL, O_NONBLOCK);
          *master = ret;
        } else /* if( i == 1) */
        {
          fcntl(ret, F_SETFL, O_NONBLOCK);
          *slave = ret;
        }
      }
    }
  }
}
#elif defined(I_RECVFD)
static void read_ptypair(int tunnel, int* master, int* slave) {
  int ret;

  if (ioctl(tunnel, I_RECVFD, &ret) == -1) {
    return;
  }

  *master = ret;

  if (ioctl(tunnel, I_RECVFD, &ret) == -1) {
    return;
  }

  *slave = ret;
}
#endif

#ifdef HAVE_SOCKETPAIR
static int open_pipe(int* a, int* b) {
  int p[2];
  int ret = -1;

#ifdef PF_UNIX
#ifdef SOCK_STREAM
  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, p);
#else
#ifdef SOCK_DGRAM
  ret = socketpair(PF_UNIX, SOCK_DGRAM, 0, p);
#endif
#endif

  if (ret == 0) {
    *a = p[0];
    *b = p[1];

    return 0;
  }
#endif

  return ret;
}
#else
static int open_pipe(int* a, int* b) {
  int p[2];
  int ret = -1;

  ret = pipe(p);

  if (ret == 0) {
    *a = p[0];
    *b = p[1];
  }

  return ret;
}
#endif

/* read ignoring EINTR and EAGAIN. */
static ssize_t n_read(int fd, void* buffer, size_t buf_size) {
  size_t n;
  char* p;
  int ret;

  n = 0;
  p = buffer;

  while (n < buf_size) {
    ret = read(fd, p + n, buf_size - n);
    switch (ret) {
      case 0:
        return n;

      case -1:
        switch (errno) {
          case EINTR:
          case EAGAIN:
#ifdef ERESTART
          case ERESTART:
#endif
            break;

          default:
            return -1;
        }

      default:
        n += ret;
    }
  }

  return n;
}

/* write ignoring EINTR and EAGAIN. */
static ssize_t n_write(int fd, const void* buffer, size_t buf_size) {
  size_t n;
  const char* p;
  int ret;

  n = 0;
  p = buffer;

  while (n < buf_size) {
    ret = write(fd, p + n, buf_size - n);
    switch (ret) {
      case 0:
        return n;

      case -1:
        switch (errno) {
          case EINTR:
          case EAGAIN:
#ifdef ERESTART
          case ERESTART:
#endif
            break;

          default:
            return -1;
        }

      default:
        n += ret;
    }
  }

  return n;
}

static void stop_pty_helper(void) {
  if (pty_helper_pid != -1) {
    free(pty_helper_tags);
    pty_helper_tags = NULL;

    num_of_pty_helper_tags = 0;

    close(pty_helper_tunnel);
    pty_helper_tunnel = -1;

    /* child processes might trigger this function on exit(). */
    if (myself == getpid()) {
      kill(pty_helper_pid, SIGTERM);
    }

    pty_helper_pid = -1;
  }
}

static int start_pty_helper(void) {
  int tmp[2];
  int tunnel;

  if (access(LIBEXECDIR "/mlterm/gnome-pty-helper", X_OK) != 0) {
    bl_error_printf("Couldn't run %s", LIBEXECDIR "/mlterm/gnome-pty-helper");

    return 0;
  }

  /* Create a communication link with the helper. */
  tmp[0] = open("/dev/null", O_RDONLY);
  if (tmp[0] == -1) {
    return 0;
  }

  tmp[1] = open("/dev/null", O_RDONLY);
  if (tmp[1] == -1) {
    close(tmp[0]);

    return 0;
  }

  if (open_pipe(&pty_helper_tunnel, &tunnel) != 0) {
    return 0;
  }

  close(tmp[0]);
  close(tmp[1]);

  pty_helper_pid = fork();
  if (pty_helper_pid == -1) {
    return 0;
  }

  if (pty_helper_pid == 0) {
    /* Child */

    int count;

    /* No need to close all descriptors because gnome-pty-helper does that
     * anyway. */
    for (count = 0; count < 3; count++) {
      close(count);
    }

    dup2(tunnel, STDIN_FILENO);
    dup2(tunnel, STDOUT_FILENO);
    close(tunnel);
    close(pty_helper_tunnel);

    execl(LIBEXECDIR "/mlterm/gnome-pty-helper", "gnome-pty-helper", NULL);

    exit(EXIT_SUCCESS);
  }

  close(tunnel);

  myself = getpid();
  atexit(stop_pty_helper);

  return 1;
}

/* --- global functions --- */

pid_t bl_pty_fork(int* master, int* slave) {
  pid_t pid;
  int ret;
  void* tag;

  if (pty_helper_pid == -1) {
    if (!start_pty_helper()) {
      return -1;
    }
  }

  /* Send our request. */
  if (n_write(pty_helper_tunnel, &pty_helper_open_ops, sizeof(pty_helper_open_ops)) !=
      sizeof(pty_helper_open_ops)) {
    return -1;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Sent request to helper.\n");
#endif

  /* Read back the response. */
  if (n_read(pty_helper_tunnel, &ret, sizeof(ret)) != sizeof(ret)) {
    return -1;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Received response from helper.\n");
#endif

  if (ret == 0) {
    return -1;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Helper returns success.\n");
#endif

  /* Read back a tag. */
  if (n_read(pty_helper_tunnel, &tag, sizeof(tag)) != sizeof(tag)) {
    return -1;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Tag = %p.\n", tag);
#endif

  /* Receive the master and slave ptys. */
  read_ptypair(pty_helper_tunnel, master, slave);

  if ((*master == -1) || (*slave == -1)) {
    close(*master);
    close(*slave);
    return -1;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Master pty %d / Slave pty %d.\n", *master, *slave);
#endif

  pty_helper_tags =
      realloc(pty_helper_tags, sizeof(pty_helper_tag_t) * (num_of_pty_helper_tags + 1));
  pty_helper_tags[num_of_pty_helper_tags].pty = *master;
  pty_helper_tags[num_of_pty_helper_tags++].tag = tag;

  pid = fork();
  if (pid == -1) {
    /* Error */

    bl_error_printf("Failed to fork.\n");

    close(*master);
    close(*slave);
  } else if (pid == 0) {
    /* child */

    close(*master);

    setup_child(*slave);
  }

  return pid;
}

int bl_pty_close(int master) {
  u_int count;

  for (count = 0; count < num_of_pty_helper_tags; count++) {
    if (pty_helper_tags[count].pty == master) {
      void* tag;
      GnomePtyOps ops;

      tag = pty_helper_tags[count].tag;
      ops = GNOME_PTY_CLOSE_PTY;

      if (n_write(pty_helper_tunnel, &ops, sizeof(ops)) != sizeof(ops) ||
          n_write(pty_helper_tunnel, &tag, sizeof(tag)) != sizeof(tag)) {
        return 0;
      }

      ops = GNOME_PTY_SYNCH;

      if (n_write(pty_helper_tunnel, &ops, sizeof(ops)) != sizeof(ops)) {
        return 0;
      }

#if 0
      /* This can be blocked (CentOS 5, vte 0.14.0) */
      n_read(pty_helper_tunnel, &ops, 1);
#endif

      pty_helper_tags[count] = pty_helper_tags[--num_of_pty_helper_tags];

      return 1;
    }
  }

  close(master);

  return 0;
}

void bl_pty_helper_set_flag(int lastlog, int utmp, int wtmp) {
  int idx;

  GnomePtyOps ops[8] = {
      GNOME_PTY_OPEN_NO_DB_UPDATE,     /* 0 0 0 */
      GNOME_PTY_OPEN_PTY_LASTLOG,      /* 0 0 1 */
      GNOME_PTY_OPEN_PTY_UTMP,         /* 0 1 0 */
      GNOME_PTY_OPEN_PTY_LASTLOGUTMP,  /* 0 1 1 */
      GNOME_PTY_OPEN_PTY_WTMP,         /* 1 0 0 */
      GNOME_PTY_OPEN_PTY_LASTLOGWTMP,  /* 1 0 1 */
      GNOME_PTY_OPEN_PTY_UWTMP,        /* 1 1 0 */
      GNOME_PTY_OPEN_PTY_LASTLOGUWTMP, /* 1 1 1 */
  };

  idx = 0;

  if (lastlog) {
    idx += 1;
  }

  if (utmp) {
    idx += 2;
  }

  if (wtmp) {
    idx += 4;
  }

  pty_helper_open_ops = ops[idx];
}
