#include <signal.h> /* sigaction */

#include "comm.h"
#include "data.h"

/* should be removed... */
#include "data_entry_string.h"
#include "data_entry_bool.h"
#include "data_entry_color.h"
#include "data_entry_radio.h"
#include "data_entry_numeric.h"


#define MAX_SECTION 8

typedef struct {
	section_t section[MAX_SECTION];
	int filled;
	int selected;
	display_state_t state;
} config_data_t;

/*
 *  global variables
 */

/* must be global to be changed by signal handler */
static int _cols;
static int _rows;
static int _redraw_all = 0;


section_t * current_section(config_data_t *data);
section_t * section_from_name(config_data_t *data, char *name);
section_t * section_from_name(config_data_t *data, char *name);
window_t * section_window_new(window_t * parent, config_data_t *data);
section_t * section_add(config_data_t *data, const char *name);
int section_reset(section_t *section);
int display_section(window_t *window, config_data_t *data);

int data_reset(config_data_t *data);
void data_free(config_data_t *data);

int edit_entry(window_t * window, config_data_t *data);
int display_entry(window_t *window, config_data_t *data);
int select_entry(config_data_t *data);

int query_exit(window_t *parent);
int init_data(config_data_t *data);
/*
 *  backyard
 */

/* recover terminal settings and die gracefully */
static void _finalize(void){
	termios_final();
	unset_altscr();	
	unset_keypad();	
	normal_char();
	cursor_show();
	exit(0);
}

/*
 *  signal handler
 */

static void _signal_int(int signo){
	_finalize();
}

static void _signal_winch(int signo){
	term_size(&_cols, &_rows);
	_redraw_all = 1; /* do not redraw immediately to avoid overload */
}

/*
 *  config data handler
 */

section_t * current_section(config_data_t *data){
	return &(data->section[data->selected]);
}

section_t * section_from_name(config_data_t *data, char *name){
	int i;

	for(i = 0; i < data->filled; i++)
		if (strcmp(data->section[i].name, name) == 0)
			return &(data->section[i]);
	return 0;
}

section_t * section_add(config_data_t *data, const char *name){
	if (data->filled > MAX_SECTION -2)
		return 0;
	data->section[data->filled].name = name;
	data->section[data->filled].size = 0;
	data->section[data->filled].selected = 0;
	data->section[data->filled].modified = 0;
	data->section[data->filled].maxwidth = 0;
	return &(data->section[data->filled++]);
}

window_t * section_window_new(window_t * parent, config_data_t *data){
	int cur = 0;
	int pos = 1;
	int skip = 0;

	while(cur < data->filled){
		pos += (strlen(data->section[cur].name) +2);
		if (pos > (window_width(parent))){
			cur--;
			pos = 2;
			skip ++;
		}
		cur ++;
	}
	return window_new(0, 0, pos , skip +3, 1, parent);
}

int section_reset(section_t *section){
	int i;
	for(i = 0; i < section->size; i++){
		entry_reset(&(section->entry[i]));
	}
	return 0;
}

int display_section(window_t *window, config_data_t *data){
	int cur = 0;
	int pos = 0;
	int len;
	int skip = 0;

	while(cur < data->filled){
		len = strlen(data->section[cur].name) +2;
		if ((pos + len) > (window->right - window->left)){
			cur--;
			pos = 1;
			skip ++;
		}else{
			display_str(window, pos+1, skip +1,
				    (char *)(data->section[cur].name),
			    (data->selected == cur) ? (data->state == DS_SELECT ? DC_POINTED:DC_CHOOSED):DC_NORMAL);
			pos += len;
		}
		cur ++;
	}
	return 0;
}

int data_reset(config_data_t *data){
	int i;

	for(i = 0; i < data->filled; i++)
			section_reset(&(data->section[i]));
	return 0;
}

void data_free(config_data_t *data){
	for (; data->filled >0; (data->filled)--)
		section_free(&data->section[data->filled -1]);
}

