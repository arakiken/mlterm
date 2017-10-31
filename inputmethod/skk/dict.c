/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "dict.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>
#include <pobl/bl_path.h>
#include <pobl/bl_unistd.h> /* bl_usleep */
#include <mef/ef_eucjp_parser.h>
#include <mef/ef_eucjp_conv.h>
#include <mef/ef_utf8_parser.h>
#include <mef/ef_utf8_conv.h>

/* for serv_search() */
#include <pobl/bl_def.h> /* USE_WIN32API */
#ifdef USE_WIN32API
#include <winsock2.h>
#endif
#include <pobl/bl_net.h> /* getaddrinfo/socket/connect */
#include <errno.h>

#include <ui_im.h>

#include "ef_str_parser.h"
#include "../im_common.h"

#ifndef USE_WIN32API
#define closesocket(sock) close(sock)
#endif

#define MAX_TABLES 256 /* MUST BE 2^N (see calc_index()) */
#define MAX_CAPTIONS 100
#define MAX_CANDS 100
#define UTF_MAX_SIZE 6 /* see vt_char.h */

typedef struct candidate {
  char *caption;
  char *caption2;
  char *cands[MAX_CANDS];
  u_int num;
  u_int local_num;
  int cur_index;
  int checked_global_dict;

  ef_char_t *caption_orig;
  u_int caption_orig_len;

} candidate_t;

typedef struct completion {
  char *captions[MAX_CAPTIONS];
  u_int num;
  u_int local_num;
  int cur_index;
  int checked_global_dict;

  ef_char_t *caption_orig;
  u_int caption_orig_len;

  char *serv_response;

} completion_t;

typedef struct table {
  char **entries;
  u_int num;

} table_t;

/* --- static variables --- */

extern ui_im_export_syms_t *syms;
static char *global_dict;

/* --- static functions --- */

static int calc_index(char *p) {
  char *end;
  u_int idx;

  if (!(end = strchr(p, ' '))) {
    return -1;
  }

  if (p + 6 < end) {
    end = p + 6;
  }

  idx = 0;
  while (p < end) {
    idx += *(p++);
  }

  idx &= (MAX_TABLES - 1);

  return idx;
}

static u_char *make_entry(u_char *str) {
  static u_int16_t time = 1;
  size_t len;
  u_char *entry;

  len = strlen(str);

  if ((entry = malloc(len + 1 + 2))) {
    strcpy(entry, str);
    entry[len] = (time >> 8) & 0xff;
    entry[len + 1] = time & 0xff;
    time++;
  }

  return entry;
}

static u_int16_t get_entry_time(u_char *entry, char *data, size_t data_size) {
  if (entry < data || data + data_size <= entry) {
    size_t len;

    len = strlen(entry);

    return (entry[len] << 8) | entry[len + 1];
  } else {
    return 0;
  }
}

static void invalidate_entry(char *entry, char *data, size_t data_size) {
  char *p;

  if (data <= entry && entry < data + data_size) {
    if ((p = strchr(entry, ' ')) && p[1] == '/') {
      p[1] = 'X';
    }
  } else {
    free(entry);
  }
}

static int is_invalid_entry(char *entry) {
  char *p;

  if (!(p = strchr(entry, ' ')) || p[1] == 'X') {
    return 1;
  } else {
    return 0;
  }
}

static char *file_load(size_t *size, table_t *tables, char *path) {
  int fd;
  struct stat st;
  char *data;
  char *p;
  int count;
  u_int filled_nums[MAX_TABLES];

  fd = open(path, O_RDONLY, 0);
  free(path);

  if (fd < 0 || fstat(fd, &st) != 0 || st.st_size == 0 || !(data = malloc(st.st_size + 1))) {
    return NULL;
  }

  if (read(fd, data, st.st_size) != st.st_size) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Failed to read skk-jisyo.\n");
#endif

    free(data);
    data = NULL;

    return NULL;
  }
  data[st.st_size] = '\0';

  p = data;
  do {
    if (p[0] != ';' || p[1] != ';') {
      if ((count = calc_index(p)) != -1) {
        tables[count].num++;
      }
    }

    if (!(p = strchr(p, '\n'))) {
      break;
    }

    p++;
  } while (1);

  for (count = 0; count < MAX_TABLES; count++) {
    if (!(tables[count].entries = malloc(sizeof(*tables[0].entries) * tables[count].num))) {
      tables[count].num = 0;
    }
  }

  memset(filled_nums, 0, sizeof(filled_nums));
  p = data;
  do {
    if (p[0] != ';' || p[1] != ';') {
      if ((count = calc_index(p)) != -1) {
        tables[count].entries[filled_nums[count]++] = p;
      }
    }

    if (!(p = strchr(p, '\n'))) {
      break;
    }

    if (*(p - 1) == '\r') {
      /* CRLF */
      *(p - 1) = '\0';
      p++;
    } else {
      /* LF */
      *(p++) = '\0';
    }
  } while (1);

