#include "comm.h"
#include "data.h"
#include "data_entry_string.h"
#include "data_entry_bool.h"
#include "data_entry_color.h"
#include "data_entry_radio.h"
#include "data_entry_numeric.h"

/*
 *   entry handler
 */

int entry_free(entry_t *entry){
	switch(entry->type){
	case ENT_STRING:
		entry_string_free_data(entry);
		break;
	case ENT_COLOR:
		entry_color_free_data(entry);
		break;
	case ENT_RADIO:
		entry_radio_free_data(entry);
		break;
	case ENT_BOOL:
		entry_bool_free_data(entry);
		break;
	case ENT_NUMERIC:
		entry_numeric_free_data(entry);
		break;
	case ENT_NONE:
		/* something stupid happning... */
		return -1;
	}	
	return 0;
}

int entry_display(window_t *window, entry_t *entry, int x, int y, int state){
	switch(entry->type){
	case ENT_STRING:
		entry_string_display(window, entry, x, y, state);
		break;
	case ENT_BOOL:
		entry_bool_display(window, entry, x, y, state);
		break;
	case ENT_COLOR:
		entry_color_display(window, entry, x, y, state);
		break;
	case ENT_NUMERIC:
		entry_numeric_display(window, entry, x, y, state);
		break;
	case ENT_RADIO:
		entry_radio_display(window, entry, x, y, state);
		break;
	case ENT_NONE:
		/* something stupid happning... */
		return -1;
	}
	return 0;
}

int entry_apply(entry_t *entry){
	switch(entry->type){
	case ENT_STRING:
		entry_string_apply(entry);
		break;
	case ENT_BOOL:
		entry_bool_apply(entry);
		break;
	case ENT_COLOR:
		entry_color_apply(entry);
		break;
	case ENT_NUMERIC:
		entry_numeric_apply(entry);
		break;
	case ENT_RADIO:
		entry_radio_apply(entry);
		break;
	case ENT_NONE:
		/* something stupid happning... */
		break;
	}
	return 0;
}

int entry_edit(window_t *window, entry_t *entry, int x, int y){
	switch(entry->type){
	case ENT_STRING:
		return entry_string_edit(window, entry, x, y);
	case ENT_BOOL:
		return entry_bool_edit(window, entry, x, y);
	case ENT_COLOR:
		return entry_color_edit(window, entry, x, y);
	case ENT_NUMERIC:
		return entry_numeric_edit(window, entry, x, y);
	case ENT_RADIO:
		return entry_radio_edit(window, entry, x, y);
	case ENT_NONE:
		/* something stupid happning... */
		break;
	}
	return 0;
}

window_t * entry_window_new(window_t * section){
	window_t * parent = section->parent;
  	return window_new(0, parent->top + section->bottom +1,
			  window_width(parent) -1, parent->bottom +1,
			  1, parent);
}

int entry_reset(entry_t *entry){
	switch(entry->type){
	case ENT_STRING:
		entry_string_reset(entry);
		break;
	case ENT_BOOL:
		entry_bool_reset(entry);
		break;
	case ENT_COLOR:
		entry_color_reset(entry);
		break;
	case ENT_NUMERIC:
		entry_numeric_reset(entry);
		break;
	case ENT_RADIO:
		entry_radio_reset(entry);
		break;
	case ENT_NONE:
		/* something stupid happning... */
		break;
	}	
	return 0;
}

/*
 *   section handler
 */
void section_free(section_t *section){
	section->size--;
	for(; section->size > 0; (section->size)--){
		entry_free(&(section->entry[section->size]));
	}
}