int edit_entry(window_t * window, config_data_t *data){
	section_t * section;
	entry_t * entry;
	int result = 0xDEADBEEF;
	
	section = current_section(data);
	entry = &(section->entry[section->selected]);
	result = entry_edit(window, entry, section->maxwidth +4, section->selected*2);
	if (result == 0)
		return 0;
	if (result < 0)
		data->state = DS_SELECT;

	entry_apply(entry);
	if (entry->modified)
		section->modified = 1;
	return 1;
}

int display_entry(window_t *window, config_data_t *data){
	section_t * current;
	int i;

	current = current_section(data);
	for(i = 0; i < current->size; i++){
		display_str(window, 1, i*2  ,
			    (char *)(current->entry[i].name),
			    (current->selected == i) ? (data->state == DS_SELECT ? DC_CHOOSED:DC_POINTED):DC_NORMAL);

		window_addstr(window, current->maxwidth +3, i*2, (char *)":");
		if ((i == current->selected) && (data->state == DS_EDIT))
			entry_display(window, &(current->entry[i]),
				      current->maxwidth +4, i*2, 1);
		else
			entry_display(window, &(current->entry[i]),
				      current->maxwidth +4, i*2, 0);
		
	}
	return 0;
}

int select_entry(config_data_t *data){
	int buffer;

	section_t  *cur;
	cur = current_section(data);
	buffer = read_one();
	switch(buffer){
	case KEY_ESC:
		data->state = DS_CANCEL;
		return 1; /* redraw */
	case KEY_UP:
		if (cur->selected > 0)
			cur->selected--;
		return 1; /* redraw */
	case KEY_DOWN: /* DOWN */
		if (cur->selected < cur->size -1)
			cur->selected++;
		return 1; /* redraw */
	case KEY_RIGHT: /* RIGHT */
		if (data->selected < data->filled -1)
			data->selected++;
		return 1; /* redraw */
	case KEY_LEFT: /* LEFT */
		if (data->selected >0)
			data->selected--;
		return 1; /* redraw */
	case 10: /* ret */
		data->state = DS_EDIT;
		return 1; /* redraw */
	default:
		/* ignore */
		return 0; /* don't redraw*/
	}
}

int query_exit(window_t *parent){
	window_t *query;
	int flag, buffer;
	int state = 0;

	flag = 1;
	query = window_new((parent->right - parent->left)/2 -7, (parent->bottom - parent->top)/2 -2,
			   (parent->right - parent->left)/2 +7, (parent->bottom - parent->top)/2 +3,
			   1, parent);
	while(1){
		if (flag){
			window_clear(query);
			window_addstr(query, 0, 0, (char *)" Really Quit?");
			if (state == 0){
				window_addstr(query, 1, 2, (char *)" <apply>");
				window_addstr(query, 1, 3, (char *)" discard ");
			}else{

				window_addstr(query, 1, 2, (char *)"  apply ");
				window_addstr(query, 1, 3, (char *)"<discard>");
			}
			flush_stdout();
			flag = 0;
		}
		buffer = read_one();
		switch(buffer){
		case KEY_ESC:/* cursor key or ESC */
			window_free(query);
			return -1;
		case KEY_UP:
		case KEY_DOWN:
		case KEY_RIGHT:
		case KEY_LEFT:
			state = 1-state;
			flag = 1;
			break;
		case 10: /* ret */
			window_free(query);
  			return state;
		default:
			break;
		}
	}
}

/*
 *  initialize ... may be separated into conffile
 */

