/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../vt_pty_intern.h"

#include <libssh2.h>
#include <pobl/bl_def.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>
#include <pobl/bl_sig_child.h>
#include <pobl/bl_path.h>
#include <pobl/bl_unistd.h> /* bl_usleep */
#include <pobl/bl_dialog.h>
#include <pobl/bl_net.h>     /* getaddrinfo/socket/connect/sockaddr_un */
#include <pobl/bl_conf_io.h> /* bl_get_user_rc_path */

#if !defined(USE_WIN32API) && defined(HAVE_PTHREAD)
#include <pthread.h>
#endif
#include <fcntl.h>  /* open */
#include <unistd.h> /* close/pipe */
#include <stdio.h>  /* sprintf */
#ifdef __CYGWIN__
#include <sys/wait.h>
#include <windows.h> /* GetModuleHandle */
#endif

#ifndef USE_WIN32API
#define closesocket(sock) close(sock)
#endif

#ifndef NO_DYNAMIC_LOAD_SSH
/*
 * If NO_DYNAMIC_LOAD_SSH is defined, mlterm/libptyssh/vt_pty_ssh.c is included
 * from mlterm/vt_pty_ssh.c.
 */
#define vt_write_to_pty(pty, buf, len) (*(pty)->write)(pty, buf, len)
#endif

#ifndef LIBSSH2_FLAG_COMPRESS
#define LIBSSH2_FLAG_COMPRESS 2
#endif
#ifndef LIBSSH2_ERROR_SOCKET_RECV
#define LIBSSH2_ERROR_SOCKET_RECV -43
#endif

#if 0
#define __DEBUG
#endif

typedef struct ssh_session {
  char *host;
  char *port;
  char *user;

  struct {
    char *pass;
    char *pubkey;
    char *privkey;
    char *cmd_path;
    char **argv;
    char **env;
    u_int cols;
    u_int rows;
    u_int width_pix;
    u_int height_pix;
  } *stored;

  LIBSSH2_SESSION *obj;
  int sock;

  int use_x11_forwarding;

  int suspended;

  LIBSSH2_CHANNEL **pty_channels;
  u_int num_of_ptys;

  int *x11_fds;
  LIBSSH2_CHANNEL **x11_channels;
  u_int num_of_x11;

} ssh_session_t;

typedef struct vt_pty_ssh {
  vt_pty_t pty;

  ssh_session_t *session;
  LIBSSH2_CHANNEL *channel;

  char *lo_buf;
  size_t lo_size;

  int is_eof;

} vt_pty_ssh_t;

typedef struct scp {
  LIBSSH2_CHANNEL *remote;
  int local;
  int src_is_remote;
  size_t src_size;

  vt_pty_ssh_t *pty_ssh;

} scp_t;

/* --- static variables --- */

static char *pass_response;

static ssh_session_t **sessions;
static u_int num_of_sessions = 0;

#ifdef USE_WIN32API
static HANDLE rd_ev;
DWORD main_tid; /* XXX set in main(). */
#endif

static const char *cipher_list;

static u_int keepalive_msec;
static u_int keepalive_msec_left;

static int use_x11_forwarding;
static int display_port = -1;

static int auth_agent_is_available;

static int auto_reconnect;

/* --- static functions --- */

#ifdef USE_WIN32API

static u_int __stdcall wait_pty_read(LPVOID thr_param) {
  u_int count;
  struct timeval tval;
  fd_set read_fds;
  int maxfd;

  tval.tv_usec = 500000; /* 0.5 sec */
  tval.tv_sec = 0;

#ifdef __DEBUG
  bl_debug_printf("Starting wait_pty_read thread.\n");
#endif

  while (num_of_sessions > 0) {
    FD_ZERO(&read_fds);
    maxfd = 0;

    for (count = 0; count < num_of_sessions; count++) {
      u_int count2;

      FD_SET(sessions[count]->sock, &read_fds);
      if (sessions[count]->sock > maxfd) {
        maxfd = sessions[count]->sock;
      }

      for (count2 = 0; count2 < sessions[count]->num_of_x11; count2++) {
        FD_SET(sessions[count]->x11_fds[count2], &read_fds);
        if (sessions[count]->x11_fds[count2] > maxfd) {
          maxfd = sessions[count]->x11_fds[count2];
        }
      }
    }

    if (select(maxfd + 1, &read_fds, NULL, NULL, &tval) > 0) {
      /* Exit GetMessage() in x_display_receive_next_event(). */
      PostThreadMessage(main_tid, WM_APP, 0, 0);

      WaitForSingleObject(rd_ev, INFINITE);
    }

#ifdef __DEBUG
    bl_debug_printf("Select socket...\n");
#endif
  }

#ifdef __DEBUG
  bl_debug_printf("Exiting wait_pty_read thread.\n");
#endif

  CloseHandle(rd_ev);
  rd_ev = 0;

/* Not necessary if thread started by _beginthreadex */
#if 0
  ExitThread(0);
#endif

  return 0;
}

#endif /* USE_WIN32API */

/* libssh2 frees response[0].text internally. */
#ifdef BL_DEBUG
#undef strdup
#endif

static void kbd_callback(const char *name, int name_len, const char *instruction,
                         int instruction_len, int num_prompts,
                         const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
                         LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses, void **abstract) {
  (void)name;
  (void)name_len;
  (void)instruction;
  (void)instruction_len;
  if (num_prompts == 1) {
    responses[0].text = strdup(pass_response);
    responses[0].length = strlen(pass_response);
    pass_response = NULL;
  }
  (void)prompts;
  (void)abstract;
}

#ifdef BL_DEBUG
#define strdup(str) bl_str_dup(str, __FILE__, __LINE__, __FUNCTION__)
#endif

#ifdef OPEN_PTY_ASYNC

#ifdef USE_WIN32API

static HANDLE *openssl_locks;

static void openssl_lock_callback(int mode, int type, const char *file, int line) {
  if (mode & 1 /* CRYPTO_LOCK */) {
    WaitForSingleObject(openssl_locks[type], INFINITE);
  } else {
    ReleaseMutex(openssl_locks[type]);
  }
}

#else

static pthread_mutex_t *openssl_locks;

static void openssl_lock_callback(int mode, int type, const char *file, int line) {
  if (mode & 1 /* CRYPTO_LOCK */) {
    pthread_mutex_lock(&openssl_locks[type]);
  } else {
    pthread_mutex_unlock(&openssl_locks[type]);
  }
}

#endif

int CRYPTO_num_locks(void);
void CRYPTO_set_locking_callback(void (*func)(int, int, const char *, int));

/* gcrypt is not supported. */
static void set_use_multi_thread(int use) {
  static int num_locks;
  int count;

  if (use) {
    num_locks = CRYPTO_num_locks();

    if ((openssl_locks = malloc(num_locks * sizeof(*openssl_locks)))) {
      for (count = 0; count < num_locks; count++) {
#ifdef USE_WIN32API
        openssl_locks[count] = CreateMutex(NULL, FALSE, NULL);
#else
        openssl_locks[count] = PTHREAD_MUTEX_INITIALIZER;
#endif
      }

      CRYPTO_set_locking_callback(openssl_lock_callback);
    } else {
      num_locks = 0;
    }
  } else {
    if (openssl_locks) {
      CRYPTO_set_locking_callback(NULL);

#ifdef USE_WIN32API
      for (count = 0; count < num_locks; count++) {
        CloseHandle(openssl_locks[count]);
      }
#endif

      free(openssl_locks);
      openssl_locks = NULL;
    }
  }
}

