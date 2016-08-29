typedef struct {
  char *initial;
  char *current;
  int max;
  int min;
} entry_string_t;

entry_string_t *entry_string_new(const char *key);
int entry_string_add(section_t *section, const char *name, const char *key);
void entry_string_free_data(entry_t *entry);
void entry_string_apply(entry_t *entry);
void entry_string_reset(entry_t *entry);
void entry_string_display(window_t *window, entry_t *entry, int x, int y, int state);
int entry_string_edit(window_t *window, entry_t *entry, int x, int y);
