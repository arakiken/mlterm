#include "config.h"
#include <string.h> /* strncasecmp/strcasecmp/strdup/strchr/strrchr/memmove*/
#include <stdlib.h> /* free/malloc */
#include "comm.h"
#include "data.h"
#include "data_entry_string.h"

int entry_string_edit(window_t *window, entry_t *entry, int x, int y){
	entry_string_t *data;
	int result;
	char * str_new;
	window_t * edit;
	data = entry->data;
	edit = window_new(x+1, y, window->right, y+2, 1, window); /*XXX*/
	result = string_edit(edit, data->current, &str_new);
	if (result){
		free(data->current);
		data->current = strdup(str_new);
		entry->modified = 1;
	}
	return -1; /* finished */
}

void entry_string_reset(entry_t *entry){
	entry_string_t * data = entry->data;
	mlterm_set_param(entry->key, (char *)data->initial);
}

void entry_string_apply(entry_t *entry){
	entry_string_t * data = entry->data;
	mlterm_set_param(entry->key, data->current);
	free(data->current);
	data->current = strdup(mlterm_get_param(entry->key));
}


entry_string_t *entry_string_new(const char *key){
	entry_string_t *entry;

	entry = malloc(sizeof(entry_string_t));
	entry->initial = mlterm_get_param(key);
	if (entry->initial){
		entry->initial = strdup(entry->initial);
		entry->current = strdup(entry->initial);
	}else{
		entry->initial = NULL;
		entry->current = NULL;
	}
	entry->max = -1;
	entry->min = 0;
	return entry;
}

int entry_string_add(section_t *section, const char *name, const char * key){
	if (section->maxwidth < strlen(name))
		section->maxwidth = strlen(name);
	section->entry[section->size].name = name;
	section->entry[section->size].key = key;
	section->entry[section->size].type = ENT_STRING;
	section->entry[section->size].data = entry_string_new(key);
	section->size++;
	return 0;
}

void entry_string_free_data(entry_t * entry){
	entry_string_t *data;

	data = entry->data;
	free(data->initial);
	free(data->current);
	free(data);
	return;
}

void entry_string_display(window_t *window, entry_t *entry, int x, int y, int state){
	entry_string_t *data;

	data = entry->data;
	if (data->current)
		display_str(window, x, y, data->current, DC_NORMAL);
	else
		display_str(window, x, y, (char *)"(error??)", DC_NORMAL);
}