#else
#define set_use_multi_thread(use) (0)
#endif

#ifdef AI_PASSIVE
#define HAVE_GETADDRINFO
#endif

static void x11_callback(LIBSSH2_SESSION *session, LIBSSH2_CHANNEL *channel, char *shost, int sport,
                         void **abstract);

/*
 * Return session which is non-blocking mode because opening a new channel
 * can work as multi threading.
 */
static ssh_session_t *ssh_connect(const char *host, const char *port, const char *user,
                                  const char *pass, const char *pubkey, const char *privkey) {
  ssh_session_t *session;
#ifdef HAVE_GETADDRINFO
  struct addrinfo hints;
  struct addrinfo *addr;
  struct addrinfo *addr_p;
#else
  struct hostent *hent;
  struct sockaddr_in addr;
  int count;
#endif

  const char *hostkey;
  size_t hostkey_len;
  int hostkey_type;

  char *userauthlist;
  int auth_success = 0;

  if ((session = vt_search_ssh_session(host, port, user))) {
    return session;
  }

  if (!(session = calloc(1, sizeof(ssh_session_t)))) {
    return NULL;
  }

  set_use_multi_thread(1);

  if (num_of_sessions == 0 && libssh2_init(0) != 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " libssh2_init failed.\n");
#endif

    goto error1;
  }

#ifdef HAVE_GETADDRINFO
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  if (getaddrinfo(host, port, &hints, &addr) != 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " getaddrinfo failed.\n");
#endif

    goto error2;
  }

  addr_p = addr;
  while (1) {
    if ((session->sock = socket(addr_p->ai_family, addr_p->ai_socktype, addr_p->ai_protocol)) >=
        0) {
      if (connect(session->sock, addr_p->ai_addr, addr_p->ai_addrlen) == 0) {
        break;
      } else {
        closesocket(session->sock);
      }
    }

    if ((addr_p = addr_p->ai_next) == NULL) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " socket/connect failed.\n");
#endif

      freeaddrinfo(addr);

      goto error2;
    }
  }

  freeaddrinfo(addr);
#else
  if (!(hent = gethostbyname(host))) {
    goto error2;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_port = htons(atoi(port));
  addr.sin_family = AF_INET;

  count = 0;
  while (1) {
    if (!hent->h_addr_list[count]) {
      goto error2;
    }

    if (hent->h_addrtype == AF_INET) {
      addr.sin_addr.s_addr = *((u_int *)hent->h_addr_list[count]);

      if ((session->sock = socket(addr.sin_family, SOCK_STREAM, 0)) >= 0) {
        if (connect(session->sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
          break;
        } else {
          closesocket(session->sock);
        }
      }
    }

    count++;
  }
#endif /* HAVE_GETADDRINFO */

  if (!(session->obj = libssh2_session_init())) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " libssh2_session_init failed.\n");
#endif

    goto error3;
  }

#ifdef DEBUG
  libssh2_trace(session->obj, -1);
#endif

  libssh2_session_set_blocking(session->obj, 1);
  libssh2_session_set_timeout(session->obj, 30000); /* 30 sec */

  if (cipher_list) {
    libssh2_session_method_pref(session->obj, LIBSSH2_METHOD_CRYPT_CS, cipher_list);
    libssh2_session_method_pref(session->obj, LIBSSH2_METHOD_CRYPT_SC, cipher_list);
  }

  libssh2_session_callback_set(session->obj, LIBSSH2_CALLBACK_X11, x11_callback);

#if !defined(LIBSSH2_VERSION_NUM) || LIBSSH2_VERSION_NUM < 0x010500
#ifndef LIBSSH2_FIX_DECOMPRESS_BUG
  /*
   * XXX
   * libssh2 1.4.3 or before fails to decompress zipped packets and breaks X11
   * forwarding.
   * Camellia branch of http://bitbucket.org/araklen/libssh2/ fixes this bug
   * and defines LIBSSH2_FIX_DECOMPRESS_BUG macro.
   * libssh2 1.5.0 or later fixes this bug.
   */
  if (!use_x11_forwarding)
#endif
#endif
  {
    libssh2_session_flag(session->obj, LIBSSH2_FLAG_COMPRESS, 1);
  }

  session->use_x11_forwarding = use_x11_forwarding;

  if (libssh2_session_handshake(session->obj, session->sock) != 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " libssh2_session_handshake failed.\n");
