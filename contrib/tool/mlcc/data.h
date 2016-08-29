#ifndef _DATA_H_

#define _DATA_H_

#define MAX_ENTRY 8

/*
 * typedefs
 */
typedef enum { ENT_NONE, ENT_STRING, ENT_NUMERIC, ENT_COLOR, ENT_BOOL, ENT_RADIO } entry_type_t;

typedef struct {
  const char *name;
  const char *key;
  int modified;
  entry_type_t type;
  void *data;
} entry_t;

typedef struct {
  const char *name;
  int size;
  int selected;
  int modified;
  int maxwidth;
  entry_t entry[MAX_ENTRY];
} section_t;

typedef enum { DS_CANCEL, DS_SELECT, DS_EDIT } display_state_t;

int entry_free(entry_t *entry);
int entry_edit(window_t *window, entry_t *entry, int x, int y);
int entry_apply(entry_t *entry);
int entry_display(window_t *window, entry_t *entry, int x, int y, int state);
int entry_reset(entry_t *entry);
window_t *entry_window_new(window_t *section);

void section_free(section_t *section);
#endif