#if 0
  for (count = 0; count < MAX_TABLES; count++) {
    bl_debug_printf("%d %d\n", count, filled_nums[count]);
  }
#endif

  *size = st.st_size;

  return data;
}

static void file_unload(table_t *tables, char *data, size_t data_size, char *path) {
  u_int tbl_idx;
  u_int ent_idx;
  FILE *fp;
  char *p;

  if (path) {
    fp = fopen(path, data ? "w" : "a");
    free(path);
  } else {
    fp = NULL;
  }

  if (fp) {
    p = data;
    while (p < data + data_size) {
      if (!is_invalid_entry(p)) {
        fprintf(fp, "%s\n", p);
      }

      p += (strlen(p) + 1);
    }
  }

  for (tbl_idx = 0; tbl_idx < MAX_TABLES; tbl_idx++) {
    for (ent_idx = 0; ent_idx < tables[tbl_idx].num; ent_idx++) {
      if (tables[tbl_idx].entries[ent_idx] < data ||
          data + data_size <= tables[tbl_idx].entries[ent_idx]) {
#ifdef DEBUG
        bl_debug_printf("Free %s\n", tables[tbl_idx].entries[ent_idx]);
#endif

        if (fp) {
          fprintf(fp, "%s\n", tables[tbl_idx].entries[ent_idx]);
        }

        free(tables[tbl_idx].entries[ent_idx]);
      }
    }

    free(tables[tbl_idx].entries);
    tables[tbl_idx].num = 0;
  }

  if (fp) {
    fclose(fp);
  }
}

static size_t ef_str_to(char *dst, size_t dst_len, ef_char_t *src, u_int src_len,
                         ef_conv_t *conv) {
  (*conv->init)(conv);

  return (*conv->convert)(conv, dst, dst_len, ef_str_parser_init(src, src_len));
}

static u_int file_get_completion_list(char **list, u_int list_size, table_t *tables,
                                      ef_conv_t *dic_conv, ef_char_t *caption, u_int len) {
  char buf[1024];
  int tbl_idx;
  int ent_idx;
  u_int num;

  len = ef_str_to(buf, sizeof(buf) - 2, caption, len, dic_conv);

  num = 0;
  for (tbl_idx = 0; tbl_idx < MAX_TABLES; tbl_idx++) {
    for (ent_idx = 0; ent_idx < tables[tbl_idx].num; ent_idx++) {
      if (strncmp(tables[tbl_idx].entries[ent_idx], buf, len) == 0) {
        list[num++] = tables[tbl_idx].entries[ent_idx];

        if (num >= list_size) {
          return num;
        }
      }
    }
  }

  return num;
}

static u_int serv_get_completion_list(char **list, u_int list_size, int sock, ef_conv_t *dic_conv,
                                      ef_char_t *caption, u_int caption_len) {
  char buf[4096];
  char *p;
  size_t filled_len;
  int num;

  buf[0] = '4';
  filled_len = ef_str_to(buf + 1, sizeof(buf) - 3, caption, caption_len, dic_conv);
  buf[1 + filled_len] = ' ';
  buf[1 + filled_len + 1] = '\n';
  send(sock, buf, filled_len + 3, 0);
#ifndef USE_WIN32API
  fsync(sock);
#endif
#ifdef DEBUG
  write(0, buf, filled_len + 3);
#endif

  p = buf;
  if (recv(sock, p, 1, 0) != 1) {
    return 0;
  }

  p++;
  while (p < buf + sizeof(buf) && recv(sock, p, 1, 0) == 1 && *p != '\n') {
    p++;
  }
  *p = '\0';

  if (*buf != '1') {
    return 0;
  }

#ifdef DEBUG
  write(0, buf, p - buf);
  write(0, "\n", 1);
#endif

  p = strdup(buf + 2); /* skip "1/" */
  num = 0;
  list[num++] = p;
  while ((p = strchr(p, '/'))) {
    *(p++) = '\0';

    list[num++] = p;

    if (num >= list_size) {
      break;
    }
  }

  return num - 1; /* -1: last is removed */
}