#endif

    goto error4;
  }

  /*
   * Check ~/.ssh/knownhosts.
   */

  if ((hostkey = libssh2_session_hostkey(session->obj, &hostkey_len, &hostkey_type))) {
    char *home;
    char *path;
    LIBSSH2_KNOWNHOSTS *nhs;

    if ((home = bl_get_home_dir()) && (path = alloca(strlen(home) + 20)) &&
        (nhs = libssh2_knownhost_init(session->obj))) {
      struct libssh2_knownhost *nh;

#ifdef USE_WIN32API
      sprintf(path, "%s\\mlterm\\known_hosts", home);
#else
      sprintf(path, "%s/.ssh/known_hosts", home);
#endif

      libssh2_knownhost_readfile(nhs, path, LIBSSH2_KNOWNHOST_FILE_OPENSSH);

      if (libssh2_knownhost_checkp(nhs, host, atoi(port), hostkey, hostkey_len,
                                   LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW,
                                   &nh) != LIBSSH2_KNOWNHOST_CHECK_MATCH) {
        const char *hash;
        size_t count;
        char *msg;
        char *p;

        hash = libssh2_hostkey_hash(session->obj, LIBSSH2_HOSTKEY_HASH_SHA1);

        msg = alloca(strlen(host) + 31 + 3 * 20 + 1);

        sprintf(msg, "Connecting to unknown host: %s (", host);
        p = msg + strlen(msg);
        for (count = 0; count < 20; count++) {
          sprintf(p + count * 3, "%02x:", (u_char)hash[count]);
        }
        msg[strlen(msg) - 1] = ')'; /* replace ':' with ')' */

        if (!bl_dialog(BL_DIALOG_OKCANCEL, msg)) {
          libssh2_knownhost_free(nhs);

          goto error4;
        }

        libssh2_knownhost_add(nhs, host, NULL, hostkey, hostkey_len,
                              LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW |
                                  LIBSSH2_KNOWNHOST_KEY_SSHRSA,
                              NULL);

        libssh2_knownhost_writefile(nhs, path, LIBSSH2_KNOWNHOST_FILE_OPENSSH);

        bl_msg_printf("Add to %s and continue connecting.\n", path);
      }

      libssh2_knownhost_free(nhs);
    }
  }

  if (!(userauthlist = libssh2_userauth_list(session->obj, user, strlen(user)))) {
    goto error4;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Authentication methods: %s\n", userauthlist);
#endif

  if (strstr(userauthlist, "publickey")) {
    char *home;
    char *p;
    LIBSSH2_AGENT *agent;

    if (*pass == '\0' && (agent = libssh2_agent_init(session->obj))) {
      if (libssh2_agent_connect(agent) == 0) {
        if (libssh2_agent_list_identities(agent) == 0) {
          struct libssh2_agent_publickey *ident;
          struct libssh2_agent_publickey *prev_ident;

          prev_ident = NULL;
          while (libssh2_agent_get_identity(agent, &ident, prev_ident) == 0) {
            if (libssh2_agent_userauth(agent, user, ident) == 0) {
              libssh2_agent_disconnect(agent);
              libssh2_agent_free(agent);

              auth_agent_is_available = 1;

              goto pubkey_success;
            }

            prev_ident = ident;
          }
        }

        libssh2_agent_disconnect(agent);
      }

      libssh2_agent_free(agent);
    }

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Unable to use ssh-agent.\n");
#endif

    if ((home = bl_get_home_dir()) && ((p = alloca(strlen(home) * 2 + 38)))) {
      if (!pubkey) {
#ifdef USE_WIN32API
        sprintf(p, "%s\\mlterm\\id_rsa.pub", home);
#else
        sprintf(p, "%s/.ssh/id_rsa.pub", home);
#endif

        pubkey = p;
        p += (strlen(pubkey) + 1);
      }

      if (!privkey) {
#ifdef USE_WIN32API
        sprintf(p, "%s\\mlterm\\id_rsa", home);
#else
        sprintf(p, "%s/.ssh/id_rsa", home);
#endif

        privkey = p;
      }
    } else {
      if (!pubkey) {
#ifdef USE_WIN32API
        pubkey = "mlterm\\ssh_host_rsa_key.pub";
#else
        pubkey = "/etc/ssh/ssh_host_rsa_key.pub";
#endif
      }

      if (!privkey) {
#ifdef USE_WIN32API
        privkey = "mlterm\\ssh_host_rsa_key";
#else
        privkey = "/etc/ssh/ssh_host_rsa_key";
#endif
      }
    }

    if (libssh2_userauth_publickey_fromfile(session->obj, user, pubkey, privkey, pass) == 0) {
    pubkey_success:
      bl_msg_printf("Authentication by public key succeeded.\n");

      auth_success = 1;
    }
#ifdef DEBUG
    else {
      bl_debug_printf(BL_DEBUG_TAG " Authentication by public key failed.\n");
    }
#endif
  }

  if (!auth_success && strstr(userauthlist, "keyboard-interactive")) {
    pass_response = pass;

    if (libssh2_userauth_keyboard_interactive(session->obj, user, &kbd_callback) == 0) {
      bl_msg_printf("Authentication by keyboard-interactive succeeded.\n");

      auth_success = 1;
    }
#ifdef DEBUG
    else {
      bl_debug_printf(BL_DEBUG_TAG " Authentication by keyboard-interactive failed.\n");
    }
#endif
  }

  if (!auth_success) {
    if (!strstr(userauthlist, "password")) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " No supported authentication methods found.\n");
#endif

      goto error4;
    }

    if (libssh2_userauth_password(session->obj, user, pass) != 0) {
      bl_msg_printf("Authentication by password failed.\n");

      goto error4;
    }

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Authentication by password succeeded.\n");
#endif
  }

  {
    void *p;

    if (!(p = realloc(sessions, sizeof(ssh_session_t) * (num_of_sessions + 1)))) {
      goto error4;
    }

    sessions = p;
  }

  libssh2_session_set_timeout(session->obj, 0);
  libssh2_session_set_blocking(session->obj, 0);

  session->host = strdup(host);
  session->port = strdup(port);
  session->user = strdup(user);

  sessions[num_of_sessions++] = session;

  return session;

error4:
  libssh2_session_disconnect(session->obj, "Normal shutdown, Thank you for playing");
  libssh2_session_free(session->obj);

error3:
  closesocket(session->sock);

error2:
  if (num_of_sessions == 0) {
    libssh2_exit();
  }

error1:
  set_use_multi_thread(0);
  free(session);

  return NULL;
}

static void close_x11(ssh_session_t *session, int idx);

/*
 * Call with in blocking mode.
 */
static int ssh_disconnect(ssh_session_t *session) {
  u_int count;

  if (session->num_of_ptys > 0) {
    /* In case this function is called from vt_pty_new. */
    libssh2_session_set_blocking(session->obj, 0);

    return 0;
  }

  for (count = 0; count < num_of_sessions; count++) {
    if (sessions[count] == session) {
      sessions[count] = sessions[--num_of_sessions];

      if (num_of_sessions == 0) {
        free(sessions);
        sessions = NULL;
      }

      break;
    }
  }

  for (count = session->num_of_x11; count > 0; count--) {
    close_x11(session, count - 1);
  }

  libssh2_session_disconnect(session->obj, "Normal shutdown, Thank you for playing");
  libssh2_session_free(session->obj);
  closesocket(session->sock);

  if (num_of_sessions == 0) {
    libssh2_exit();
  }

  free(session->host);
  free(session->port);
  free(session->user);
  free(session->stored);
  free(session->pty_channels);
  free(session->x11_fds);
  free(session->x11_channels);
  free(session);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Closed session.\n");
#endif

  return 1;
}

static int unuse_loopback(vt_pty_t *pty);

static int final(vt_pty_t *pty) {
  ssh_session_t *session;
  u_int count;

  unuse_loopback((vt_pty_ssh_t *)pty);

  session = ((vt_pty_ssh_t *)pty)->session;

  libssh2_session_set_blocking(session->obj, 1);

  for (count = 0; count < session->num_of_ptys; count++) {
    if (session->pty_channels[count] == ((vt_pty_ssh_t *)pty)->channel) {
      session->pty_channels[count] = session->pty_channels[--session->num_of_ptys];
      break;
    }
  }

  libssh2_channel_free(((vt_pty_ssh_t *)pty)->channel);
  ssh_disconnect(session);

  return 1;
}

static int set_winsize(vt_pty_t *pty, u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " win size cols %d rows %d width %d height %d.\n", cols, rows,
                  width_pix, height_pix);
#endif

  libssh2_channel_request_pty_size_ex(((vt_pty_ssh_t *)pty)->channel, cols, rows, width_pix,
                                      height_pix);

  return 1;
}

static int reconnect(vt_pty_ssh_t *pty);
static int use_loopback(vt_pty_t *pty);
static ssize_t lo_write_to_pty(vt_pty_t *pty, u_char *buf, size_t len);

static int zombie(vt_pty_ssh_t *pty) {
  if (use_loopback(&pty->pty)) {
    lo_write_to_pty(&pty->pty, "=== Press any key to exit ===", 29);
    pty->is_eof = 1;

    return 1;
  }

  return 0;
}

static ssize_t write_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  ssize_t ret;

  if (((vt_pty_ssh_t *)pty)->session->suspended) {
    return 0;
  }

  ret = libssh2_channel_write(((vt_pty_ssh_t *)pty)->channel, buf, len);

  if (ret == LIBSSH2_ERROR_SOCKET_SEND || ret == LIBSSH2_ERROR_SOCKET_RECV ||
      libssh2_channel_eof(((vt_pty_ssh_t *)pty)->channel)) {
    if ((ret < 0 && reconnect(pty)) || zombie((vt_pty_ssh_t *)pty)) {
      return 0;
    }

    bl_trigger_sig_child(pty->child_pid);

    return -1;
  } else {
    return ret < 0 ? 0 : ret;
  }
}

static ssize_t read_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  ssize_t ret;

  if (((vt_pty_ssh_t *)pty)->lo_buf) {
    if (((vt_pty_ssh_t *)pty)->lo_size < len) {
      len = ((vt_pty_ssh_t *)pty)->lo_size;
    }

    memcpy(buf, ((vt_pty_ssh_t *)pty)->lo_buf, len);

    /* XXX */
    free(((vt_pty_ssh_t *)pty)->lo_buf);
    ((vt_pty_ssh_t *)pty)->lo_buf = NULL;
    ((vt_pty_ssh_t *)pty)->lo_size = 0;

    return len;
  }

  if (((vt_pty_ssh_t *)pty)->session->suspended) {
    return 0;
  }

  ret = libssh2_channel_read(((vt_pty_ssh_t *)pty)->channel, buf, len);

