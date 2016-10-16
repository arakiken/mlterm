/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <string.h> /* strncasecmp/strcasecmp/strdup/strchr/strrchr/memmove*/
#include <stdlib.h> /* free/malloc */
#include "comm.h"
#include "data.h"
#include "data_entry_string.h"

int entry_string_edit(window_t *window, entry_t *entry, int x, int y) {
  entry_string_t *data;
  int result;
  char *str_new = NULL;
  window_t *edit;
  data = entry->data;
  edit = window_new(x + 1, y, window->right, y + 2, 1, window); /*XXX*/
  result = string_edit(edit, data->current, &str_new);
  if (result && str_new) {
    free(data->current);
    data->current = str_new;
    entry->modified = 1;
  }
  return -1; /* finished */
}

void entry_string_reset(entry_t *entry) {
  entry_string_t *data = entry->data;
  if (data->initial) {
    mlterm_set_param(entry->key, data->initial);
  } else {
    mlterm_set_param(entry->key, "");
  }
}

void entry_string_apply(entry_t *entry) {
  entry_string_t *data = entry->data;
  if (data->current) {
    mlterm_set_param(entry->key, data->current);
    free(data->current);
  } else {
    mlterm_set_param(entry->key, "");
  }
  data->current = mlterm_get_param(entry->key);
  if (data->current) data->current = strdup(data->current);
}

entry_string_t *entry_string_new(const char *key) {
  entry_string_t *entry;

  entry = malloc(sizeof(entry_string_t));
  entry->initial = mlterm_get_param(key);
  if (entry->initial) {
    entry->initial = strdup(entry->initial);
    entry->current = strdup(entry->initial);
  } else {
    entry->initial = NULL;
    entry->current = NULL;
  }
  entry->max = -1;
  entry->min = 0;
  return entry;
}

int entry_string_add(section_t *section, const char *name, const char *key) {
  if (section->maxwidth < strlen(name)) section->maxwidth = strlen(name);
  section->entry[section->size].name = name;
  section->entry[section->size].key = key;
  section->entry[section->size].type = ENT_STRING;
  section->entry[section->size].data = entry_string_new(key);
  section->size++;
  return 0;
}

void entry_string_free_data(entry_t *entry) {
  entry_string_t *data;

  data = entry->data;
  free(data->initial);
  free(data->current);
  free(data);
  return;
}

void entry_string_display(window_t *window, entry_t *entry, int x, int y, int state) {
  entry_string_t *data;

  data = entry->data;
  if (data->current)
    display_str(window, x, y, data->current, DC_NORMAL);
  else
    display_str(window, x, y, "(error?)", DC_NORMAL);
}

int string_edit(window_t *window, char *src, char **result) {
  int input;
  int offset = 0;
  int cur_pos = 0;
  int width;
  char buffer[256]; /*XXX*/
  char *work;
  int flag = 1;

  width = window_width(window);
  work = malloc(width * (sizeof(char)));
  if (src) {
    strncpy(buffer, src, sizeof(buffer));
    buffer[255] = 0;
  } else
    buffer[0] = 0;

  cursor_show();
  while (1) {
    if (flag) {
      window_clear(window);
      strncpy(work, buffer + offset, width - 2);
      window_addstr(window, 0, 0, work);
      if (strlen(buffer) - offset < width - 2) {
        memset(work, ' ', width - (strlen(buffer) - offset) - 2);
        work[width - (strlen(buffer) - offset) - 2] = 0;
        window_addstr(window, width - strlen(work) - 2, 0, work);
      }
      set_cursor_pos(window, cur_pos - offset, 1);
      flush_stdout();
      flag = 0;
    }

    input = read_one();
    switch (input) {
      case KEY_BS: /* BS */
        if (cur_pos > 1) {
          memmove(buffer + cur_pos - 2, buffer + cur_pos - 1, strlen(buffer) - cur_pos + 2);
          cur_pos--;
        }
        flag = 1;
        break;
      case 10: /* ret */
        *result = strdup(buffer);
        cursor_hide();
        return 1;
      case KEY_ESC:
        cursor_hide();
        free(work);
        *result = NULL;
        return 0; /* discard */
      case KEY_DOWN:
      case KEY_RIGHT:
        if (cur_pos > strlen(buffer)) break;
        cur_pos++;
        flag = 1;
        if (cur_pos > offset + width - 2) offset++;
        break;
      case KEY_UP:
      case KEY_LEFT:
        if (cur_pos <= 0) break;
        cur_pos--;
        flag = 1;
        if (cur_pos < offset) offset--;
        break;
      case KEY_DEL: /* DEL */
        if ((cur_pos > 0) && (cur_pos <= strlen(buffer)))
          memmove(buffer + cur_pos - 1, buffer + cur_pos, strlen(buffer) - cur_pos + 2);
        flag = 1;
        break;
      case -1: /* ignore */
        break;
      default: /*discard non-printable chars?*/
        if (cur_pos > 0) {
          memmove(buffer + cur_pos, buffer + cur_pos - 1, strlen(buffer) - cur_pos + 2);
          buffer[cur_pos - 1] = input;
        } else {
          memmove(buffer + 1, buffer, strlen(buffer) + 2);
          buffer[0] = input;
        }
        cur_pos++;
        if (cur_pos > offset + width - 2) offset++;
        flag = 1;
    }
  }
  /* never reached */
  return 0;
}
