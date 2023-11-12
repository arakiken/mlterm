/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifdef USE_CONPTY
#if !defined(WINVER) || WINVER < _WIN32_WINNT_WIN8 /* 0x602 */
#define WINVER _WIN32_WINNT_WIN8
#endif
#if !defined(NTDDI_VERSION) || NTDDI_VERSION < NTDDI_WIN10_RS5
#define NTDDI_VERSION NTDDI_WIN10_RS5
#endif
#endif

#include "vt_pty_intern.h"

#include <windows.h>
#include <stdio.h>
#include <unistd.h> /* close */
#include <string.h> /* strchr/memcpy */
#include <stdlib.h> /* putenv/alloca */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* realloc/alloca */
#include <pobl/bl_pty.h>
#include <pobl/bl_sig_child.h>
#include <pobl/bl_path.h>

#if 0
#define __DEBUG
#endif

#if 0
#define USE_OPEN_OSFHANDLE
#endif

typedef struct vt_pty_win32 {
  vt_pty_t pty;

  HANDLE master_input;  /* master read(stdout,stderr) */
  HANDLE master_output; /* master write */
  HANDLE slave_stdout;  /* slave write */
  HANDLE child_proc;

  u_char rd_ch;
  HANDLE rd_ev;
  int8_t rd_ready;

  int8_t is_plink;
#ifdef USE_CONPTY
  HPCON hpc;
  COORD coord;
  PPROC_THREAD_ATTRIBUTE_LIST attr_list;
#endif

} vt_pty_win32_t;

/* --- static variables --- */

static HANDLE* child_procs; /* Notice: The first element is "ADDED_CHILD" event */
static DWORD num_child_procs;

static void (*trigger_pty_read)(void);

#ifdef USE_CONPTY
static int use_conpty = 1;
#endif

/* --- static functions --- */

static DWORD WINAPI wait_child_exited(LPVOID thr_param) {
  DWORD ev;

#ifdef __DEBUG
  bl_debug_printf("Starting wait_child_exited thread.\n");
#endif

  while (1) {
    ev = WaitForMultipleObjects(num_child_procs, child_procs, FALSE, INFINITE);

    if (ev > WAIT_OBJECT_0 && ev < WAIT_OBJECT_0 + num_child_procs) {
#ifdef __DEBUG
      bl_debug_printf("%dth child exited.\n", ev);
#endif

      /* XXX regarding pid_t as HANDLE */
      bl_trigger_sig_child(child_procs[ev - WAIT_OBJECT_0]);

      CloseHandle(child_procs[ev - WAIT_OBJECT_0]);

      child_procs[ev - WAIT_OBJECT_0] = child_procs[--num_child_procs];
    }

    if (num_child_procs == 1) {
      break;
    }
  }

  free(child_procs);
  num_child_procs = 0;
  child_procs = NULL;

#ifdef __DEBUG
  bl_debug_printf("Exiting wait_child_exited thread.\n");
#endif

  ExitThread(0);

  return 0;
}

/*
 * Monitors handle for input. Exits when child exits or pipe is broken.
 */
static DWORD WINAPI wait_pty_read(LPVOID thr_param) {
  vt_pty_win32_t *pty = (vt_pty_win32_t*)thr_param;
  DWORD n_rd;

#ifdef __DEBUG
  bl_debug_printf("Starting wait_pty_read thread.\n");
#endif

  while (1) {
    if (!ReadFile(pty->master_input, &pty->rd_ch, 1, &n_rd, NULL) || n_rd == 0) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " ReadFile() failed.");
#endif

      if (GetLastError() == ERROR_BROKEN_PIPE) {
/*
 * If slave_stdout member is not necessary, wait_child_exited
 * is called here and wait_child_exited thread becomes
 * unnecessary. (But master_input is never broken until
 * slave_stdout is closed even if child process exited, so
 * wait_child_exited thread is necessary.)
 */
#ifdef DEBUG
        bl_msg_printf(" ==> ERROR_BROKEN_PIPE.");
#endif
      }

#ifdef DEBUG
      bl_msg_printf("\n");
#endif

      break;
    }

    /* Exit GetMessage() in x_display_receive_next_event(). */
    (*trigger_pty_read)();

    WaitForSingleObject(pty->rd_ev, INFINITE);

    if (!pty->child_proc) {
      break;
    }