#ifdef USE_WIN32API
  SetEvent(rd_ev);
#endif

  if (ret == LIBSSH2_ERROR_SOCKET_SEND || ret == LIBSSH2_ERROR_SOCKET_RECV ||
      libssh2_channel_eof(((vt_pty_ssh_t *)pty)->channel)) {
    if ((ret < 0 && reconnect(pty)) || zombie((vt_pty_ssh_t *)pty)) {
      return 0;
    }

    bl_trigger_sig_child(pty->child_pid);

    return -1;
  } else {
    return ret < 0 ? 0 : ret;
  }
}

static int scp_stop(vt_pty_ssh_t *pty_ssh) {
  pty_ssh->session->suspended = -1;

  return 1;
}

#ifdef USE_WIN32API
static ssize_t lo_recv_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  return recv(pty->master, buf, len, 0);
}

static ssize_t lo_send_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  if (len == 1 && buf[0] == '\x03') {
    /* ^C */
    scp_stop(pty);
  }

  return send(pty->slave, buf, len, 0);
}

static int _socketpair(int af, int type, int proto, SOCKET sock[2]) {
  SOCKET listen_sock;
  SOCKADDR_IN addr;
  int addr_len;

  if ((listen_sock = WSASocket(af, type, proto, NULL, 0, 0)) == -1) {
    return -1;
  }

  addr_len = sizeof(addr);

  memset((void *)&addr, 0, sizeof(addr));
  addr.sin_family = af;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  if (bind(listen_sock, (SOCKADDR *)&addr, addr_len) != 0) {
    goto error1;
  }

  if (getsockname(listen_sock, (SOCKADDR *)&addr, &addr_len) != 0) {
    goto error1;
  }

  if (listen(listen_sock, 1) != 0) {
    goto error1;
  }

  if ((sock[0] = WSASocket(af, type, proto, NULL, 0, 0)) == -1) {
    goto error1;
  }

  if (connect(sock[0], (SOCKADDR *)&addr, addr_len) != 0) {
    goto error2;
  }

  if ((sock[1] = accept(listen_sock, 0, 0)) == -1) {
    goto error2;
  }

  closesocket(listen_sock);

  return 0;

error2:
  closesocket(sock[0]);

error1:
  closesocket(listen_sock);

  return -1;
}
#endif /* USE_WIN32API */

#ifdef __CYGWIN__
static int check_sig_child(pid_t pid) {
  /* SIGCHLD signal isn't delivered on cygwin even if mlconfig exits. */
  int status;

  if (pid > 0 && waitpid(pid, &status, WNOHANG) == pid) {
    bl_trigger_sig_child(pid);

    return 1;
  } else {
    return 0;
  }
}
#endif

static ssize_t lo_read_pty(vt_pty_t *pty, u_char *buf, size_t len) {
#ifdef __CYGWIN__
  if (check_sig_child(pty->config_menu.pid)) {
    /* vt_pty_set_use_loopback(0) is called from sig_child() in vt_config_menu.c. */
    return 0;
  }
#endif

  return read(pty->master, buf, len);
}

static ssize_t lo_write_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
#ifdef __CYGWIN__
  if (check_sig_child(pty->config_menu.pid)) {
    /*
     * vt_pty_set_use_loopback(0) is called from sig_child()
     * in vt_config_menu.c is called
     */
    return 0;
  }
#endif

  if (len == 1 && buf[0] == '\x03') {
    /* ^C */
    scp_stop((vt_pty_ssh_t *)pty);
  }
  else if (((vt_pty_ssh_t *)pty)->is_eof) {
    bl_trigger_sig_child(pty->child_pid);

    return -1;
  }

  return write(pty->slave, buf, len);
}

static int use_loopback(vt_pty_t *pty) {
  int fds[2];

  if (pty->stored) {
    pty->stored->ref_count++;

    return 1;
  }

  if ((pty->stored = malloc(sizeof(*(pty->stored)))) == NULL) {
    return 0;
  }

  pty->stored->master = pty->master;
  pty->stored->slave = pty->slave;
  pty->stored->read = pty->read;
  pty->stored->write = pty->write;

#ifdef USE_WIN32API
  if (_socketpair(AF_INET, SOCK_STREAM, 0, fds) == 0) {
    u_long val;

    val = 1;
    ioctlsocket(fds[0], FIONBIO, &val);
    val = 1;
    ioctlsocket(fds[1], FIONBIO, &val);

    pty->read = lo_recv_pty;
    pty->write = lo_send_to_pty;
  } else if (_pipe(fds, 256, O_BINARY) == 0) {
    pty->read = lo_read_pty;
    pty->write = lo_write_to_pty;
  }
#else
  if (pipe(fds) == 0) {
    fcntl(fds[0], F_SETFL, O_NONBLOCK | fcntl(pty->master, F_GETFL, 0));
    fcntl(fds[1], F_SETFL, O_NONBLOCK | fcntl(pty->slave, F_GETFL, 0));

    pty->read = lo_read_pty;
    pty->write = lo_write_to_pty;
  }
#endif
  else {
    free(pty->stored);
    pty->stored = NULL;

    return 0;
  }

  pty->master = fds[0];
  pty->slave = fds[1];

  pty->stored->ref_count = 1;

  ((vt_pty_ssh_t *)pty)->session->suspended = 1;

  return 1;
}

static int unuse_loopback(vt_pty_t *pty) {
  char buf[128];
  ssize_t len;

  if (!pty->stored || --(pty->stored->ref_count) > 0) {
    return 1;
  }

  while ((len = (*pty->read)(pty, buf, sizeof(buf))) > 0) {
    char *p;

    if (!(p = realloc(((vt_pty_ssh_t *)pty)->lo_buf, ((vt_pty_ssh_t *)pty)->lo_size + len))) {
      break;
    }

    memcpy(p + ((vt_pty_ssh_t *)pty)->lo_size, buf, len);
    ((vt_pty_ssh_t *)pty)->lo_buf = p;
    ((vt_pty_ssh_t *)pty)->lo_size += len;
  }

#ifdef USE_WIN32API
  if (pty->read == lo_recv_pty) {
    closesocket(pty->slave);
    closesocket(pty->master);
  } else
#endif
  {
    close(pty->slave);
    close(pty->master);
  }

  pty->master = pty->stored->master;
  pty->slave = pty->stored->slave;
  pty->read = pty->stored->read;
  pty->write = pty->stored->write;

  free(pty->stored);
  pty->stored = NULL;

  ((vt_pty_ssh_t *)pty)->session->suspended = 0;

  return 1;
}