static u_int completion_remove_duplication(char **captions, u_int local_num, u_int num,
                                           ef_parser_t *global_parser, ef_conv_t *local_conv) {
  u_int count;
  u_int count2;
  char buf[1024];
  size_t len;
  char *p;

  for (count = local_num; count < num;) {
    if ((p = strchr(captions[count], ' '))) {
      len = p - captions[count];
    } else {
      len = strlen(captions[count]);
    }

    (*global_parser->init)(global_parser);
    (*global_parser->set_str)(global_parser, captions[count], len);

    (*local_conv->init)(local_conv);
    len = (*local_conv->convert)(local_conv, buf, sizeof(buf) - 1, global_parser);
    buf[len] = '\0';

    for (count2 = 0; count2 < local_num; count2++) {
      if (strncmp(captions[count2], buf, len) == 0) {
        memmove(captions + count, captions + count + 1, sizeof(*captions) * (--num - count));
        continue;
      }
    }

    count++;
  }

  return num;
}

static char *file_search(table_t *tables, ef_conv_t *dic_conv, ef_parser_t *dic_parser,
                         ef_char_t *caption, u_int caption_len) {
  int idx;
  u_int count;
  char buf[1024];
  size_t filled_len;

  filled_len = ef_str_to(buf, sizeof(buf) - 2, caption, caption_len, dic_conv);
  buf[filled_len] = ' ';
  buf[filled_len + 1] = '\0';
#ifdef DEBUG
  bl_debug_printf("Searching %s\n", buf);
#endif

  idx = calc_index(buf);

  for (count = 0; count < tables[idx].num; count++) {
    if (strncmp(buf, tables[idx].entries[count], filled_len + 1) == 0) {
      strcpy(buf + filled_len + 1, tables[idx].entries[count] + filled_len + 1);

      return strdup(buf);
    }
  }

  return NULL;
}

static char *serv_search(int sock, ef_conv_t *dic_conv, ef_parser_t *dic_parser,
                         ef_char_t *caption, u_int caption_len) {
  char buf[1024];
  char *p;
  size_t filled_len;

  buf[0] = '1';
  filled_len = ef_str_to(buf + 1, sizeof(buf) - 3, caption, caption_len, dic_conv);
  buf[1 + filled_len] = ' ';
  buf[1 + filled_len + 1] = '\n';
  send(sock, buf, filled_len + 3, 0);
#ifndef USE_WIN32API
  fsync(sock);
#endif
#ifdef DEBUG
  write(0, buf, filled_len + 3);
#endif

  p = buf;
  if (recv(sock, p, 1, 0) != 1) {
    return 0;
  }

  p += (1 + filled_len + 1); /* skip caption */
  while (p < buf + sizeof(buf) && recv(sock, p, 1, 0) == 1 && *p != '\n') {
    p++;
  }
  *p = '\0';

  if (*buf != '1') {
    return 0;
  }

#ifdef DEBUG
  write(0, buf, p - buf);
  write(0, "\n", 1);
#endif

  return strdup(buf + 1);
}

static void set_blocking(int fd, int nonblock) {
/* Non blocking */
#ifdef USE_WIN32API
  u_long val = nonblock;

  ioctlsocket(fd, FIONBIO, &val);
#else
  if (nonblock) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  } else {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
  }
#endif
}

