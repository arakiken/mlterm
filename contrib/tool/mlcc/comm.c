#include <stdio.h>
#include <unistd.h>
#include <string.h> /* memset, strchr */
#include <stdlib.h> /* atoi */
#include <termios.h> /* tcgetattr/tcsetattr */
#include "comm.h"

static char _buffer[512];

#define COLORID_DEFAULT 9

static int _choosed_color = 2;
static int _pointed_color = 1;
static const char *  color_name_table[] =
{
        "black" ,
        "red" ,
        "green" ,
        "yellow" ,
        "blue" ,
        "magenta" ,
        "cyan" ,
        "white" ,
	NULL
};

struct termios _oldtio;

/*
 * communication functions
 */

static void _csi(char *str){
	write(STDOUT_FILENO, "\033",1);
	write(STDOUT_FILENO, str, strlen(str));
}

int read_one(){
	int count;
	char buf[3] = {0,0,0};
	count = read(STDIN_FILENO, buf, 3);
	if (buf[0] != 27)  /* XXX should check non-printable */
		return  buf[0];
	if (buf[1] == 0)
		return KEY_ESC; /* single esc */
	if (buf[1] == 79)  /*cursor key?*/
		switch(buf[2]){
		case 65:
			return KEY_UP;
		case 66:
			return KEY_DOWN;
		case 67:
			return KEY_RIGHT;
		case 68:
			return KEY_LEFT;
		}
	return -1; /* couldn't processed */
}


void flush_stdout(){
	fsync(STDOUT_FILENO);
}

void dec_char(){
	_csi((char *)"(0"); 
}

void normal_char(){
	_csi((char *)"(B"); 
}

void set_cursor_pos(window_t * window, int x, int y){
	static char local_buffer[12]; /* [xxxx;yyyyH should enough._buffer may be already used */
	snprintf(local_buffer, sizeof(local_buffer), "[%d;%dH", window->top +y, window->left +x);
	_csi(local_buffer);
}

void set_fg_color(int colorid){
	char cmd[] = "[30m";
	if (colorid < 0)
		colorid = COLORID_DEFAULT;
	if (colorid > COLORID_DEFAULT)
		colorid = COLORID_DEFAULT;
	cmd[2] += colorid;
	_csi(cmd);
}

void set_fg_color_default(){
	_csi((char *)"[39m");
}

void set_bg_color(int colorid){
	char cmd[] = "[40m";
	if (colorid < 0)
		colorid = COLORID_DEFAULT;
	if (colorid > COLORID_DEFAULT)
		colorid = COLORID_DEFAULT;
	cmd[2] += colorid;
	_csi((char *)cmd);
}

void set_altscr(){
	_csi((char *)"7");
	_csi((char *)"[?47h");
}

void unset_altscr(){
	_csi((char *)"[?47l");
	_csi((char *)"8");
}

void set_keypad(){
	_csi((char *)"[?1h");
	_csi((char *)"=");
}

void unset_keypad(){
	_csi((char *)"[?1l");
	_csi((char *)">");
}

void cursor_show(){
	_csi((char *)"[?25h");
}

void cursor_hide(){
	_csi((char *)"[?25l");
}

void term_size(int *w, int *h){
	*w = atoi(mlterm_get_param("cols"));
	*h = atoi(mlterm_get_param("rows"));
}

char * mlterm_get_param(const char * key){
	char * result;

	snprintf(_buffer, sizeof(_buffer), "]5380;%s\007",key);
	_csi(_buffer);
	fgets(_buffer, sizeof(_buffer) -1, stdin);
	result = strchr(_buffer, '\n');
	if (!result){
		return NULL;
	}
	*result = 0; /* terminate */
	result = strchr(_buffer, '=');
	if (result){ /*XXX check key and error!!*/
		if (*(result +1))
			return result +1;
		else
			return NULL;
	}
	return NULL;
}

void mlterm_set_value(const char * key, int value){
	snprintf(_buffer, sizeof(_buffer), "]5379;%s=%d\007",key, value);
	_csi(_buffer);
}

void mlterm_set_param(const char * key, char *value){
	snprintf(_buffer, sizeof(_buffer), "]5379;%s=%s\007",key, value);
	_csi(_buffer);
}

void display_colorcube(window_t * window, int x, int y, int colorid){
	if ((colorid >= 0) && (colorid < COLORID_DEFAULT)){
		dec_char();
		set_fg_color(colorid);
		window_addstr(window, x, y, (char *)"a"); /* XXX better char? */
		set_fg_color(COLORID_DEFAULT); /* return to default*/
		normal_char();
	}else
		window_addstr(window, x, y, (char *)" ");
}