#ifdef USE_WIN32API
static u_int __stdcall
#else
static void *
#endif
    scp_thread(void *p) {
  scp_t *scp;
  size_t rd_len;
  char buf[8192];
  int progress;
  char msg1[] = "\x1b[?25l\r\nTransferring data.\r\n|";
  char msg2[] = "**************************************************|\x1b[?25h\r\n";
  char err_msg[] = "\r\nInterrupted.\x1b[?25h\r\n";

#if !defined(USE_WIN32API) && defined(HAVE_PTHREAD)
  pthread_detach(pthread_self());
#endif

  scp = p;

  rd_len = 0;
  progress = 0;

  vt_write_to_pty(&scp->pty_ssh->pty, msg1, sizeof(msg1) - 1);

  while (rd_len < scp->src_size && scp->pty_ssh->session->suspended > 0) {
    int new_progress;
    ssize_t len;

    if (scp->src_is_remote) {
      if ((len = libssh2_channel_read(scp->remote, buf, sizeof(buf))) < 0) {
        if (len == LIBSSH2_ERROR_EAGAIN) {
          bl_usleep(1);
          continue;
        } else {
          break;
        }
      }

      write(scp->local, buf, len);
    } else {
      if ((len = read(scp->local, buf, sizeof(buf))) < 0) {
        break;
      }

      while (libssh2_channel_write(scp->remote, buf, len) == LIBSSH2_ERROR_EAGAIN) {
        bl_usleep(1);
      }
    }

    rd_len += len;

    new_progress = 50 * rd_len / scp->src_size;

    if (progress < new_progress && new_progress < 50) {
      int count;

      progress = new_progress;

      for (count = 0; count < new_progress; count++) {
        vt_write_to_pty(&scp->pty_ssh->pty, "*", 1);
      }
      for (; count < 50; count++) {
        vt_write_to_pty(&scp->pty_ssh->pty, " ", 1);
      }
      vt_write_to_pty(&scp->pty_ssh->pty, "|\r|", 3);

#ifdef USE_WIN32API
      /* Exit GetMessage() in x_display_receive_next_event(). */
      PostThreadMessage(main_tid, WM_APP, 0, 0);
#endif
    }
  }

  if (scp->pty_ssh->session->suspended > 0) {
    vt_write_to_pty(&scp->pty_ssh->pty, msg2, sizeof(msg2) - 1);
  } else {
    vt_write_to_pty(&scp->pty_ssh->pty, err_msg, sizeof(err_msg) - 1);
  }

#if 1
  bl_usleep(100000); /* Expect to switch to main thread and call vt_read_pty(). */
#else
  pthread_yield();
#endif

  while (libssh2_channel_free(scp->remote) == LIBSSH2_ERROR_EAGAIN)
    ;
  close(scp->local);

  unuse_loopback(&scp->pty_ssh->pty);

  scp->pty_ssh->session->suspended = 0;

  free(scp);

/* Not necessary if thread started by _beginthreadex */
#if 0
  ExitThread(0);
#endif

  return 0;
}

#if 0
#define TRUSTED
#endif

static int setup_x11(LIBSSH2_CHANNEL *channel) {
  char *display;
  char *display_port_str;
  char *p;
  char *proto;
  char *data;
#if !defined(USE_WIN32API) && !defined(OPEN_PTY_ASYNC)
  char *cmd;
  FILE *fp;
  char line[512];
#ifndef TRUSTED
  char *xauth_file;
#endif
#endif
  int ret;

  if (!(display = getenv("DISPLAY"))) {
    display = ":0.0";
  }

  if (strncmp(display, "unix:", 5) == 0) {
    display_port_str = display + 5;
  } else if (display[0] == ':') {
    display_port_str = display + 1;
  } else {
    return 0;
  }

  if (!(display_port_str = bl_str_alloca_dup(display_port_str))) {
    return 0;
  }

  if ((p = strrchr(display_port_str, '.'))) {
    *p = '\0';
  }

  display_port = atoi(display_port_str);

  proto = NULL;
  data = NULL;

/* I don't know why but system() and popen() can freeze if OPEN_PTY_ASYNC. */
#if !defined(USE_WIN32API) && !defined(OPEN_PTY_ASYNC)
#ifdef TRUSTED
  if ((cmd = alloca(24 + strlen(display) + 1))) {
    sprintf(cmd, "xauth list %s 2> /dev/null", display);
#else
  if ((xauth_file = bl_get_user_rc_path("mlterm/xauthfile"))) {
    if ((cmd = alloca(61 + strlen(xauth_file) + strlen(display) + 1))) {
      sprintf(cmd,
              "xauth -f %s generate %s MIT-MAGIC-COOKIE-1 "
              "untrusted 2> /dev/null",
              xauth_file, display);
      system(cmd);
      sprintf(cmd, "xauth -f %s list %s 2> /dev/null", xauth_file, display);
#endif

      if ((fp = popen(cmd, "r"))) {
        if (fgets(line, sizeof(line), fp)) {
          if ((proto = strchr(line, ' '))) {
            proto += 2;

            if ((data = strchr(proto, ' '))) {
              *data = '\0';
              data += 2;

              if ((p = strchr(data, '\n'))) {
                *p = '\0';
              }
            }
          }
        }

        pclose(fp);
      }

#ifdef TRUSTED
  }
#else
      unlink(xauth_file);
    }

    free(xauth_file);
  }
#endif
#endif

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " libssh2_channel_x11_req_ex (with xauth %s %s)\n", proto, data);
#endif

  while ((ret = libssh2_channel_x11_req_ex(channel, 0, proto, data, 0)) == LIBSSH2_ERROR_EAGAIN)
    ;

  return ret == 0;
}

