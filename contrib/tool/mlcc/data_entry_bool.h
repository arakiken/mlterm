typedef struct {
	int initial;
	int current;
} entry_bool_t;

entry_bool_t *entry_bool_new(const char *key);
int entry_bool_add(section_t *section, const char *name, const char * key);
void entry_bool_free_data(entry_t * entry);
void entry_bool_apply(const entry_t *entry);
void entry_bool_reset(const entry_t *entry);
void entry_bool_display(window_t *window, const entry_t *entry, int x, int y, int state);
int entry_bool_edit(window_t *window, entry_t *entry, int x, int y);