#ifdef __DEBUG
    bl_debug_printf("Exit WaitForSingleObject\n");
#endif
  }

#ifdef __DEBUG
  bl_debug_printf("Exiting wait_pty_read thread.\n");
#endif

  ExitThread(0);

  return 0;
}

#ifdef USE_CONPTY
static int conpty_open(vt_pty_win32_t *pty, const char *cmd_path, char *const cmd_argv[]) {
  HRESULT ret;
  HANDLE slave_read;
  STARTUPINFOEX si;
  PROCESS_INFORMATION pi;
  size_t list_size;
  char *cmd_line;

  if (!CreatePipe(&slave_read, &pty->master_output, NULL, 0)) {
    return 0;
  }

  if (!CreatePipe(&pty->master_input, &pty->slave_stdout, NULL, 0)) {
    goto error1;
  }

  ret = CreatePseudoConsole(pty->coord, slave_read, pty->slave_stdout, 0, &pty->hpc);
  if (FAILED(ret)) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " CreatePseudoConsole() failed with err %d.\n", ret);
#endif

    goto error2;
  }

  /* Prepare Startup Information structure */
  ZeroMemory(&si, sizeof(si));
  si.StartupInfo.cb = sizeof(STARTUPINFOEX);

  /* the size required for the list */
  InitializeProcThreadAttributeList(NULL, 1, 0, &list_size);

  si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, list_size);
  if (!si.lpAttributeList) {
    goto error3;
  }

  if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &list_size) ||
      /* Set the pseudoconsole information into the list */
      !UpdateProcThreadAttribute(si.lpAttributeList, 0,
                                 PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                 pty->hpc, sizeof(pty->hpc),
                                 NULL, NULL)) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " InitializeProcThreadAttributeList() or "
                    "UpdateProcThreadAttribute() failed with err %d.\n",
                    HRESULT_FROM_WIN32(GetLastError()));
#endif

    goto error4;
  }

  if (cmd_argv) {
    int count;
    size_t cmd_line_len;

    /* Because cmd_path == cmd_argv[0], cmd_argv[0] is ignored. */

    cmd_line_len = strlen(cmd_path) + 1;
    for (count = 1; cmd_argv[count] != NULL; count++) {
      cmd_line_len += (strlen(cmd_argv[count]) + 1);
    }

    if ((cmd_line = alloca(sizeof(char) * cmd_line_len)) == NULL) {
      goto error4;
    }

    strcpy(cmd_line, cmd_path);
    for (count = 1; cmd_argv[count] != NULL; count++) {
      strcat(cmd_line, " ");
      strcat(cmd_line, cmd_argv[count]);
    }
  } else {
    cmd_line = cmd_path;
  }

  ZeroMemory(&pi, sizeof(pi));

  if (!CreateProcess(cmd_path, cmd_line, NULL, NULL, FALSE,
                     EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                     &si.StartupInfo, &pi)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreateProcess() failed with err %d.\n",
                   HRESULT_FROM_WIN32(GetLastError()));
#endif

    goto error3;
  }

  pty->child_proc = pi.hProcess;
  pty->attr_list = si.lpAttributeList;

  CloseHandle(slave_read);
  CloseHandle(pi.hThread);

  return 1;

error4:
  HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

error3:
  ClosePseudoConsole(pty->hpc);

error2:
  CloseHandle(pty->master_input);
  CloseHandle(slave_read);

error1:
  CloseHandle(pty->master_output);
  CloseHandle(pty->slave_stdout);

  return 0;
}
#endif