static int check_protocol_4(int sock) {
  char msg[] = "4ab \n";
  char p;
  int count;

  set_blocking(sock, 0);
  send(sock, msg, sizeof(msg) - 1, 0);
#ifndef USE_WIN32API
  fsync(sock);
#endif

  set_blocking(sock, 1);
  count = 0;
  while (1) {
    if (recv(sock, &p, 1, 0) == 1) {
      if (p == '\n') {
        break;
      }
    } else if (errno == EAGAIN) {
      if (++count == 10) {
        break;
      }

      bl_usleep(1000);
    }
  }

  set_blocking(sock, 0);

  return (count != 10);
}

static int connect_to_server(void) {
  char *serv;
  int port;
  struct sockaddr_in sa;
  struct hostent *host;
  int sock;

  port = 1178;
  if (global_dict && *global_dict) {
    char *p;

    if ((p = alloca(strlen(global_dict) + 1))) {
      char *port_str;

      strcpy(p, global_dict);

      if (bl_parse_uri(NULL, NULL, &serv, &port_str, NULL, NULL, p) && port_str) {
        port = atoi(port_str);
      }
    } else {
      return -1;
    }
  } else {
    serv = "localhost";
  }

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    return -1;
  }

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);

  if (!(host = gethostbyname(serv))) {
    goto error;
  }

  memcpy(&sa.sin_addr, host->h_addr_list[0], sizeof(sa.sin_addr));

  if (connect(sock, &sa, sizeof(struct sockaddr_in)) == -1) {
    goto error;
  }

  return sock;

error:
  closesocket(sock);

  return -1;
}

static void unconcat(char *str) {
  char *p;

  if ((p = strstr(str, "(concat")) && (p = strchr(p, '\"'))) {
    char *end;
    size_t len;

    len = 0;
    while ((end = strchr(++p, '\"'))) {
      strncpy(str + len, p, end - p);
      len += (end - p);

      if (!(p = strchr(end + 1, '\"'))) {
        break;
      }
    }
    str[len] = '\0';

    p = str;
    while (*p) {
      if (*p == '\\') {
        u_int ch;

        if ('0' <= *(p + 1) && *(p + 1) <= '9') {
          ch = strtol(p + 1, &end, 8);
        } else if (*(p + 1) == 'x' && '0' <= *(p + 2) && *(p + 2) <= '9') {
          ch = strtol(p + 2, &end, 16);
        } else {
          p++;

          continue;
        }

        if (ch <= 0xff) {
          *(p++) = ch;
        }

        if (*end) {
          memmove(p, end, strlen(end) + 1);
        } else {
          *p = '\0';
          break;
        }
      } else {
        p++;
      }
    }
  }
}

static int candidate_exists(const char **cands, u_int num_of_cands, const char *cand) {
  u_int count;

  for (count = 0; count < num_of_cands; count++) {
    if (strcmp(cands[count], cand) == 0) {
      return 1;
    }
  }

  return 0;
}

static u_int candidate_string_to_array(candidate_t *cand, char *str) {
  u_int num;
  char *p;
  char *p2;

  if (cand->caption) {
    cand->caption2 = str;
  } else {
    cand->caption = str;
  }

  num = cand->num;
  p = strchr(str, ' '); /* skip caption */
  *p = '\0';
  p += 2; /* skip ' /' */

  while (*p) {
    /* skip [] */
    if (*p == '[') {
      if ((p2 = strstr(p + 1, "]/"))) {
        p = p2 + 2;

        continue;
      }
    }

    cand->cands[num] = p;
    if ((p = strchr(p, '/'))) {
      *(p++) = '\0';
    }
    if ((p2 = strchr(cand->cands[num], ';'))) {
      /* Remove comment */
      *p2 = '\0';
    }

    unconcat(cand->cands[num]);

    if (!candidate_exists(cand->cands, num, cand->cands[num])) {
      num++;
    }

    if (!p || num >= MAX_CANDS - 1) {
      break;
    }
  }

  return num - cand->num;
}

/* --- static variables --- */

static table_t global_tables[MAX_TABLES];
static char *global_data;
static size_t global_data_size;
static int global_sock = -1;
static int global_is_loaded = 0;
static int server_supports_protocol_4 = 0;
static ef_conv_t *global_conv;
static ef_parser_t *global_parser;

static table_t local_tables[MAX_TABLES];
static char *local_data;
static size_t local_data_size;
static ef_conv_t *local_conv;
static ef_parser_t *local_parser;

