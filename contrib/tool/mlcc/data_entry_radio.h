typedef struct {
  int initial;
  int current;
  int num;
  char **term;
} entry_radio_t;

entry_radio_t *entry_radio_new(const char *key, const char *terms);
int entry_radio_add(section_t *section, const char *name, const char *key, const char *terms);
void entry_radio_free_data(entry_t *entry);
void entry_radio_apply(entry_t *entry);
void entry_radio_reset(entry_t *entry);
void entry_radio_display(window_t *window, entry_t *entry, int x, int y, int state);
int entry_radio_edit(window_t *window, entry_t *entry, int x, int y);
