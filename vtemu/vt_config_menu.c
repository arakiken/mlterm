/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef NO_TOOLS

#include "vt_config_menu.h"

#ifdef USE_WIN32API
#include <windows.h>
#endif

#include <stdio.h>  /* sprintf */
#include <string.h> /* strchr */
#include <unistd.h> /* fork */
#include <pobl/bl_sig_child.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h> /* DIGIT_STR_LEN */
#include <pobl/bl_mem.h>  /* malloc */
#include <pobl/bl_file.h> /* bl_file_set_cloexec, bl_file_unset_cloexec */
#include <pobl/bl_def.h>  /* HAVE_WINDOWS_H */
#include <pobl/bl_path.h> /* BL_LIBEXECDIR */

/* --- static functions --- */

#ifdef USE_WIN32API

static DWORD WINAPI wait_child_exited(LPVOID thr_param) {
  vt_config_menu_t *config_menu;
  DWORD ev;

#if 0
  bl_debug_printf("wait_child_exited thread.\n");
#endif

  config_menu = thr_param;

  while (1) {
    ev = WaitForSingleObject(config_menu->pid, INFINITE);

#if 0
    bl_debug_printf("WaitForMultipleObjects %dth event signaled.\n", ev);
#endif

    if (ev == WAIT_OBJECT_0) {
      CloseHandle(config_menu->fd);
      CloseHandle(config_menu->pid);
      config_menu->fd = 0;
      config_menu->pid = 0;

#ifdef USE_LIBSSH2
      vt_pty_set_use_loopback(config_menu->pty, 0);
      config_menu->pty = NULL;
#endif

      break;
    }
  }

  ExitThread(0);

  return 0;
}

#else

static void sig_child(void *self, pid_t pid) {
  vt_config_menu_t *config_menu;

  config_menu = self;

  if (config_menu->pid == pid) {
    config_menu->pid = 0;

    close(config_menu->fd);
    config_menu->fd = -1;

#ifdef USE_LIBSSH2
    if (config_menu->pty) {
      vt_pty_set_use_loopback(config_menu->pty, 0);
      config_menu->pty = NULL;
    }
#endif
  }
}

#endif

/* --- global functions --- */

int vt_config_menu_init(vt_config_menu_t *config_menu) {
  config_menu->pid = 0;
#ifdef USE_WIN32API
  config_menu->fd = 0;
#else
  config_menu->fd = -1;

  bl_add_sig_child_listener(config_menu, sig_child);
#endif
#ifdef USE_LIBSSH2
  config_menu->pty = NULL;
#endif

  return 1;
}

int vt_config_menu_final(vt_config_menu_t *config_menu) {
#ifndef USE_WIN32API
  bl_remove_sig_child_listener(config_menu, sig_child);
#endif

  return 1;
}

int vt_config_menu_start(vt_config_menu_t *config_menu, char *cmd_path, int x, int y, char *display,
                         vt_pty_ptr_t pty) {
#ifdef USE_WIN32API

  HANDLE input_write_tmp;
  HANDLE input_read;
  HANDLE output_write;
  HANDLE error_write;
  SECURITY_ATTRIBUTES sa;
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
  char *cmd_line;
  char geometry[] = "--geometry";
  DWORD tid;
  int pty_fd;

  if (config_menu->pid > 0) {
    /* configuration menu is active now */

    return 0;
  }

  input_read = output_write = error_write = 0;

  if ((pty_fd = vt_pty_get_slave_fd(pty)) == -1) {
#ifdef USE_LIBSSH2
    if (vt_pty_set_use_loopback(pty, 1)) {
      pty_fd = vt_pty_get_slave_fd(pty);
      config_menu->pty = pty;
    } else
#endif
    {
      return 0;
    }
  }

  /*
   * pty_fd is not inheritable(see vt_pty_pipewin32.c), so it is necessary
   * to duplicate inheritable handle.
   * It is possible to DuplicateHandle(socket) if pty_fd is socket.
   */
  if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)pty_fd, GetCurrentProcess(), &output_write, 0,
                       TRUE, DUPLICATE_SAME_ACCESS)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " DuplicateHandle() failed.\n");
#endif

    goto error1;
  }

  if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)pty_fd, GetCurrentProcess(), &error_write, 0,
                       TRUE, DUPLICATE_SAME_ACCESS)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " DuplicateHandle() failed.\n");