static int pipe_open(vt_pty_win32_t *pty, const char *cmd_path, char *const cmd_argv[]) {
  HANDLE output_read_tmp, output_write;
  HANDLE input_write_tmp, input_read;
  HANDLE error_write;
  SECURITY_ATTRIBUTES sa;
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
  char *cmd_line;

  output_read_tmp = input_write_tmp = output_write = input_read = error_write = 0;

  /* Set up the security attributes struct. */
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;

  /* Create the child output pipe. */
  if (!CreatePipe(&output_read_tmp, &output_write, &sa, 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreatePipe() failed.\n");
#endif

    return 0;
  }

  /*
   * Create a duplicate of the output write handle for the std error
   * write handle. This is necessary in case the child application
   * closes one of its std output handles.
   */
  if (!DuplicateHandle(GetCurrentProcess(), output_write, GetCurrentProcess(), &error_write, 0,
                       TRUE, DUPLICATE_SAME_ACCESS)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " DuplicateHandle() failed.\n");
#endif

    goto error1;
  }

  /* Create the child input pipe. */
  if (!CreatePipe(&input_read, &input_write_tmp, &sa, 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreatePipe() failed.\n");
#endif

    goto error1;
  }

  /*
   * Create new output read handle and the input write handles. Set
   * the Properties to FALSE. Otherwise, the child inherits the
   * properties and, as a result, non-closeable handles to the pipes
   * are created.
   */
  if (!DuplicateHandle(GetCurrentProcess(), output_read_tmp, GetCurrentProcess(),
                       &pty->master_input, /* Address of new handle. */
                       0, FALSE,           /* Make it uninheritable. */
                       DUPLICATE_SAME_ACCESS)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " DuplicateHandle() failed.\n");
#endif

    goto error1;
  } else if (!DuplicateHandle(GetCurrentProcess(), input_write_tmp, GetCurrentProcess(),
                              &pty->master_output, /* Address of new handle. */
                              0, FALSE,            /* Make it uninheritable. */
                              DUPLICATE_SAME_ACCESS)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " DuplicateHandle() failed.\n");
#endif

    CloseHandle(pty->master_input);

    goto error1;
  }

  /*
   * Close inheritable copies of the handles you do not want to be
   * inherited.
   *
   * !! Notice !!
   * After here, goto error2 if error happens.
   */
  CloseHandle(output_read_tmp);
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

  if (cmd_argv) {
    int count;
    size_t cmd_line_len;

    /* Because cmd_path == cmd_argv[0], cmd_argv[0] is ignored. */

    cmd_line_len = strlen(cmd_path) + 1;
    for (count = 1; cmd_argv[count] != NULL; count++) {
      cmd_line_len += (strlen(cmd_argv[count]) + 1);
    }

    if ((cmd_line = alloca(sizeof(char) * cmd_line_len)) == NULL) {
      CloseHandle(pty->master_input);
      CloseHandle(pty->master_output);

      goto error2;
    }

    strcpy(cmd_line, cmd_path);
    for (count = 1; cmd_argv[count] != NULL; count++) {
      strcat(cmd_line, " ");
      strcat(cmd_line, cmd_argv[count]);
    }
  } else {
    cmd_line = cmd_path;
  }

  if (!CreateProcess(cmd_path, cmd_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si,
                     &pi)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreateProcess() failed.\n");
#endif

    CloseHandle(pty->master_input);
    CloseHandle(pty->master_output);

    goto error2;
  }

  pty->slave_stdout = output_write;

  /* Set global child process handle to cause threads to exit. */
  pty->child_proc = pi.hProcess;

#ifndef USE_CONPTY
  if (strstr(cmd_path, "plink")) {
    pty->is_plink = 1;
  } else {
    pty->is_plink = 0;
  }
#endif

  /* close unnecessary handles. */
  CloseHandle(pi.hThread);
  CloseHandle(input_read);
  CloseHandle(error_write);

  return 1;

error1:
  if (output_read_tmp) {
    CloseHandle(output_read_tmp);
  }

  if (input_write_tmp) {
    CloseHandle(input_write_tmp);
  }

error2:
  if (output_write) {
    CloseHandle(output_write);
  }

  if (error_write) {
    CloseHandle(error_write);
  }

  if (input_read) {
    CloseHandle(input_read);
  }

  return 0;
}

