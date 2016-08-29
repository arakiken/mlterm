/*
 *	$Id$
 */

#include "daemon.h"

#ifndef USE_WIN32API

#include <stdio.h>
#include <string.h> /* memset/memcpy */
#include <unistd.h> /* setsid/unlink */
#include <signal.h> /* SIGHUP/kill */
#include <errno.h>
#include <sys/stat.h> /* umask */

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* alloca/malloc/free */
#include <pobl/bl_conf_io.h>
#include <pobl/bl_net.h> /* socket/bind/listen/sockaddr_un */

#include <ui_screen_manager.h>
#include <ui_event_source.h>

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static int sock_fd = -1;
static char *un_file;

/* --- static functions --- */

#ifdef USE_CONSOLE
static void client_connected(void) {
  struct sockaddr_un addr;
  socklen_t sock_len;
  int fd;
  char ch;
  char cmd[1024];
  size_t cmd_len;
  int dopt_added = 0;
  u_int num;
  ui_display_t **displays;

  sock_len = sizeof(addr);

  if ((fd = accept(sock_fd, (struct sockaddr *)&addr, &sock_len)) < 0) {
    return;
  }

  for (cmd_len = 0; cmd_len < sizeof(cmd) - 1; cmd_len++) {
    if (read(fd, &ch, 1) <= 0) {
      close(fd);

      return;
    }

    if ((ch == ' ' || ch == '\n') && !dopt_added) {
      if (cmd_len + 11 + DIGIT_STR_LEN(fd) + 1 > sizeof(cmd)) {
        close(fd);

        return;
      }

      sprintf(cmd + cmd_len, " -d client:%d", fd);
      cmd_len += strlen(cmd + cmd_len);
      dopt_added = 1;
    }

    if (ch == '\n') {
      break;
    } else {
      cmd[cmd_len] = ch;
    }
  }
  cmd[cmd_len] = '\0';

  ui_mlclient(cmd, stdout);

  displays = ui_get_opened_displays(&num);

  if (num == 0 || fileno(displays[num - 1]->display->fp) != fd) {
    close(fd);
  }
}
#else
static void client_connected(void) {
  struct sockaddr_un addr;
  socklen_t sock_len;
  int fd;
  FILE *fp;
  bl_file_t *from;
  char *line;
  size_t line_len;
  char *args;

  fp = NULL;

  sock_len = sizeof(addr);

  if ((fd = accept(sock_fd, (struct sockaddr *)&addr, &sock_len)) < 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " accept failed.\n");
#endif

    return;
  }

  /*
   * Set the close-on-exec flag.
   * If this flag off, this fd remained open until the child process forked in
   * open_screen_intern()(vt_term_open_pty()) close it.
   */
  bl_file_set_cloexec(fd);

  if ((fp = fdopen(fd, "r+")) == NULL) {
    goto crit_error;
  }

  if ((from = bl_file_new(fp)) == NULL) {
    goto error;
  }

  if ((line = bl_file_get_line(from, &line_len)) == NULL) {
    bl_file_delete(from);

    goto error;
  }

  if ((args = alloca(line_len)) == NULL) {
    bl_file_delete(from);

    goto error;
  }

  strncpy(args, line, line_len - 1);
  args[line_len - 1] = '\0';

  bl_file_delete(from);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s\n", args);
#endif

  if (strstr(args, "--kill")) {
    daemon_final();
    fprintf(fp, "bye\n");
    exit(0);
  } else if (!ui_mlclient(args, fp)) {
    goto error;
  }

  fclose(fp);

  return;

error : {
  char msg[] = "Error happened.\n";

  write(fd, msg, sizeof(msg) - 1);
}

crit_error:
  if (fp) {
    fclose(fp);
  } else {
    close(fd);
  }
}
#endif

/* --- global functions --- */

int daemon_init(void) {
  pid_t pid;
  struct sockaddr_un servaddr;
  char *path;

#ifdef USE_CONSOLE
  if ((path = bl_get_user_rc_path("mlterm/socket-con")) == NULL)
#else
  if ((path = bl_get_user_rc_path("mlterm/socket")) == NULL)
#endif
  {
    return 0;
  }

  if (strlen(path) >= sizeof(servaddr.sun_path) || !bl_mkdir_for_file(path, 0700)) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Failed mkdir for %s\n", path);
#endif
    free(path);

    return 0;
  }

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, path);
  free(path);
  path = servaddr.sun_path;

  if ((sock_fd = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " socket failed\n");
#endif

    return 0;
  }

  for (;;) {
    int ret;
    int saved_errno;
    mode_t mode;

    mode = umask(077);
    ret = bind(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    saved_errno = errno;
    umask(mode);

    if (ret == 0) {
      break;
    } else if (saved_errno == EADDRINUSE) {
      if (connect(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) {
        close(sock_fd);

        bl_msg_printf("daemon is already running.\n");

        return -1;
      }

      bl_msg_printf("removing stale lock file %s.\n", path);

      if (unlink(path) == 0) {
        continue;
      }
    } else {
      bl_msg_printf("failed to lock file %s: %s\n", path, strerror(saved_errno));

      goto error;
    }
  }

  pid = fork();

  if (pid == -1) {
    goto error;
  }

  if (pid != 0) {
    exit(0);
  }

  /*
   * child
   */

  /*
   * This process becomes a session leader and purged from control terminal.
   */
  setsid();

  /*
   * SIGHUP signal when the child process exits must not be sent to
   * the grandchild process.
   */
  signal(SIGHUP, SIG_IGN);

  pid = fork();

  if (pid != 0) {
    exit(0);
  }

  /*
   * grandchild
   */

  if (listen(sock_fd, 1024) < 0) {
    goto error;
  }

  bl_file_set_cloexec(sock_fd);

#ifndef DEBUG
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
#endif

  /* Mark started as daemon. */
  un_file = strdup(path);

  ui_event_source_add_fd(sock_fd, client_connected);

  return 1;

error:
  close(sock_fd);

  return 0;
}

int daemon_final(void) {
  if (un_file) {
    close(sock_fd);
    unlink(un_file);
    free(un_file);
  }

  return 1;
}

#endif /* USE_WIN32API */