#endif

    goto error1;
  }

  /* Set up the security attributes struct. */
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;

  /* Create the child input pipe. */
  if (!CreatePipe(&input_read, &input_write_tmp, &sa, 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreatePipe() failed.\n");
#endif

    goto error1;
  }

  if (!DuplicateHandle(GetCurrentProcess(), input_write_tmp, GetCurrentProcess(),
                       &config_menu->fd, /* Address of new handle. */
                       0, FALSE,         /* Make it uninheritable. */
                       DUPLICATE_SAME_ACCESS)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " DuplicateHandle() failed.\n");
#endif

    CloseHandle(input_write_tmp);

    goto error1;
  }

  /*
   * Close inheritable copies of the handles you do not want to be
   * inherited.
   */
  CloseHandle(input_write_tmp);

  /* Set up the start up info struct. */
  ZeroMemory(&si, sizeof(STARTUPINFO));
  si.cb = sizeof(STARTUPINFO);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
  si.hStdOutput = output_write;
  si.hStdInput = input_read;
  si.hStdError = error_write;

  /*
   * Use this if you want to hide the child:
   *  si.wShowWindow = SW_HIDE;
   * Note that dwFlags must include STARTF_USESHOWWINDOW if you want to
   * use the wShowWindow flags.
   */

  if ((cmd_line = alloca(strlen(cmd_path) + 1 + sizeof(geometry) + (1 + DIGIT_STR_LEN(int)) * 2 +
                         1)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " alloca failed.\n");
#endif

    goto error1;
  }

  sprintf(cmd_line, "%s %s +%d+%d", cmd_path, geometry, x, y);

  if (!CreateProcess(cmd_path, cmd_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si,
                     &pi)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreateProcess() failed.\n");
#endif

    goto error1;
  }

  /* Set global child process handle to cause threads to exit. */
  config_menu->pid = pi.hProcess;

  /* Close any unnecessary handles. */
  CloseHandle(pi.hThread);

  /*
   * Close pipe handles (do not continue to modify the parent).
   * You need to make sure that no handles to the write end of the
   * output pipe are maintained in this process or else the pipe will
   * not close when the child process exits and the ReadFile will hang.
   */
  CloseHandle(output_write);
  CloseHandle(input_read);
  CloseHandle(error_write);

  if (!CreateThread(NULL, 0, wait_child_exited, config_menu, 0, &tid)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreateThread() failed.\n");
#endif

    goto error2;
  }

  return 1;

error1:
  if (input_read) {
    CloseHandle(input_read);
  }

  if (output_write) {
    CloseHandle(output_write);
  }

  if (error_write) {
    CloseHandle(error_write);
  }

error2:
  if (config_menu->pid) {
    /*
     * TerminateProcess must be called before CloseHandle(fd).
     */
    TerminateProcess(config_menu->pid, 0);
    config_menu->pid = 0;
  }

  if (config_menu->fd) {
    CloseHandle(config_menu->fd);
    config_menu->fd = 0;
  }

#ifdef USE_LIBSSH2
  if (config_menu->pty) {
    vt_pty_set_use_loopback(config_menu->pty, 0);
    config_menu->pty = NULL;
  }
#endif

  return 0;

#else /* USE_WIN32API */

  pid_t pid;
  int fds[2];
  int pty_fd;

  if (config_menu->pid > 0) {
    /* configuration menu is active now */

    return 0;
  }

  if ((pty_fd = vt_pty_get_slave_fd(pty)) == -1) {
#ifdef USE_LIBSSH2
    if (vt_pty_set_use_loopback(pty, 1)) {
      pty_fd = vt_pty_get_slave_fd(pty);
      config_menu->pty = pty;
    } else
#endif
    {
      return 0;
    }
  }

  if (!bl_file_unset_cloexec(pty_fd)) {
    /* configulators require an inherited pty. */
    return 0;
  }

  if (pipe(fds) == -1) {
    return 0;
  }

  pid = fork();
  if (pid == -1) {
    return 0;
  }

  if (pid == 0) {
    /* child process */

    char *args[6];
    char geom[2 + DIGIT_STR_LEN(int)*2 + 1];

    args[0] = cmd_path;

    sprintf(geom, "+%d+%d", x, y);
    args[1] = "--geometry";
    args[2] = geom;

    if (display) {
      args[3] = "--display";
      args[4] = display;
      args[5] = NULL;
    } else {
      args[3] = NULL;
    }

    close(fds[1]);

    /* for configulators,
     * STDIN => to read replys from mlterm
     * STDOUT => to write the "master" side of pty
     * STDERR => is retained to be the mlterm's STDERR
     */
    if (dup2(fds[0], STDIN_FILENO) == -1 || dup2(pty_fd, STDOUT_FILENO) == -1) {
      bl_msg_printf("dup2 failed.\n");

      exit(1);
    }

#if (defined(__CYGWIN__) || defined(__MSYS__)) && !defined(DEBUG)
    /* Suppress error message */
    close(STDERR_FILENO);
#endif

    execv(cmd_path, args);

    /* failed */

    /* If program name was specified without directory, prepend LIBEXECDIR to it. */
    if (strchr(cmd_path, '/') == NULL) {
      char *p;
      char dir[] = BL_LIBEXECDIR("mlterm");

      if ((p = alloca(sizeof(dir) + strlen(cmd_path) + 1))) {
        sprintf(p, "%s/%s", dir, cmd_path);

        args[0] = cmd_path = p;

        execv(cmd_path, args);
      }
    }

    bl_error_printf("Failed to exec %s.\n", cmd_path);

    exit(1);
  }

  /* parent process */

  close(fds[0]);

  config_menu->fd = fds[1];
  config_menu->pid = pid;

  bl_file_set_cloexec(pty_fd);
  bl_file_set_cloexec(config_menu->fd);

  return 1;

#endif /* USE_WIN32API */
}

int vt_config_menu_write(vt_config_menu_t *config_menu, u_char *buf, size_t len) {
  ssize_t write_len;

#ifdef USE_WIN32API
  if (config_menu->fd == 0 || !WriteFile(config_menu->fd, buf, len, &write_len, NULL))
#else
  if (config_menu->fd == -1 || (write_len = write(config_menu->fd, buf, len)) == -1)
#endif
  {
    return 0;
  } else {
    return write_len;
  }
}

#endif /* NO_TOOLS */