static int final(vt_pty_t *p) {
  vt_pty_win32_t *pty;
  int count;
  DWORD size;

  pty = (vt_pty_win32_t*)p;

#ifdef USE_CONPTY
  if (pty->hpc) {
    DeleteProcThreadAttributeList(pty->attr_list);
    /*
     * Any attached client character-mode applications, such as the one from the
     * CreateProcess call, will be terminated when the session is closed.
     * (https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session)
     */
    ClosePseudoConsole(pty->hpc);
  } else
#endif
  {
    /*
     * TerminateProcess must be called before CloseHandle.
     * If pty->child_proc is not in child_procs, pty->child_proc is already
     * closed in wait_child_exited, so TerminateProcess is not called.
     */
    for (count = 0; count < num_child_procs; count++) {
      if (pty->child_proc == child_procs[count]) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Terminate process %d\n", pty->child_proc);
#endif
        TerminateProcess(pty->child_proc, 0);

        break;
      }
    }
  }

  /* Used to check if child process is dead or not in wait_pty_read. */
  pty->child_proc = 0;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Trying to terminate wait_pty_read thread.");
#endif

  /*
   * If wait_pty_read waits in ReadFile( pty->master_input),
   * CloseHandle( pty->master_input) causes process to be blocked.
   * (Even if child process exits, ReadFile in wait_pty_read doesn't
   * exit by ERROR_BROKEN_PIPE, because pty->slave_stdout remains
   * opened in parent process.)
   * Write a dummy data to child process, and wait_pty_read waits
   * in WaitForSingleObject(pty->rd_ev).
   * Then close pty->slave_stdout and set pty->rd_ev.
   * As a result, wait_pty_read thread exits.
   */
  WriteFile(pty->slave_stdout, "", 1, &size, NULL);
  CloseHandle(pty->slave_stdout);
  SetEvent(pty->rd_ev);

#ifdef __DEBUG
  bl_msg_printf(" => Finished.\n");
#endif

#ifdef USE_OPEN_OSFHANDLE
  _close(pty->pty.master);
#else
  CloseHandle(pty->master_input);
#endif
  CloseHandle(pty->master_output);
  CloseHandle(pty->rd_ev);

  return 1;
}

static int set_winsize(vt_pty_t *pty, u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
#ifdef USE_CONPTY
  ((vt_pty_win32_t*)pty)->coord.X = cols;
  ((vt_pty_win32_t*)pty)->coord.Y = rows;

  if (((vt_pty_win32_t*)pty)->hpc) {
    ResizePseudoConsole(((vt_pty_win32_t*)pty)->hpc, ((vt_pty_win32_t*)pty)->coord);

    return 1;
  } else
#endif
  if (((vt_pty_win32_t*)pty)->is_plink) {
    /*
     * XXX Hack
     */

    u_char opt[5];

    opt[0] = 0xff;
    opt[1] = (cols >> 8) & 0xff;
    opt[2] = cols & 0xff;
    opt[3] = (rows >> 8) & 0xff;
    opt[4] = rows & 0xff;

    vt_write_to_pty(pty, opt, 5);

    return 1;
  }

  return 0;
}

/*
 * Return size of lost bytes.
 */
static ssize_t write_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  DWORD written_size;

  if (((vt_pty_win32_t*)pty)->hpc == NULL &&
      (memchr(buf, '\r', len) || memchr(buf, '\n', len))) {
    /*
     * \r or \n -> \r\n if ConPTY is not used, because putty 0.79 expects receiving \r\n.
     * (See console_get_userpass_input() in windows/console.c)
     */
    u_char *newbuf;

    if ((newbuf = alloca(len * 2))) {
      u_int count1;
      u_int count2;

      for (count1 = 0, count2 = 0; count1 < len; count1++, count2++) {
        if (buf[count1] == '\r') {
          if (buf[count1 + 1] == '\n') {
            newbuf[count2++] = buf[count1++];
          } else {
            goto replace;
          }
        } else if (buf[count1] == '\n') {
          goto replace;
        }

      normal:
        newbuf[count2] = buf[count1];
        continue;

      replace:
        newbuf[count2++] = '\r';
        newbuf[count2] = '\n';
      }

      buf = newbuf;
      len = count2;
    }
  }

  if (!WriteFile(((vt_pty_win32_t*)pty)->master_output, buf, len, &written_size, NULL)) {
    if (GetLastError() == ERROR_BROKEN_PIPE) {
      return -1;
    }
  }

  /*
   * FlushFileBuffers does not return until the client has read all data from the pipe.
   * (https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-operations)
   *
   * If a console application deosn't read stdin, FlushFileBuffers() blocks.
   */