static int open_channel(vt_pty_ssh_t *pty,    /* pty->session is non-blocking */
                        const char *cmd_path, /* can be NULL */
                        char **cmd_argv,      /* can be NULL(only if cmd_path is NULL) */
                        char **env,           /* can be NULL */
                        u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  char *term;
  void *p;
  int ret;

  if (pty->session->suspended) {
    goto error2;
  }

  if (!(p = realloc(pty->session->pty_channels,
                    (pty->session->num_of_ptys + 1) * sizeof(LIBSSH2_CHANNEL *)))) {
    goto error2;
  }

  pty->session->pty_channels = p;

#if 0
  while (!(pty->channel = libssh2_channel_open_session(pty->session->obj)))
#else
  while (!(pty->channel = libssh2_channel_open_ex(
               pty->session->obj, "session", sizeof("session") - 1,
               /* RLogin's window size */
               64 * LIBSSH2_CHANNEL_PACKET_DEFAULT, LIBSSH2_CHANNEL_PACKET_DEFAULT, NULL, 0)))
#endif
  {
    if (libssh2_session_last_errno(pty->session->obj) != LIBSSH2_ERROR_EAGAIN) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Unable to open a channel\n");
#endif

      goto error2;
    }
  }

  pty->session->suspended = 0;

  if (auth_agent_is_available) {
#if defined(__CYGWIN__)
    static int (*func)(LIBSSH2_CHANNEL *);
    static int is_tried;

    if (!is_tried) {
      func = GetProcAddress(GetModuleHandle("cygssh2-1"), "libssh2_channel_request_auth_agent");
      is_tried = 1;
    }

    if (func) {
      while ((ret = (*func)(pty->channel)) == LIBSSH2_ERROR_EAGAIN)
        ;
      if (ret == 0) {
        bl_msg_printf("Agent forwarding.\n");
      }
    }
#elif defined(LIBSSH2_FORWARD_AGENT)
    while ((ret = libssh2_channel_request_auth_agent(pty->channel)) == LIBSSH2_ERROR_EAGAIN)
      ;
    if (ret == 0) {
      bl_msg_printf("Agent forwarding.\n");
    }
#endif

    auth_agent_is_available = 0;
  }

  term = NULL;
  if (env) {
    while (*env) {
      char *val;
      size_t key_len;

      if ((val = strchr(*env, '='))) {
        key_len = val - *env;
        val++;
      } else {
        key_len = strlen(*env);
        val = "";
      }

      while (libssh2_channel_setenv_ex(pty->channel, *env, key_len, val, strlen(val)) ==
             LIBSSH2_ERROR_EAGAIN)
        ;

#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Env %s => key_len %d val %s\n", *env, key_len, val);
#endif

      if (strncmp(*env, "TERM=", 5) == 0) {
        term = val;
      }

      env++;
    }
  }

  while ((ret = libssh2_channel_request_pty(pty->channel, term ? term : "xterm")) < 0) {
    if (ret != LIBSSH2_ERROR_EAGAIN) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Failed to request pty. (Err %d)\n", ret);
#endif

      goto error3;
    }
  }

  if (pty->session->use_x11_forwarding) {
    if (!setup_x11(pty->channel)) {
      bl_msg_printf("X11 forwarding failed.\n");
    }
  }

  if (cmd_path) {
    int count;
    size_t cmd_line_len;

    /* Because cmd_path == cmd_argv[0], cmd_argv[0] is ignored. */

    /* 1 = NULL terminator */
    cmd_line_len = strlen(cmd_path) + 1;
    for (count = 1; cmd_argv[count] != NULL; count++) {
      /* 3 = " " */
      cmd_line_len += (strlen(cmd_argv[count]) + 3);
    }

    if ((pty->pty.cmd_line = malloc(sizeof(char) * cmd_line_len)) == NULL) {
      goto error3;
    }

    strcpy(pty->pty.cmd_line, cmd_path);
    for (count = 1; cmd_argv[count] != NULL; count++) {
      sprintf(pty->pty.cmd_line + strlen(pty->pty.cmd_line),
              strchr(cmd_argv[count], ' ') ? " \"%s\"" : " %s", cmd_argv[count]);
    }

    while ((ret = libssh2_channel_exec(pty->channel, pty->pty.cmd_line)) < 0) {
      if (ret != LIBSSH2_ERROR_EAGAIN) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Unable to exec %s on allocated pty. (Err %d)\n",
                        pty->pty.cmd_line, ret);
#endif

        goto error3;
      }
    }
  } else {
    /* Open a SHELL on that pty */
    while ((ret = libssh2_channel_shell(pty->channel)) < 0) {
      if (ret != LIBSSH2_ERROR_EAGAIN) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Unable to request shell on allocated pty. (Err %d)\n", ret);
#endif

        goto error3;
      }
    }
  }

  pty->pty.master = pty->session->sock;
  pty->pty.slave = -1;
  pty->pty.child_pid = (pid_t)pty->channel; /* XXX regarding pid_t as channel */
  pty->pty.final = final;
  pty->pty.set_winsize = set_winsize;
  pty->pty.write = write_to_pty;
  pty->pty.read = read_pty;

  if (set_winsize(&pty->pty, cols, rows, width_pix, height_pix) == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_set_pty_winsize() failed.\n");
#endif
  }

  if (keepalive_msec >= 1000) {
    libssh2_keepalive_config(pty->session->obj, 1, keepalive_msec / 1000);
  }

#ifdef USE_WIN32API
  if (!rd_ev) {
    HANDLE thrd;
    u_int tid;

    rd_ev = CreateEvent(NULL, FALSE, FALSE, "PTY_READ_READY");
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
      /* Launch the thread that wait for receiving data from pty. */
      if (!(thrd = _beginthreadex(NULL, 0, wait_pty_read, NULL, 0, &tid))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " CreateThread() failed.\n");
#endif

        goto error3;
      }

      CloseHandle(thrd);
    } else {
      /* java/MLTerm.java has already watched pty. */
    }
  }
#endif

  pty->session->pty_channels[pty->session->num_of_ptys++] = pty->channel;

  set_use_multi_thread(0);

  return 1;

error3:
  libssh2_session_set_blocking(pty->session->obj, 1); /* unblock in ssh_disconnect */
  libssh2_channel_free(pty->channel);

error2:
  ssh_disconnect(pty->session);
  set_use_multi_thread(0);

  return 0;
}