int init_data(config_data_t *data){
	section_t *section;
	data->state = DS_SELECT;
	data->selected = 0;
	data->filled = 0;

	section = section_add(data, "Encoding");
	entry_string_add(section, "Encoding" , "encoding");
	entry_string_add(section, "Input Method", "input_method");
	entry_bool_add(section, "Bidi", "use_bidi");
	entry_bool_add(section, "Combining", "use_combining");
	entry_bool_add(section, "Process via unicode", "copy_paste_via_ucs");

	section = section_add(data, "Appearance");
	entry_numeric_add(section, "Font size", "fontsize", 5, 50, "pix");
	entry_numeric_add(section, "Line space", "line_space", 0, 5, "pix");
	entry_numeric_add(section, "Width", "screen_width_ratio", 0, 100, "%");
	entry_numeric_add(section, "Height", "screen_height_ratio", 0, 100, "%");
	entry_bool_add(section, "Variable column width", "use_variable_column_width");
	entry_bool_add(section, "Anti alias", "use_anti_alias");

	section = section_add(data, "Color");
	entry_color_add(section, "Foreground color", "fg_color");
	entry_color_add(section, "Background color", "bg_color");
	entry_numeric_add(section, "Fade ratio", "fade_ratio", 0, 100, "%");
	entry_string_add(section, "Wall picture", "wall_picture");
	entry_numeric_add(section, "Brightness", "brightness", 0, -1, "%");
	entry_numeric_add(section, "Contrast", "contrast", 0, -1, "%");
	entry_numeric_add(section, "Gammma", "gamma", 0, -1, "%");
	entry_bool_add(section, "Transparent background", "use_transbg");

	section = section_add(data, "Scrollbar");
	entry_radio_add(section, "Position", "scrollbar_mode", (char *)"none/left/right");
	entry_string_add(section, "View", "scrollbar_view_name");
	entry_color_add(section, "Foreground color", "sb_fg_color");
	entry_color_add(section, "Background color", "sb_bg_color");

	section = section_add(data, "Others");
	entry_numeric_add(section, "Tab width", "tabsize", 0, 100, "column");
	entry_numeric_add(section, "Backlog size", "logsize", 128, 2048, "line");
	entry_radio_add(section, "Meta key outputs", "mod_meta_mode", (char *)"none/esc/8bit");
	entry_radio_add(section, "Vertical mode", "vertical_mode", (char *)"none/cjk/mongol");
	entry_radio_add(section, "Bel mode", "bel_mode", (char *)"none/sound/visual");
	entry_bool_add(section, "Combining", "use_dynamic_comb");
	entry_bool_add(section, "Fullwidth", "use_multi_column_char");
	return 0;
}

int main(int argc, char **argv){
	static config_data_t data;
	struct sigaction act;	
	window_t *win_root =0, *win_section = 0, *win_entry = 0;

	sigemptyset(&(act.sa_mask));
	sigaddset(&act.sa_mask,SIGINT | SIGWINCH);
	act.sa_flags = SA_RESTART;

	act.sa_handler = _signal_int;
	sigaction(SIGINT, &act, 0);
	act.sa_handler = _signal_winch;
	sigaction(SIGWINCH, &act, 0);

	cursor_hide();
	set_keypad();
	set_altscr();
	termios_init();

	init_data(&data);
  	term_size(&_cols, &_rows);
	_redraw_all = 1;
	while(1){
		if (_redraw_all){
			_redraw_all = 0;
			window_free(win_section);
			window_free(win_entry);
			window_free(win_root);
			
			win_root = window_new(0, 0, _cols-1, _rows-1, 0, 0);
			window_clear(win_root);
			
			win_section = section_window_new(win_root, &data);
			window_clear(win_section);
			
			win_entry = entry_window_new(win_section);
			window_clear(win_entry);
			display_section(win_section, &data);
			display_entry(win_entry, &data);			
			flush_stdout();
		}
		switch(data.state){
		case DS_SELECT:
			if (select_entry(&data)){
  				window_clear(win_entry);/* clear garbage */
				display_section(win_section, &data);
				display_entry(win_entry, &data);
				flush_stdout();
			}			
			break;
		case DS_EDIT:
			if (edit_entry(win_entry, &data)){	
				window_clear(win_entry); /* clear garbage */
				display_section(win_section, &data); /* change color */
				display_entry(win_entry, &data);
				flush_stdout();
			}			
			break;
		case DS_CANCEL:
			switch(query_exit(win_entry)){
			case 1:
				goto CANCEL;
			case 0:
				goto FIN;
			case -1:
				data.state = DS_SELECT;
				_redraw_all = 1;
			}
			break;
		}
	}
 CANCEL:
	/*XXX reset params to initial state*/
	data_reset(&data);
	
 FIN:
	data_free(&data); /* destruct date tree. segv should be caught here if there are bugs*/
	_finalize(); /*recover terminal setting*/
	return 0;
}