/* --- static variables --- */

static int global_dict_load(void) {
  if (!global_conv) {
    global_conv = (*syms->vt_char_encoding_conv_new)(VT_EUCJP);
    global_parser = (*syms->vt_char_encoding_parser_new)(VT_EUCJP);
  }

  if (!global_is_loaded && !global_data && global_sock == -1) {
    char *path;

    global_is_loaded = 1;

    if (global_dict && (path = strdup(global_dict))) {
      global_data = file_load(&global_data_size, global_tables, path);
    }

    if (!global_data) {
      if ((global_sock = connect_to_server()) != -1) {
        server_supports_protocol_4 = check_protocol_4(global_sock);
      }
    }
  }

  if (global_data) {
    return 1;
  } else if (global_sock != -1) {
    return 2;
  } else {
    return 0;
  }
}

static void local_dict_load(void) {
  static int is_loaded;
  char *path;

  if (!local_conv) {
    local_conv = (*syms->vt_char_encoding_conv_new)(VT_UTF8);
    local_parser = (*syms->vt_char_encoding_parser_new)(VT_UTF8);
  }

  if (!is_loaded && !local_data && (path = bl_get_user_rc_path("mlterm/skk-jisyo"))) {
    is_loaded = 1;

    local_data = file_load(&local_data_size, local_tables, path);
  }
}

static int dict_add_to_local(char *caption, size_t caption_len, char *word, size_t word_len) {
  int idx;
  u_int count;
  void *p;

  idx = calc_index(caption);
  for (count = 0; count < local_tables[idx].num; count++) {
    if (strncmp(caption, local_tables[idx].entries[count], caption_len) == 0) {
      char *tmp;
      char *p1;
      char *p2;
      char *p3;

      p1 = local_tables[idx].entries[count];

      if (!(tmp = alloca(strlen(p1) + word_len + 1))) {
        return 0;
      }

#ifdef DEBUG
      bl_debug_printf("Adding to dictionary: %s to %s\n", word, p1);
#endif

      p2 = p1 + caption_len;
      if (*p2 == '/') {
        p2++;
      }

      memcpy(tmp, p1, p2 - p1);

      strcpy(tmp + (p2 - p1), word);

      if ((p3 = strstr(p2, word)) && *(p3 - 1) == '/') {
        if (p3 > p2) {
          tmp[strlen(tmp) + p3 - p2] = '\0';
          memcpy(tmp + strlen(tmp), p2, p3 - p2);
        }

        p3 += word_len;
      } else {
        p3 = p2;
      }

      strcpy(tmp + strlen(tmp), p3);

      if (strcmp(tmp, local_tables[idx].entries[count]) != 0) {
        invalidate_entry(local_tables[idx].entries[count], local_data, local_data_size);
        local_tables[idx].entries[count] = make_entry(tmp);
      }

      return 1;
    }
  }

  if ((p = realloc(local_tables[idx].entries,
                   sizeof(*local_tables[idx].entries) * (local_tables[idx].num + 1)))) {
    local_tables[idx].entries = p;

    if ((p = alloca(strlen(caption) + 1 + 1 + strlen(word) + 1 + 1))) {
      sprintf(p, "%s/%s", caption, word);
      local_tables[idx].entries[local_tables[idx].num++] = make_entry(p);
#ifdef DEBUG
      bl_debug_printf("Adding to dictionary: %s\n", p);
#endif
    }
  }

  return 0;
}

static int dict_add_to_local_with_concat(char *caption, char *word) {
  u_int num;
  size_t caption_len;
  size_t word_len;

  num = bl_count_char_in_str(word, '/');
  num += bl_count_char_in_str(word, ';');

  if (num > 0) {
    char *new;

    /*
     * (concat "\057")
     * 123456789    ab => 11
     */
    if ((new = alloca(11 + 4 * num + (strlen(word) - num) + 1))) {
      char *src;
      char *dst;
      char *p;

      dst = strcpy(new, "(concat \"") + 9;
      src = word;
      while ((p = strchr(src, '/')) || (p = strchr(src, ';'))) {
        memcpy(dst, src, p - src);
        dst += (p - src);
        if (*p == '/') {
          strcpy(dst, "\\057");
        } else {
          strcpy(dst, "\\073");
        }
        dst += 4;
        src = p + 1;
      }
      strcpy(dst, src);
      strcat(dst, "\")");

      word = new;
    } else {
      return 0;
    }
  }

  caption_len = strlen(caption);
  word_len = strlen(word);

  caption[caption_len++] = ' ';
  caption[caption_len] = '\0';
  word[word_len++] = '/';
  word[word_len] = '\0';

  return dict_add_to_local(caption, caption_len, word, word_len);
}

