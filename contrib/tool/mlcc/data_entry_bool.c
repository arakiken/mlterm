#include "config.h"
#include <stdlib.h> /* free/malloc */
#include "comm.h"
#include "data.h"
#include "data_entry_bool.h"

static int _is_true(char *str){

	if ((str[0] != 't')&&(str[0] != 'T'))
		return 0; /* mlterm returns "true" or "false". first char is enough*/
	return 1;
}


void entry_bool_reset(entry_t *entry){
	entry_bool_t * data = entry->data;
	mlterm_set_param(entry->key, (char *)((data->initial)? "true" : "false"));
}
void entry_bool_apply(entry_t *entry){
	entry_bool_t * data = entry->data;
	mlterm_set_param(entry->key, (char *)((data->current)? "true" : "false"));
}

entry_bool_t *entry_bool_new(const char *key){
	entry_bool_t *entry;
	
	entry = malloc(sizeof(entry_bool_t));
	entry->initial = _is_true(mlterm_get_param(key));
	entry->current = entry->initial;;
	return entry;
}

int entry_bool_add(section_t *section, const char *name, const char * key){
	if (section->maxwidth < strlen(name))
		section->maxwidth = strlen(name);
	section->entry[section->size].name = name;
	section->entry[section->size].key = key;
	section->entry[section->size].type = ENT_BOOL;
	section->entry[section->size].data = entry_bool_new(key);
	section->size++;
	return 0;
}

void entry_bool_free_data(entry_t * entry){
	free(entry->data);
	return;
}

void entry_bool_display(window_t *window, entry_t *entry, int x, int y, int state){
	entry_bool_t * data;

	data = entry->data;
	display_str(window, x, y, (char *)((data->current)? "True" : "False"), DC_NORMAL);
}

int entry_bool_edit(window_t *window, entry_t *entry, int x, int y){
	int buffer;
	entry_bool_t *data;
	
	data = entry->data;
	buffer = read_one();
	switch(buffer){
	case 27:
		buffer = read_one();
		if( buffer != 79){ /* cursor key or ESC ? */
			/* ESC */
			return -1;
		}
		buffer = read_one();
		switch(buffer){
		case 67: /* RIGHT */
			data->current = 1 - data->current;
			entry->modified = 1;
			break;
		case 68: /* LEFT */
			data->current = 1 - data->current;
			entry->modified = 1;
			break;
		default:
			/* ignore */
			return 0;
		}
		return 1; /* redraw */
	}
	return 0; /* don't redraw*/
}


