typedef struct {
	int initial;
	int current;
	int max;
	int min;
	const char *unit;
} entry_numeric_t;

entry_numeric_t *entry_numeric_new(const char *key, int max, int min, const char *unit);
int entry_numeric_add(section_t *section, const char *name, const char * key, int min, int max, const char *unit);
void entry_numeric_free_data(entry_t * entry);
void entry_numeric_apply(entry_t *entry);
void entry_numeric_reset(entry_t *entry);
void entry_numeric_display(window_t *window, entry_t *entry, int x, int y, int state);
int entry_numeric_edit(window_t *window, entry_t *entry, int x, int y);
