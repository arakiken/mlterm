#include <stdio.h>  /* printf */
#include <signal.h> /* sigaction */
#include <stdlib.h>
#include <string.h>

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
static int cols;
static int rows;
static int redraw_all = 0;


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

static void help_msg(void){
	printf( "[Usage]\n");
	printf( " mlcc               : Show configuration screen.\n");
	printf( " mlcc -h/--help     : Show this message.\n");
	printf( " mlcc exec [command]: Execute mlterm command. (full_reset, mlclient, open_pty and so on)\n") ;
	printf( " mlcc [key]         : Get current value of [key].\n");
	printf( " mlcc [key] [value] : Set [value] for [key].\n");
	printf( " mlcc [font file name] [charset],[font size] : Get font name of [charset] and [font size] in [font file name].\n");
	printf( " mlcc [font file name] [charset] [font name] : Set [font name] for [charset] in [font file name].\n");
	printf( " mlcc color [color name] [rgb] : Set [rgb] for [color name].\n") ;
	printf( " (See doc/en/PROTOCOL, PROTOCOL.font and PROTOCOL.color for configuration details.)\n") ;
}

/*
 *  backyard
 */

/* recover terminal settings and die gracefully */
static void finalize(void){
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

static void signal_int(int signo){
	finalize();
}

static void signal_winch(int signo){
	term_size(&cols, &rows);
	redraw_all = 1; /* do not redraw immediately to avoid overload */
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
	return NULL;
}

section_t * section_add(config_data_t *data, const char *name){
	if (data->filled > MAX_SECTION -2)
		return NULL;
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
				    (data->section[cur].name),
				    (data->selected == cur) ?
				    (data->state == DS_SELECT ?
				     DC_POINTED:DC_CHOOSED):DC_NORMAL);
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
	result = entry_edit(window,
			    entry,
			    section->maxwidth +4,
			    section->selected*2);
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
			    (current->entry[i].name),
			    (current->selected == i) ?
			    (data->state == DS_SELECT ?
			     DC_CHOOSED:DC_POINTED):DC_NORMAL);

		window_addstr(window, current->maxwidth +3, i*2, ":");
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
	query = window_new((parent->right - parent->left)/2 -7,
			   (parent->bottom - parent->top)/2 -2,
			   (parent->right - parent->left)/2 +7,
			   (parent->bottom - parent->top)/2 +3,
			   1, parent);
	while(1){
		if (flag){
			window_clear(query);
			window_addstr(query, 0, 0, " Really Quit?");
			if (state == 0){
				window_addstr(query, 1, 2, " <apply>");
				window_addstr(query, 1, 3, " discard ");
			}else{

				window_addstr(query, 1, 2, "  apply ");
				window_addstr(query, 1, 3, "<discard>");
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
	entry_bool_add(section, "Indic" , "use_ind");
	entry_bool_add(section, "Combining", "use_combining");
	entry_bool_add(section, "Process via unicode", "copy_paste_via_ucs");

	section = section_add(data, "Appearance");
	entry_numeric_add(section, "Font size", "fontsize", 5, 50, "pix");
	entry_numeric_add(section, "Line space", "line_space", 0, 5, "pix");
	entry_numeric_add(section, "Width", "screen_width_ratio", 0, 100, "%");
	entry_numeric_add(section, "Height", "screen_height_ratio", 0, 100, "%");
	entry_bool_add(section, "Variable column width", "use_variable_column_width");
	entry_bool_add(section, "Anti alias", "use_anti_alias");
	entry_string_add(section, "Type engine", "type_engine");

	section = section_add(data, "Color");
	entry_color_add(section, "Foreground color", "fg_color");
	entry_color_add(section, "Background color", "bg_color");
	entry_numeric_add(section, "Fade ratio", "fade_ratio", 0, 100, "%");
	entry_string_add(section, "Wall picture", "wall_picture");
	entry_numeric_add(section, "Brightness", "brightness", 0, -1, "%");
	entry_numeric_add(section, "Contrast", "contrast", 0, -1, "%");
	entry_numeric_add(section, "Gammma", "gamma", 0, -1, "%");
	entry_numeric_add(section, "Alpha", "alpha", 0, -1, "%");
	entry_bool_add(section, "Transparent background", "use_transbg");

	section = section_add(data, "Scrollbar");
	entry_radio_add(section, "Position", "scrollbar_mode", "none/left/right");
	entry_string_add(section, "View", "scrollbar_view_name");
	entry_color_add(section, "Foreground color", "sb_fg_color");
	entry_color_add(section, "Background color", "sb_bg_color");

	section = section_add(data, "Others");
	entry_numeric_add(section, "Tab width", "tabsize", 0, 100, "column");
	entry_numeric_add(section, "Backlog size", "logsize", 128, 2048, "line");
	entry_radio_add(section, "Meta key outputs", "mod_meta_mode", "none/esc/8bit");
	entry_radio_add(section, "Vertical mode", "vertical_mode", "none/cjk/mongol");
	entry_radio_add(section, "Bel mode", "bel_mode", "none/sound/visual");
	entry_bool_add(section, "Combining", "use_dynamic_comb");
	entry_bool_add(section, "Fullwidth", "use_multi_column_char");
	entry_bool_add(section, "Clipboard", "use_clipboard");
	return 0;
}

int main(int argc, char **argv){
	static config_data_t data;
	struct sigaction act;
	window_t *win_root =NULL, *win_section = NULL, *win_entry = NULL;

	if(argc == 2){
		if (strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"--help") == 0){
			help_msg();
		}
		else{
			mlterm_get_param(argv[1]);
		}
		exit(0);
	}
	else if(argc >= 3 && strcmp(argv[1],"exec") == 0){
		int i;
		char * cmd;
		size_t cmd_len = 0;
		for (i = 2; i < argc; i++){
			/* +3 is for "" and space(or NULL terminator). */
			cmd_len += (strlen(argv[i]) + 3) ;
		}
		cmd = alloca(cmd_len);
		i = 2;
		strcpy(cmd,argv[i]);
		while(++i < argc){
			strcat(cmd , " ");
			if(strncmp(argv[2],"mlclient",8) == 0){
				/* for mlclient arguments. */
				strcat(cmd , "\"");
			}
			strcat(cmd , argv[i]);
			if(strncmp(argv[2],"mlclient",8) == 0){
				/* for mlclient arguments. */
				strcat(cmd , "\"");
			}
		}
		mlterm_exec(cmd);
		exit(0);
	}
	else if(argc == 3 || argc == 4){
		char * p;
		
		if(argv[1][0] == 't' || argv[1][0] == 'v'){
			p = argv[1] + 1;
		}else{
			p = argv[1];
		}
		
		if(strcmp(p,"font") == 0 || strcmp(p,"aafont") == 0){
			if(argc == 3){
				mlterm_get_font_param(argv[1],argv[2]);
				exit(0);
			}
			if(argc == 4){
				mlterm_set_font_param(argv[1],argv[2],argv[3]);
				exit(0);
			}
		}else if(strcmp(p,"color") == 0){
			if(argc == 3){
				mlterm_get_color_param(argv[2]);
				exit(0);
			}
			if(argc == 4){
				mlterm_set_color_param(argv[2],argv[3]);
				exit(0);
			}
		}else if(argc == 3){
			mlterm_set_param(argv[1], argv[2]);
			exit(0);
		}
	}
	
	sigemptyset(&(act.sa_mask));
	sigaddset(&act.sa_mask,SIGINT | SIGWINCH);
	act.sa_flags = SA_RESTART;

	act.sa_handler = signal_int;
	sigaction(SIGINT, &act, NULL);
	act.sa_handler = signal_winch;
	sigaction(SIGWINCH, &act, NULL);

	cursor_hide();
	set_keypad();
	set_altscr();
	termios_init();

	init_data(&data);
	if (term_size(&cols, &rows) == -1)
		goto FIN;

	redraw_all = 1;
	while(1){
		if (redraw_all){
			redraw_all = 0;
			window_free(win_section);
			window_free(win_entry);
			window_free(win_root);

			win_root = window_new(0, 0, cols-1, rows-1, 0, NULL);
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
				redraw_all = 1;
			}
			break;
		}
	}
CANCEL:
	/*XXX reset params to initial state*/
	data_reset(&data);
FIN:
	data_free(&data); /* destruct date tree. segv should be caught here if there are bugs*/
	finalize(); /*recover terminal setting*/
	return 0;
}