static void x11_callback(LIBSSH2_SESSION *session_obj, LIBSSH2_CHANNEL *channel, char *shost,
                         int sport, void **abstract) {
  u_int count;
  ssh_session_t *session;
  void *p;
  int display_sock = -1;
#ifdef USE_WIN32API
  struct sockaddr_in addr;
#else
  struct sockaddr_un addr;
#endif

  for (count = 0;; count++) {
    if (count == num_of_sessions) {
      /* XXX count must not reache num_of_sessions. */
      return;
    }

    if (session_obj == sessions[count]->obj) {
      session = sessions[count];
      break;
    }
  }

  if (!(p = realloc(session->x11_fds, (session->num_of_x11 + 1) * sizeof(int)))) {
    /* XXX channel resource is leaked. */
    return;
  }

  session->x11_fds = p;

  if (!(p = realloc(session->x11_channels,
                    (session->num_of_x11 + 1) * sizeof(LIBSSH2_CHANNEL *)))) {
    /* XXX channel resource is leaked. */
    return;
  }

  session->x11_channels = p;

  if (display_port == -1) {
    goto error;
  }

#ifdef USE_WIN32API
  if ((display_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    goto error;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(6000);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
  if ((display_sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    goto error;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/.X11-unix/X%d", display_port);
#endif

  if (connect(display_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
  error:
    bl_error_printf("Failed to connect X Server.\n");
    closesocket(display_sock);
    display_sock = -1;

    /* Don't call libssh2_channel_free() which causes segfault here. */
  } else {
#ifdef USE_WIN32API
    u_long val;

    val = 1;
    ioctlsocket(display_sock, FIONBIO, &val);
#else
    fcntl(display_sock, F_SETFL, O_NONBLOCK | fcntl(display_sock, F_GETFL, 0));
#endif
  }

  session->x11_channels[session->num_of_x11] = channel;
  session->x11_fds[session->num_of_x11++] = display_sock;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG
                  " x11 forwarding %d (display %d <=> ssh %p) started. => channel num %d\n",
                  session->num_of_x11 - 1, display_sock, channel, session->num_of_x11);
#endif
}

static int reconnect(vt_pty_ssh_t *pty) {
  ssh_session_t *session;
  vt_pty_ssh_t orig_pty;

  if (!(session = vt_search_ssh_session(pty->session->host,
                                        pty->session->port, pty->session->user)) ||
      session == pty->session) {
    char *host;
    host = pty->session->host;
    pty->session->host = "***dummy***";

#ifdef __DEBUG
    bl_debug_printf("Reconnect to %s@%s:%s pw %s pubkey %s privkey %s\n",
                    pty->session->user, host, pty->session->port, pty->session->stored->pass,
                    pty->session->stored->pubkey, pty->session->stored->privkey);
#endif

    bl_usleep(1000);

    if (!pty->session->stored ||
        !(session = ssh_connect(host, pty->session->port, pty->session->user,
                                pty->session->stored->pass, pty->session->stored->pubkey,
                                pty->session->stored->privkey))) {
      pty->session->host = host;

      return 0;
    }
    pty->session->host = host;
    session->stored = pty->session->stored;
    pty->session->stored = NULL;
  }

  orig_pty = *pty;
  memset(pty, 0, sizeof(*pty));
  pty->session = session;
  if (!open_channel(pty, session->stored->cmd_path, session->stored->argv, session->stored->env,
                    session->stored->cols, session->stored->rows, session->stored->width_pix,
                    session->stored->height_pix)) {
    *pty = orig_pty;
    return 0;
  }

  /* XXX See vt_pty_delete() */
  free(orig_pty.pty.buf);
  free(orig_pty.pty.cmd_line);
  final(&orig_pty.pty);

  return 1;
}

static void close_x11(ssh_session_t *session, int idx) {
  closesocket(session->x11_fds[idx]);
  while (libssh2_channel_free(session->x11_channels[idx]) == LIBSSH2_ERROR_EAGAIN)
    ;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG
                  " x11 forwarding %d (display %d <=> ssh %p) stopped. => channel num %d\n",
                  idx, session->x11_fds[idx], session->x11_channels[idx], session->num_of_x11 - 1);
#endif

  if (--session->num_of_x11 > 0) {
    session->x11_channels[idx] = session->x11_channels[session->num_of_x11];
    session->x11_fds[idx] = session->x11_fds[session->num_of_x11];
  }
}

static int xserver_to_ssh(LIBSSH2_CHANNEL *channel, int display) {
  char buf[8192];
  ssize_t len;

  while ((len = recv(display, buf, sizeof(buf), 0)) > 0) {
    ssize_t w_len;
    char *p;

    p = buf;
    while ((w_len = libssh2_channel_write(channel, p, len)) < len) {
      if (w_len > 0) {
        len -= w_len;
        p += w_len;
      } else if (w_len < 0) {
        if (libssh2_channel_eof(channel)) {
#if 0
          shutdown(display, SHUT_RDWR);
#endif

          return 0;
        }

        bl_usleep(1);
      }
    }

#if 0
    bl_debug_printf("X SERVER(%d) -> CHANNEL %d\n", display, len);
#endif
  }

  if (len == 0) {
    return 0;
  } else {
    return 1;
  }
}

static int ssh_to_xserver(LIBSSH2_CHANNEL *channel, int display) {
  char buf[8192];
  ssize_t len;

  while ((len = libssh2_channel_read(channel, buf, sizeof(buf))) > 0) {
    ssize_t w_len;
    char *p;

    p = buf;
    while ((w_len = send(display, p, len, 0)) < len) {
      if (w_len > 0) {
        len -= w_len;
        p += w_len;
      } else if (w_len < 0) {
        bl_usleep(1);
      }
    }

#if 0
    bl_debug_printf("CHANNEL -> X SERVER(%d) %d\n", display, len);
#endif
  }

  if (libssh2_channel_eof(channel)) {
#if 0
    shutdown(display, SHUT_RDWR);
#endif

    return 0;
  } else {
    return 1;
  }
}

/* cmd_path, cmd_argv, env, pubkey and privkey can be NULL. */
static void save_data_for_reconnect(ssh_session_t *session, const char *cmd_path, char **argv,
                                    char **env, const char *pass, const char *pubkey,
                                    const char *privkey, u_int cols, u_int rows,
                                    u_int width_pix, u_int height_pix) {
  size_t len;
  u_int array_size[2];
  int idx;
  char **src;

  len = sizeof(*session->stored);
  len += (strlen(pass) + 1);
  if (cmd_path) len += (strlen(cmd_path) + 1);
  if (pubkey) len += (strlen(pubkey) + 1);
  if (privkey) len += (strlen(privkey) + 1);

  for (src = argv, idx = 0; idx < 2; src = env, idx++) {
    if (src) {
      array_size[idx] = 1;
      for ( ; *src; src++) {
        array_size[idx]++;
        len += (strlen(*src) + 1 + sizeof(*src));
      }
      len += sizeof(*src);
    }
    else {
      array_size[idx] = 0;
    }
  }

  if ((session->stored = calloc(len, 1))) {
    char *str;
    char **dst;

    session->stored->argv = session->stored + 1;
    session->stored->env = session->stored->argv + array_size[0];
    str = session->stored->env + array_size[1];
    session->stored->pass = strcpy(str, pass);
    str += (strlen(pass) + 1);
    if (cmd_path) {
      session->stored->cmd_path = strcpy(str, cmd_path);
      str += (strlen(cmd_path) + 1);
    }
    if (pubkey) {
      session->stored->pubkey = strcpy(str, pubkey);
      str += (strlen(pubkey) + 1);
    }
    if (privkey) {
      session->stored->privkey = strcpy(str, privkey);
      str += (strlen(privkey) + 1);
    }

    for (src = argv, dst = session->stored->argv, idx = 0; idx < 2;
         src = env, dst = session->stored->env, idx++) {
      if (src) {
        for ( ; *src; src++) {
          *(dst++) = strcpy(str, *src);
          str += (strlen(str) + 1);
        }
        *dst = NULL;
      }
      else {
        if (idx == 0) {
          session->stored->argv = NULL;
        } else {
          session->stored->env = NULL;
        }
      }
    }

    session->stored->cols = cols;
    session->stored->rows = rows;
    session->stored->width_pix = width_pix;
    session->stored->height_pix = height_pix;
  }
}

/* --- global functions --- */

/*
 * Thread-safe.
 */
vt_pty_t *vt_pty_ssh_new(const char *cmd_path, /* can be NULL */
                         char **cmd_argv,      /* can be NULL(only if cmd_path is NULL) */
                         char **env,           /* can be NULL */
                         const char *uri, const char *pass, const char *pubkey, /* can be NULL */
                         const char *privkey,                                   /* can be NULL */
                         u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  vt_pty_ssh_t *pty;
  char *user;
  char *proto;
  char *host;
  char *port;

  if (!bl_parse_uri(&proto, &user, &host, &port, NULL, NULL, bl_str_alloca_dup(uri))) {
    return NULL;
  }

  /* USER: unix , USERNAME: win32 */
  if (!user && !(user = getenv("USER")) && !(user = getenv("USERNAME"))) {
    return NULL;
  }

  if (proto && strcmp(proto, "ssh") != 0) {
    return NULL;
  }

  if ((pty = calloc(1, sizeof(vt_pty_ssh_t))) == NULL) {
    return NULL;
  }

  if (!(pty->session = ssh_connect(host, port ? port : "22", user, pass, pubkey, privkey)) ||
      !open_channel(pty, cmd_path, cmd_argv, env, cols, rows, width_pix, height_pix)) {
    free(pty);

    return NULL;
  }

  if (auto_reconnect && ! pty->session->stored &&
      strcmp(host, "localhost") != 0 && strcmp(host, "127.0.0.1") != 0) {
    save_data_for_reconnect(pty->session, cmd_path, cmd_argv, env, pass, pubkey, privkey,
                            cols, rows, width_pix, height_pix);
  }

  return &pty->pty;
}

void *vt_search_ssh_session(const char *host, const char *port, /* can be NULL */
                            const char *user                    /* can be NULL */
                            ) {
  int count;

  /* USER: unix , USERNAME: win32 */
  if (!user && !(user = getenv("USER")) && !(user = getenv("USERNAME"))) {
    return NULL;
  }

  /* search from newer sessions. */
  for (count = num_of_sessions - 1; count >= 0; count--) {
    if (strcmp(sessions[count]->host, host) == 0 &&
        (port == NULL || strcmp(sessions[count]->port, port) == 0) &&
        strcmp(sessions[count]->user, user) == 0) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Find cached session for %s %s %s.\n", host, port, user);
#endif

      return sessions[count];
    }
  }

  return NULL;
}

int vt_pty_set_use_loopback(vt_pty_t *pty, int use) {
  if (use) {
    if (((vt_pty_ssh_t *)pty)->session->suspended) {
      return 0;
    } else {
      return use_loopback(pty);
    }
  } else {
    return unuse_loopback(pty);
  }
}

int vt_pty_ssh_scp_intern(vt_pty_t *pty, int src_is_remote, char *dst_path, char *src_path) {
  scp_t *scp;
  struct stat st;
  char *msg;

  /* Note that session is non-block mode in this context. */

  /* Check if pty is vt_pty_ssh_t or not. */
  if (pty->final != final) {
    return 0;
  }

  if (((vt_pty_ssh_t *)pty)->session->suspended) {
    bl_msg_printf("SCP: Another scp process is working.\n");

    return 0;
  }

  if (!(scp = malloc(sizeof(scp_t)))) {
    return 0;
  }
  scp->pty_ssh = (vt_pty_ssh_t *)pty;

  scp->pty_ssh->session->suspended = 1;

  if (src_is_remote) {
    while (!(scp->remote = libssh2_scp_recv(scp->pty_ssh->session->obj, src_path, &st)) &&
           libssh2_session_last_errno(scp->pty_ssh->session->obj) == LIBSSH2_ERROR_EAGAIN)
      ;
    if (!scp->remote) {
      bl_msg_printf("SCP: Failed to open remote:%s.\n", src_path);

      goto error;
    }

    if ((scp->local = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC
#ifdef USE_WIN32API
                                         | O_BINARY
#endif
                           ,
                           st.st_mode)) < 0) {
      bl_msg_printf("SCP: Failed to open local:%s.\n", dst_path);

      while (libssh2_channel_free(scp->remote) == LIBSSH2_ERROR_EAGAIN)
        ;

      goto error;
    }
  } else {
    if ((scp->local = open(src_path, O_RDONLY
#ifdef USE_WIN32API
                                         | O_BINARY
#endif
                           ,
                           0644)) < 0) {
      bl_msg_printf("SCP: Failed to open local:%s.\n", src_path);

      goto error;
    }

    fstat(scp->local, &st);

    while (!(scp->remote = libssh2_scp_send(scp->pty_ssh->session->obj, dst_path, st.st_mode & 0777,
                                            (u_long)st.st_size)) &&
           libssh2_session_last_errno(scp->pty_ssh->session->obj) == LIBSSH2_ERROR_EAGAIN)
      ;
    if (!scp->remote) {
      bl_msg_printf("SCP: Failed to open remote:%s.\n", dst_path);

      close(scp->local);

      goto error;
    }
  }

  scp->src_is_remote = src_is_remote;
  scp->src_size = st.st_size;

  if (!use_loopback(pty)) {
    while (libssh2_channel_free(scp->remote) == LIBSSH2_ERROR_EAGAIN)
      ;

    goto error;
  }

  if ((msg = alloca(24 + strlen(src_path) + strlen(dst_path) + 1))) {
    sprintf(msg, "\r\nSCP: %s%s => %s%s", src_is_remote ? "remote:" : "local:", src_path,
            src_is_remote ? "local:" : "remote:", dst_path);
    vt_write_to_pty(pty, msg, strlen(msg));
  }

#if defined(USE_WIN32API)
  {
    HANDLE thrd;
    u_int tid;

    if ((thrd = _beginthreadex(NULL, 0, scp_thread, scp, 0, &tid))) {
      CloseHandle(thrd);
    }
  }
#elif defined(HAVE_PTHREAD)
  {
    pthread_t thrd;

    pthread_create(&thrd, NULL, scp_thread, scp);
  }
#else
  scp_thread(scp);
#endif

  return 1;

error:
  scp->pty_ssh->session->suspended = 0;
  free(scp);

  return 0;
}

void vt_pty_ssh_set_cipher_list(const char *list) { cipher_list = list; }

void vt_pty_ssh_set_keepalive_interval(u_int interval_sec) {
  keepalive_msec_left = keepalive_msec = interval_sec * 1000;
}

int vt_pty_ssh_keepalive(u_int spent_msec) {
  if (keepalive_msec_left <= spent_msec) {
    u_int count;

    for (count = 0; count < num_of_sessions; count++) {
      libssh2_keepalive_send(sessions[count]->obj, NULL);
    }

    keepalive_msec_left = keepalive_msec;
  } else {
    keepalive_msec_left -= spent_msec;
  }

  return 1;
}

void vt_pty_ssh_set_use_x11_forwarding(void *session, int use) {
  if (session) {
    ((ssh_session_t *)session)->use_x11_forwarding = use;
  } else {
    use_x11_forwarding = use;
  }
}

int vt_pty_ssh_poll(void *p) {
  fd_set *fds;
  int num_of_fds;
  u_int count;

  fds = p;
  FD_ZERO(fds);
  num_of_fds = 0;

  for (count = 0; count < num_of_sessions; count++) {
    u_int idx;

    if (sessions[count]->suspended) {
      continue;
    }

    for (idx = 0; idx < sessions[count]->num_of_ptys; idx++) {
      if (libssh2_poll_channel_read(sessions[count]->pty_channels[idx], 0)) {
        goto found;
      }
    }

    for (idx = 0; idx < sessions[count]->num_of_x11; idx++) {
      if (libssh2_poll_channel_read(sessions[count]->x11_channels[idx], 0)) {
        goto found;
      }
    }

    continue;

  found:
    FD_SET(sessions[count]->sock, fds);
    num_of_fds++;
  }

  return num_of_fds;
}

/*
 * The returned fds can contain -1 which means the failure of x11_callback().
 */
u_int vt_pty_ssh_get_x11_fds(int **fds) {
  static int *x11_fds;
  static u_int num_of_x11_fds;
  u_int count;
  u_int num;

  if (num_of_sessions == 0) {
    return 0;
  }

  num = 0;
  for (count = 0; count < num_of_sessions; count++) {
    num += sessions[count]->num_of_x11;
  }

  if (num_of_x11_fds < num) {
    void *p;

    num_of_x11_fds = num;

    if (!(p = realloc(x11_fds, num * sizeof(int)))) {
      return 0;
    }

    x11_fds = p;
  }

  num = 0;
  for (count = 0; count < num_of_sessions; count++) {
    memcpy(x11_fds + num, sessions[count]->x11_fds, sessions[count]->num_of_x11 * sizeof(int));
    num += sessions[count]->num_of_x11;
  }

  *fds = x11_fds;

  return num;
}

int vt_pty_ssh_send_recv_x11(int idx, int bidirection) {
  u_int count;
  ssh_session_t *session;

  for (count = 0;; count++) {
    if (count >= num_of_sessions) {
      return 0;
    }

    if (idx < sessions[count]->num_of_x11) {
      break;
    }

    idx -= sessions[count]->num_of_x11;
  }

  session = sessions[count];

  if (session->suspended) {
    return 0;
  }

  if (session->x11_fds[idx] == -1 || /* Failed to connect X server */
      !((!bidirection || xserver_to_ssh(session->x11_channels[idx], session->x11_fds[idx])) &&
        ssh_to_xserver(session->x11_channels[idx], session->x11_fds[idx]))) {
    close_x11(session, idx);
  }

  return 1;
}

void vt_pty_ssh_set_use_auto_reconnect(int flag) {
  auto_reconnect = flag;
}