/* --- global functions --- */

void dict_final(void) {
  file_unload(local_tables, local_data, local_data_size, bl_get_user_rc_path("mlterm/skk-jisyo"));
  free(local_data);
  local_data = NULL;

  if (local_conv) {
    (*local_conv->delete)(local_conv);
    (*local_parser->delete)(local_parser);
  }

  if (global_data) {
    file_unload(global_tables, global_data, global_data_size, NULL);
    free(global_data);
    global_data = NULL;
  } else {
    closesocket(global_sock);
    global_sock = -1;
  }

  if (global_conv) {
    (*global_conv->delete)(global_conv);
    (*global_parser->delete)(global_parser);
  }

  free(global_dict);
  global_dict = NULL;
}

u_int dict_completion(ef_char_t *caption, u_int caption_len, void **aux, int step) {
  completion_t * compl;
  int load_global_dict = 0;
  int move_index;
  u_int count;
  u_int max_time;
  char *str;
  char *end;
  ef_parser_t *parser;

  if (!*aux) {
    if (!(*aux = compl = calloc(1, sizeof(completion_t) + sizeof(*caption) * caption_len))) {
      return caption_len;
    }

    compl->caption_orig = (char *)(compl + 1);
    memcpy(compl->caption_orig, caption, sizeof(*caption) * caption_len);
    compl->caption_orig_len = caption_len;

    local_dict_load();
    compl->local_num = compl->num = file_get_completion_list(
        compl->captions, MAX_CAPTIONS, local_tables, local_conv, caption, caption_len);

    if (compl->num == 0) {
      load_global_dict = 1;
    }

    move_index = 0;
  } else {
    compl = *aux;

    if (compl->cur_index + step < 0 || compl->num <= compl->cur_index + step) {
      load_global_dict = 1;
    }

    move_index = 1;
  }

  if (load_global_dict) {
    if (!compl->checked_global_dict) {
      u_int num;

      switch (global_dict_load()) {
        case 1:
          num = file_get_completion_list(compl->captions + compl->num, MAX_CAPTIONS - compl->num,
                                         global_tables, global_conv, compl->caption_orig,
                                         compl->caption_orig_len);
          break;

        case 2:
          if (!server_supports_protocol_4) {
            num = 0;
          } else {
            num = serv_get_completion_list(compl->captions + compl->num, MAX_CAPTIONS - compl->num,
                                           global_sock, global_conv, compl->caption_orig,
                                           compl->caption_orig_len);
            compl->serv_response = compl->captions[compl->num];
          }
          break;

        default:
          num = 0;
      }

      if ((compl->num += num) == 0) {
        return caption_len;
      }

      if (num > 0) {
        compl->num = completion_remove_duplication(compl->captions, compl->local_num, compl->num,
                                                   global_parser, local_conv);
      }

      compl->checked_global_dict = 1;
    } else if (compl->num == 0) {
      return caption_len;
    }
  }

  if (move_index) {
    compl->cur_index += step;

    while (compl->cur_index < 0) {
      compl->cur_index += compl->num;
    }

    while (compl->cur_index >= compl->num) {
      compl->cur_index -= compl->num;
    }
  }

  max_time = 0;
  for (count = compl->cur_index; count < compl->num; count++) {
    u_int time;

    if (count < compl->local_num &&
        (time = get_entry_time(compl->captions[count], local_data, local_data_size)) > max_time) {
      char *tmp;

      tmp = compl->captions[compl->cur_index];
      compl->captions[compl->cur_index] = compl->captions[count];
      compl->captions[count] = tmp;

      max_time = time;
    }
  }

  str = compl->captions[compl->cur_index];

  if (compl->cur_index < compl->local_num) {
    parser = local_parser;
  } else {
    parser = global_parser;
  }

  (*parser->init)(parser);
  (*parser->set_str)(parser, str, (end = strchr(str, ' ')) ? end - str : strlen(str));
  for (count = 0; count < MAX_CAPTIONS && (*parser->next_char)(parser, caption + count); count++)
    ;

  return count;
}

