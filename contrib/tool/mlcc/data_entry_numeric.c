#include "config.h"
#include <string.h> /* strncasecmp/strcasecmp/strdup/strchr/strrchr/memmove*/
#include <stdlib.h> /* free/malloc */
#include "comm.h"
#include "data.h"
#include "data_entry_numeric.h"

void entry_numeric_reset(entry_t *entry){
	entry_numeric_t * data = entry->data;
	mlterm_set_value(entry->key, data->initial);
}

void entry_numeric_apply(entry_t *entry){
	entry_numeric_t * data = entry->data;
	mlterm_set_value(entry->key, data->current);
}

entry_numeric_t *entry_numeric_new(const char *key, int max, int min, const char *unit){
	entry_numeric_t *entry;
	char * tmp;

	tmp = mlterm_get_param(key);
	entry = malloc(sizeof(entry_numeric_t));
	if(tmp)
		entry->initial = atoi(strdup(tmp));
	else
		entry->initial = 0;
	entry->current = entry->initial;
	entry->unit = unit;
	entry->max = max;
	entry->min = min;
	return entry;
}

int entry_numeric_add(section_t *section, const char *name, const char * key, int min, int max, const char *unit){
	if (section->maxwidth < strlen(name))
		section->maxwidth = strlen(name);
	section->entry[section->size].name = name;
	section->entry[section->size].key = key;
	section->entry[section->size].type = ENT_NUMERIC;
	section->entry[section->size].data = entry_numeric_new(key, max, min, unit);
	section->size++;
	return 0;
}

void entry_numeric_free_data(entry_t * entry){
	free(entry->data);
	return;
}

void entry_numeric_display(window_t *window, entry_t *entry, int x, int y, int state){
	entry_numeric_t * data;

	data = entry->data;
	display_numeric(window, x, y, data->current, data->unit, DC_NORMAL);
}

int entry_numeric_edit(window_t *window, entry_t *entry, int x, int y){
	entry_numeric_t *data;
	int buffer;
	
	data = entry->data;
	buffer = read_one();
	switch(buffer){
	case KEY_ESC:
		return -1;
	case KEY_UP:
		data->current *= 2;
		if ((data->max != -1) && (data->current > data->max))
			data->current = data->max;
		entry->modified = 1;
		return 1; /* redraw */
	case KEY_DOWN:
		data->current /= 2;
		if ((data->min != -1) && (data->current < data->min))
			data->current = data->min;
		entry->modified = 1;
		return 1; /* redraw */
	case KEY_RIGHT:
		if ((data->max == -1) || (data->current < data->max)){
			data->current++;
			entry->modified = 1;
		}
		return 1; /* redraw */
	case KEY_LEFT:
		if ((data->min == -1) || (data->current > data->min)){
			entry->modified = 1;
			data->current--;
		}
		return 1; /* redraw */
	default:
		/* ignore */
		return 0;
	}
}
