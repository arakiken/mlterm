#include <string.h> /* strncasecmp/strcasecmp/strdup/strchr/strrchr/memmove*/
#include <stdlib.h> /* free/malloc */
#include "comm.h"
#include "data.h"
#include "data_entry_color.h"
int entry_color_edit(window_t *window, entry_t *entry, int x, int y){
	entry_color_t *data;
	int result;
	window_t * edit;

	data = entry->data;
	edit = window_new(x+2, y, x +12, y +9, 1, window); /*XXX*/
	result = color_select(edit, colorid_from_name(data->current));
	if (result != colorid_from_name(data->current)){
		free(data->current);
		data->current = strdup(colorname_from_id(result));
		entry->modified = 1;
	}
	return -1; /* finished */
}

void entry_color_reset(entry_t *entry){
	entry_color_t * data = entry->data;
	mlterm_set_param(entry->key, data->initial);
}

void entry_color_apply(entry_t *entry){
	entry_color_t * data = entry->data;
	mlterm_set_param(entry->key, data->current);
	free(data->current);
	data->current = strdup(mlterm_get_param(entry->key));
}

entry_color_t *entry_color_new(const char *key){
	entry_color_t *entry;
	
	entry = malloc(sizeof(entry_color_t));
	entry->initial = mlterm_get_param(key);
	if (entry->initial)
		entry->initial = strdup(entry->initial);
	else
		entry->initial = strdup("");
	entry->current = strdup(entry->initial);
	return entry;
}

int entry_color_add(section_t *section, const char *name, const char * key){
	if (section->maxwidth < strlen(name))
		section->maxwidth = strlen(name);
	section->entry[section->size].name = name;
	section->entry[section->size].key = key;
	section->entry[section->size].type = ENT_COLOR;
	section->entry[section->size].data = entry_color_new(key);
	section->size++;
	return 0;
}

void entry_color_free_data(entry_t * entry){
	entry_color_t *data;

	data = entry->data;
	free(data->initial);
	free(data->current);
	free(data);
	return;
}


void entry_color_display(window_t *window, entry_t *entry, int x, int y, int state){
	entry_color_t * data;

	data = entry->data;
	display_colorcube(window, x+1, y, colorid_from_name(data->current));
  	display_str(window, x+2, y, data->current, DC_NORMAL);
}

int color_select(window_t *edit, int initial){
	int ind, i, flag = 1;
	int buffer;

	window_clear(edit);
	ind = initial;

	while(1){
		if (flag){
			window_clear(edit);
			for(i = 0; i < 8; i++){				
				if (i == ind)
					window_addstr(edit, 0, i, ">");
				set_fg_color(i);
				window_addstr(edit, 1, i, colorname_from_id(i));
				set_fg_color_default();
			}
			flush_stdout();
			flag = 0;
		}
		buffer = read_one();
		switch(buffer){
		case KEY_ESC:
			return initial;
		case KEY_UP:
		case KEY_LEFT:
			ind--;
			if (ind < 0)
				ind = 8;
			flag = 1;
			break;
		case KEY_DOWN:
		case KEY_RIGHT:
			ind++;
			if (ind > 8)
				ind = 0;
			flag = 1;			
			break;
		case 10: /* ret */
  			return ind;
		}
	}
}