void dict_completion_finish(void **aux) {
  if (global_sock != -1) {
    free(((completion_t *)*aux)->serv_response);
  }

  free(*aux);
  *aux = NULL;
}

u_int dict_completion_reset_and_finish(ef_char_t *caption, void **aux) {
  completion_t * compl;
  u_int len;

  compl = *aux;
  memcpy(caption, compl->caption_orig, compl->caption_orig_len * sizeof(*caption));

  len = compl->caption_orig_len;

  dict_completion_finish(aux);

  return len;
}

u_int dict_candidate(ef_char_t *caption, u_int caption_len, void **aux, int step) {
  candidate_t *cand;
  char *str;
  int load_global_dict = 0;
  int move_index;
  u_int count;
  u_int max_time;
  ef_parser_t *parser;

  if (!*aux) {
    if (!(*aux = cand = calloc(1, sizeof(candidate_t) + sizeof(*caption) * caption_len))) {
      return caption_len;
    }

    cand->caption_orig = (char *)(cand + 1);
    memcpy(cand->caption_orig, caption, sizeof(*caption) * caption_len);
    cand->caption_orig_len = caption_len;

    local_dict_load();
    if ((str = file_search(local_tables, local_conv, local_parser, caption, caption_len))) {
      cand->local_num = cand->num = candidate_string_to_array(cand, str);
    }

    if (cand->num < CAND_UNIT) {
      load_global_dict = 1;
    }

    move_index = 0;
  } else {
    cand = *aux;

    if (cand->cur_index + step < 0 ||
        cand->num <= (cand->cur_index + step + CAND_UNIT) / CAND_UNIT * CAND_UNIT - 1) {
      load_global_dict = 1;
    }

    move_index = 1;
  }

  if (load_global_dict) {
    if (!cand->checked_global_dict) {
      u_int num;

      switch (global_dict_load()) {
        case 1:
          str = file_search(global_tables, global_conv, global_parser, cand->caption_orig,
                            cand->caption_orig_len);
          break;

        case 2:
          str = serv_search(global_sock, global_conv, global_parser, cand->caption_orig,
                            cand->caption_orig_len);
          break;

        default:
          str = NULL;
      }

      if (str) {
        num = candidate_string_to_array(cand, str);
      } else {
        num = 0;
      }

      if ((cand->num += num) == 0) {
        goto error;
      }

      cand->checked_global_dict = 1;
    } else if (cand->num == 0) {
      goto error;
    }
  }

  if (move_index) {
    cand->cur_index += step;

    while (cand->cur_index < 0) {
      cand->cur_index += cand->num;
    }

    while (cand->cur_index >= cand->num) {
      cand->cur_index -= cand->num;
    }
  }

  max_time = 0;
  for (count = cand->cur_index; count < cand->num; count++) {
    u_int time;

    if (count < cand->local_num &&
        (time = get_entry_time(cand->cands[count], local_data, local_data_size)) > max_time) {
      char *tmp;

      tmp = cand->cands[cand->cur_index];
      cand->cands[cand->cur_index] = cand->cands[count];
      cand->cands[count] = tmp;

      max_time = time;
    }
  }

  if (cand->cur_index < cand->local_num) {
    parser = local_parser;
  } else {
    parser = global_parser;
  }

  (*parser->init)(parser);
  (*parser->set_str)(parser, cand->cands[cand->cur_index], strlen(cand->cands[cand->cur_index]));

  for (count = 0; count < MAX_CANDS && (*parser->next_char)(parser, caption + count); count++)
    ;

  return count;

error:
  free(*aux);
  *aux = NULL;

  return caption_len;
}

void dict_candidate_get_state(void *aux, int *cur, u_int *num) {
  *cur = ((candidate_t *)aux)->cur_index;
  *num = ((candidate_t *)aux)->num;
}