#if 0
  FlushFileBuffers(((vt_pty_win32_t*)pty)->master_output);
#endif

  return written_size;
}

static ssize_t read_pty(vt_pty_t *p, u_char *buf, size_t len) {
  vt_pty_win32_t *pty;
  ssize_t n_rd;

  pty = (vt_pty_win32_t*)p;

  if (pty->rd_ch == '\0' && !pty->rd_ready) {
    return 0;
  }

  if (pty->rd_ch != '\0') {
    buf[0] = pty->rd_ch;
    n_rd = 1;
    len--;
    pty->rd_ch = '\0';
    pty->rd_ready = 1;
  } else {
    n_rd = 0;
  }

  while (len > 0) {
    DWORD ret;

    if (!PeekNamedPipe(pty->master_input, NULL, 0, NULL, &ret, NULL) || ret == 0 ||
        !ReadFile(pty->master_input, &buf[n_rd], len, &ret, NULL) || ret == 0) {
      break;
    }

    n_rd += ret;
    len -= ret;
  }

  if (n_rd == 0) {
    SetEvent(pty->rd_ev);
    pty->rd_ready = 0;
  }
#if 0
  else {
    int i;
    for (i = 0; i < n_rd; i++) {
      if (buf[i] == 0xff) {
        bl_msg_printf("Server Option => ");
        bl_msg_printf("%d ", buf[i++]);
        if (i < n_rd) printf("%d ", buf[i++]);
        if (i < n_rd) printf("%d ", buf[i++]);
        if (i < n_rd) printf("%d ", buf[i++]);
        if (i < n_rd) printf("%d ", buf[i++]);
      }
    }
  }
#endif

  return n_rd;
}

/* --- global functions --- */

void vt_set_use_conpty(int flag) {
#ifdef USE_CONPTY
  use_conpty = flag;
#endif
}

vt_pty_t *vt_pty_win32_new(const char *cmd_path, /* can be NULL */
                           char **cmd_argv,      /* can be NULL(only if cmd_path is NULL) */
                           char **env,           /* can be NULL */
                           const char *uri, const char *pass, u_int cols, u_int rows) {
  vt_pty_win32_t *pty;
  HANDLE thrd;
  DWORD tid;
  char ev_name[25];
  char *user;
  char *proto;
  char *host;
  char *port;
  int idx;

  if (num_child_procs == 0) {
    /*
     * Initialize child_procs array.
     */

    if ((child_procs = malloc(sizeof(HANDLE))) == NULL) {
      return NULL;
    }

    child_procs[0] = CreateEvent(NULL, FALSE, FALSE, "ADDED_CHILD");
    num_child_procs = 1;

    /* Launch the thread that wait for child exited. */
    if (!(thrd = CreateThread(NULL, 0, wait_child_exited, NULL, 0, &tid))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " CreateThread() failed.\n");
#endif

      return NULL;
    }

    CloseHandle(thrd);
  }

  if ((pty = calloc(1, sizeof(vt_pty_win32_t))) == NULL) {
    return NULL;
  }

  if (env) {
    while (*env) {
      char *p;
      char *key;
      char *val;

      if ((key = alloca(strlen(*env) + 1)) &&
          (p = strchr(strcpy(key, *env), '='))) {
        *p = '\0';
        val = ++p;
        SetEnvironmentVariable(key, val);
#ifdef __DEBUG
        bl_debug_printf("Env: %s=%s\n", key, val);
#endif
      }

      env++;
    }
  }

  if (!cmd_path || /* ! cmd_argv || */
      (strstr(cmd_path, "plink") && cmd_argv[0] && !cmd_argv[1])) {
    char *p;

    if (!(cmd_argv = alloca(sizeof(char*) * 8)) ||
        !(p = alloca(strlen(uri) + 1)) ||
        !bl_parse_uri(&proto, &user, &host, &port, NULL, NULL, strcpy(p, uri))) {
      free(pty);

      return NULL;
    }

    if (!cmd_path) {
      cmd_path = "plink.exe";
    }

    idx = 0;
    cmd_argv[idx++] = cmd_path;
    if (proto) {
      if ((p = alloca(strlen(proto) + 2))) {
        sprintf(p, "-%s", proto);
        cmd_argv[idx++] = p;

        /* -pw option can only be used with SSH. */
        if (strcmp(proto, "ssh") == 0) {
          cmd_argv[idx++] = "-pw";
          cmd_argv[idx++] = pass;
        }
      }
    }

    if (user || (user = getenv("USERNAME"))) {
      cmd_argv[idx++] = "-l";
      cmd_argv[idx++] = user;
    }
    cmd_argv[idx++] = host;
    cmd_argv[idx++] = NULL;
  }

