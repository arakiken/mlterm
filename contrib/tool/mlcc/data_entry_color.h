typedef struct {
	char * initial;
	char * current;
} entry_color_t;

entry_color_t *entry_color_new(const char *key);
int entry_color_add(section_t *section, const char *name, const char * key);
void entry_color_free_data(entry_t * entry);
void entry_color_apply(entry_t *entry);
void entry_color_reset(entry_t *entry);
void entry_color_display(window_t *window, entry_t *entry, int x, int y, int state);
int entry_color_edit(window_t *window, entry_t *entry, int x, int y);