void dict_candidate_get_list(void *aux, char *list, size_t list_size, ef_conv_t *conv) {
  candidate_t *cand;
  ef_parser_t *parser;
  char *dst;
  u_int count;
  int start_index;

  cand = aux;

  start_index = cand->cur_index - (cand->cur_index % CAND_UNIT);
  dst = list;
  for (count = 0; count < CAND_UNIT && start_index + count < cand->num; count++) {
    if (dst + MAX_DIGIT_NUM + 1 - list > list_size) {
      break;
    }
    sprintf(dst, "%d ", start_index + count + 1);
    dst += strlen(dst);

    if (start_index + count >= cand->local_num) {
      parser = global_parser;
    } else {
      parser = local_parser;
    }

    (*parser->init)(parser);
    (*parser->set_str)(parser, cand->cands[start_index + count],
                       strlen(cand->cands[start_index + count]));
    (*conv->init)(conv);
    dst += (*conv->convert)(conv, dst, list_size - (dst - list) - 2, parser);
    *(dst++) = ' ';
    *dst = '\0';
  }
  *(dst - 1) = '\0';
}

void dict_candidate_finish(void **aux) {
  candidate_t *cand;

  cand = *aux;

  free(cand->caption);
  free(cand->caption2);
  free(cand);

  *aux = NULL;
}

u_int dict_candidate_reset_and_finish(ef_char_t *caption, void **aux) {
  candidate_t *cand;
  u_int len;

  cand = *aux;
  memcpy(caption, cand->caption_orig, cand->caption_orig_len * sizeof(*caption));

  len = cand->caption_orig_len;

  dict_candidate_finish(aux);

  return len;
}

int dict_add_new_word_to_local(ef_char_t *_caption, u_int _caption_len, ef_char_t *_word,
                               u_int _word_len) {
  char caption[1024];
  char word[1024];

  /* -2: ' ' is appended in dict_add_to_local_with_concat() */
  caption[ef_str_to(caption, sizeof(caption) - 2, _caption, _caption_len, local_conv)] = '\0';

  /* -2: '/' is appended in dict_add_to_local_with_concat() */
  word[ef_str_to(word, sizeof(word) - 2, _word, _word_len, local_conv)] = '\0';

  return dict_add_to_local_with_concat(caption, word);
}

int dict_candidate_add_to_local(void *aux) {
  char caption[1024];
  char word[1024];
  candidate_t *cand;
  ef_parser_t *parser;

  cand = aux;

  /* -2: ' ' is appended in dict_add_to_local_with_concat() */
  caption[ef_str_to(caption, sizeof(caption) - 2, cand->caption_orig, cand->caption_orig_len,
                     local_conv)] = '\0';

  if (cand->cur_index < cand->local_num) {
    parser = local_parser;
  } else {
    parser = global_parser;
  }

  (*parser->init)(parser);
  (*parser->set_str)(parser, cand->cands[cand->cur_index], strlen(cand->cands[cand->cur_index]));
  (*local_conv->init)(local_conv);
  /* -2: '/' is appended in dict_add_to_local_with_concat() */
  word[ (*local_conv->convert)(local_conv, word, sizeof(word) - 2, parser)] = '\0';

  return dict_add_to_local_with_concat(caption, word);
}

void dict_set_global(char *dict) {
  size_t len;

  if (global_dict) {
    if (strcmp(dict, global_dict) == 0) {
      return;
    }

    free(global_dict);
    global_dict = strdup(dict);
  }

  if (global_data) {
    file_unload(global_tables, global_data, global_data_size, NULL);
    free(global_data);
    global_data = NULL;
  }

  if (global_sock != -1) {
    closesocket(global_sock);
    global_sock = -1;
  }

  if (global_conv) {
    (*global_conv->delete)(global_conv);
    (*global_parser->delete)(global_parser);
  }

  if ((len = strlen(dict)) > 5 && strcmp(dict + len - 5, ":utf8") == 0) {
    global_conv = (*syms->vt_char_encoding_conv_new)(VT_UTF8);
    global_parser = (*syms->vt_char_encoding_parser_new)(VT_UTF8);

    global_dict[len - 5] = '\0';
  } else {
    global_conv = NULL;
    global_parser = NULL;
  }
}