#ifdef USE_CONPTY
  if (use_conpty) {
    set_winsize(&pty->pty, cols, rows, 0, 0);
  }

  if (!((use_conpty ? conpty_open : pipe_open)(pty, cmd_path, cmd_argv)))
#else
  if (!pipe_open(pty, cmd_path, cmd_argv))
#endif
  {
    free(pty);

    return NULL;
  }

  snprintf(ev_name, sizeof(ev_name), "PTY_READ_READY%x", (int)pty->child_proc);
  pty->rd_ev = CreateEvent(NULL, FALSE, FALSE, ev_name);
#ifdef __DEBUG
  bl_debug_printf("Created pty read event: %s\n", ev_name);
#endif

#ifdef USE_OPEN_OSFHANDLE
  pty->pty.master = _open_osfhandle(pty->master_input, 0);
#else
  pty->pty.master = (int)pty->master_input;    /* XXX Cast HANDLE => int */
#endif
  pty->pty.slave = (int)pty->slave_stdout;     /* XXX Cast HANDLE => int */
  pty->pty.child_pid = (pid_t)pty->child_proc; /* Cast HANDLE => pid_t */
  pty->pty.final = final;
  pty->pty.set_winsize = set_winsize;
  pty->pty.write = write_to_pty;
  pty->pty.read = read_pty;
  pty->pty.mode = PTY_WIN32;

#ifdef USE_CONPTY
  if (!use_conpty)
#endif
  {
    if (set_winsize(&pty->pty, cols, rows, 0, 0) == 0) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " vt_set_pty_winsize() failed.\n");
#endif
    }
  }

  /* Launch the thread that read the child's output. */
  if (!(thrd = CreateThread(NULL, 0, wait_pty_read, (LPVOID)pty, 0, &tid))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreateThread() failed.\n");
#endif

    vt_pty_destroy(&pty->pty);

    return NULL;
  } else {
    void *p;

    CloseHandle(thrd);

    /* Add to child_procs */

    if (!(p = realloc(child_procs, sizeof(HANDLE) * (num_child_procs + 1)))) {
      vt_pty_destroy(&pty->pty);

      return NULL;
    }

    child_procs = p;
    child_procs[num_child_procs++] = pty->child_proc;

    /*
     * Exit WaitForMultipleObjects in wait_child_proc and do
     * WaitForMultipleObjects again with new child_procs.
     */
    SetEvent(child_procs[0]);

#ifdef __DEBUG
    bl_warn_printf(BL_DEBUG_TAG " Added child procs NUM %d ADDED-HANDLE %d:%d.\n",
                   num_child_procs, child_procs[num_child_procs - 1], pty->child_proc);
#endif

    return &pty->pty;
  }
}

void vt_pty_win32_set_pty_read_trigger(void (*func)(void)) {
  trigger_pty_read = func;
}
