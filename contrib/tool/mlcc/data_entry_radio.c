#include <string.h> /* strncasecmp/strcasecmp/strdup/strchr/strrchr/memmove*/
#include <stdlib.h> /* free/malloc */
#include "comm.h"
#include "data.h"
#include "data_entry_radio.h"

int entry_radio_edit(window_t *window, entry_t *entry, int x, int y){
	entry_radio_t *data;
	int buffer;
	data = entry->data;
	buffer = read_one();
	switch(buffer){
	case KEY_ESC:
	case 10:
		return -1;
	case KEY_RIGHT:
	case KEY_DOWN:
		if (data->current < data->num){
			data->current++;
			entry->modified = 1;
		}
		return 1; /* redraw */
	case KEY_LEFT:
	case KEY_UP:
		if (data->current >0){
			data->current--;
			entry->modified = 1;
		}
		return 1; /* redraw */
	default:
		/* ignore */
		return 0;
	}
}

static int _which_one(char *str, char **terms){
	int i = 0;
	if (!str)
		return 0;
	while(strcasecmp(str, terms[i])){
		i++;
		if (!terms[i])
			return -1;
	}
	return i;
}


void entry_radio_reset(entry_t *entry){
	entry_radio_t * data = entry->data;
	mlterm_set_param(entry->key, data->term[data->initial]);
}

void entry_radio_apply(entry_t *entry){
	entry_radio_t * data = entry->data;
	mlterm_set_param(entry->key, data->term[data->current]);
}

entry_radio_t *entry_radio_new(const char *key, char *terms){
	int i = 0;
	char *p;
	char *src;
	entry_radio_t *entry;
	
	entry = malloc(sizeof(entry_radio_t));
	src = strdup(terms); 
	p = strchr(src, '/');
	while(p){
		i++;
		p = strchr(p +1, '/');
	}
	entry->num = i+1;
	entry->term = malloc(sizeof(char *) * (i+1));
	p = src;
	entry->term[i] = NULL; /* garrison */
	for(; i > 0; i--){
		p = strrchr(src, '/');
		entry->term[i] = strdup(p +1);
		*p = 0;
	}
	entry->term[0] = strdup(src);
	free(src);
	entry->initial = _which_one(mlterm_get_param(key), entry->term);
	entry->current = entry->initial;
	return entry;
}

int entry_radio_add(section_t *section, const char *name, const char * key, char *terms){
	if (section->maxwidth < strlen(name))
		section->maxwidth = strlen(name);
	section->entry[section->size].name = name;
	section->entry[section->size].key = key;
	section->entry[section->size].type = ENT_RADIO;
	section->entry[section->size].data = entry_radio_new(key, terms);
	section->size++;
	return 0;
}

void entry_radio_free_data(entry_t * entry){
	entry_radio_t * data;
	int i;

	data = entry->data;
	for(i = 0; i < data->num; i++){
		free(data->term[i]); /*last term is always null and free is not required (but harmless)*/
	}
	free(data->term);
	free(data);
	return;
}

void entry_radio_display(window_t *window, entry_t *entry, int x, int y, int state){
	entry_radio_t * data;
	int i;

	data = entry->data;
	for(i = 0; i < data->num; i++){
		if (i == data->current){
			if (state)
				display_str(window, x, y, data->term[i], DC_POINTED);
			else
				display_str(window, x, y, data->term[i], DC_CHOOSED);
		}else{
			display_str(window, x, y, data->term[i], DC_NORMAL);
		}
		x += strlen(data->term[i]) +2;
	}	
}