void display_str(window_t *window, int x, int y, char *src , decor_t flag){
	int len;
	
	len = strlen(src);
	if (len > sizeof(_buffer) -3)
		len = sizeof(_buffer) -3;
	memmove(_buffer +1, src, len +1);

	switch (flag){
	case DC_CHOOSED:
		_buffer[0]= '<';
		_buffer[len+1]= '>';
		set_fg_color(_choosed_color);
		break;
	case DC_POINTED:
		_buffer[0]= '<';
		_buffer[len+1]= '>';
		set_fg_color(_pointed_color);
		break;
	default:
	case DC_NORMAL:
		_buffer[0]= ' ';
		_buffer[len+1]= ' ';
		break;
	}
	_buffer[len+2]= 0;	
	window_addstr(window, x, y, _buffer);
	switch (flag){
	case DC_CHOOSED:
	case DC_POINTED:
		set_fg_color_default();
		break;
	default:
	}
}

void display_numeric(window_t *window, int x, int y, int value, const char *unit , decor_t flag){
	char *pos;
	
	snprintf(_buffer, sizeof(_buffer)-1, " %4d:(%s)", value, unit);
	pos = strchr(_buffer, ':');
	switch (flag){
	case DC_CHOOSED:
		_buffer[0]= '<';
		pos[0]= '>';
		set_fg_color(_choosed_color);
		break;
	case DC_POINTED:
		_buffer[0]= '<';
		pos[0]= '>';
		set_fg_color(_pointed_color);
		break;
	case DC_NORMAL:
		_buffer[0]= ' ';
		pos[0]= ' ';
		break;
	}
	window_addstr(window, x, y, _buffer);
	switch (flag){
	case DC_CHOOSED:
		/* pass through */
	case DC_POINTED:
		set_fg_color_default();
		break;
	case DC_NORMAL:
	}
}

int colorid_from_name(char * name){
	int i;

	for (i = 0; color_name_table[i]; i++)
		if (strcmp(name, color_name_table[i]) == 0)
			return i;
	return -1;
}

char *colorname_from_id(int colorid){
	if ((colorid >= 0) && (colorid < COLORID_DEFAULT)) 
		return (char *)(color_name_table[colorid]);
	return (char *)"(error)";
}

/*
 * window handling functions
 */

window_t * window_new(int left, int top, int right, int bottom, int framed, window_t * parent){
	window_t * window;
	window = (window_t *)malloc(sizeof(window_t));
	window->framed = framed;
	window->parent = parent;
	if (parent){
			window->top = top + parent->top;
			window->bottom = bottom + parent->top;
			window->left = left + parent->left;
			window->right = right + parent->left;
	}else{
		window->top = top;
		window->bottom = bottom;
		window->left = left;
		window->right = right;
	}
	return window;
}

void window_free(window_t * window){
	free(window);
}

void window_addstr(window_t * window, int x, int y, char *str){
	if (window->framed){
		set_cursor_pos(window, x +1, y+1);
	}else{
		set_cursor_pos(window, x, y);
	}
	write(STDOUT_FILENO, str, strlen(str));
}

int window_width(window_t *window){
	return window->right - window->left +1;
}

void window_clear(window_t * window){
	int y, width;

	width = window_width(window);
	if (width > sizeof(_buffer) -1)
		width = sizeof(_buffer) -1;
	memset(_buffer, ' ', width-1);
	_buffer[width] = 0;

	if (window->framed){
		dec_char();
		_buffer[0] = 'x'; /* vertical line*/
		_buffer[width -1] = 'x';
	}else
		_buffer[width -1] = ' ';

	for (y = 1; y < window->bottom - window->top; y++){
		set_cursor_pos(window, 0, y);
		write(STDOUT_FILENO, _buffer, strlen(_buffer));
	}

	if (window->framed){
		memset(_buffer, 'q', width -1); /* horiz. line*/
		_buffer[0] = 'l';               /* upper left*/
		_buffer[width -1] = 'k';        /* upper right*/
	}
	set_cursor_pos(window, 0, 0);
	write(STDOUT_FILENO, _buffer, strlen(_buffer));
	if (window->framed){
		_buffer[0] = 'm';               /* lower left*/
		_buffer[width -1] = 'j';        /* lower right*/
	}
	set_cursor_pos(window, 0, window->bottom - window->top);
	write(STDOUT_FILENO, _buffer, strlen(_buffer));
	if (window->framed)
		normal_char();

	flush_stdout();
}

int termios_init(){
	struct termios newtio;
	tcgetattr(0, &_oldtio);
	newtio = _oldtio;
	newtio.c_lflag &= ~ICANON;
	newtio.c_lflag &= ~ECHO;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0; /* have to break with some intervals to distinguish ESC/Right*/
	tcsetattr(0, TCSAFLUSH, &newtio);
	return 0;
}

int termios_final(){
	tcsetattr(0, TCSAFLUSH, &_oldtio);
	return 0;
}

