/*
 *	$Id$
 */

#include  "x_keymap.h"

#include  <stdio.h>		/* sscanf */
#include  <string.h>		/* strchr/memcpy */
#include  <X11/keysym.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>


typedef struct  key_func_table
{
	char *  name ;
	x_key_func_t  func ;
	
} key_func_table_t ;


/*
 * !! Notice !!
 * these are not distinguished.
 */
#define  ModMask  (Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)


/* --- static variables --- */

static key_func_table_t  key_func_table[] =
{
	{ "XIM_OPEN" , XIM_OPEN , } ,
	{ "XIM_CLOSE" , XIM_CLOSE , } ,
	{ "OPEN_SCREEN" , OPEN_SCREEN , } ,
	{ "OPEN_PTY" , OPEN_PTY , } ,
	{ "PAGE_UP" , PAGE_UP , } ,
	{ "PAGE_DOWN" , PAGE_DOWN , } ,
	{ "SCROLL_UP" , SCROLL_UP , } ,
	{ "SCROLL_DOWN" , SCROLL_DOWN , } ,
	{ "INSERT_SELECTION" , INSERT_SELECTION , } ,
	{ "EXIT_PROGRAM" , EXIT_PROGRAM , } ,

	/* obsoleted: alias of OPEN_SCREEN */
	{ "NEW_PTY" , OPEN_SCREEN , } ,
} ;


/* --- static functions --- */

static int
parse(
	x_keymap_t *  keymap ,
	char *  key ,
	char *  oper
	)
{
	char *  p ;
	KeySym  ksym ;
	u_int  state ;
	int  count ;

	state = 0 ;

	while( ( p = strchr( key , '+')) != NULL)
	{
		*(p ++) = '\0' ;

		if( strcmp( key , "Control") == 0)
		{
			state |= ControlMask ;
		}
		else if( strcmp( key , "Shift") == 0)
		{
			state |= ShiftMask ;
		}
		else if( strcmp( key , "Mod") == 0)
		{
			state |= ModMask ;
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG " unrecognized mask(%s)\n" , key) ;
		}
	#endif

		key = p ;
	}

	if( ( ksym = XStringToKeysym( key)) == NoSymbol)
	{
		return  0 ;
	}

	if( *oper == '"')
	{
		char *  str ;
		char *  p ;

		if( ( str = malloc( strlen( oper))) == NULL)
		{
			return  0 ;
		}

		p = str ;

		oper ++ ;

		while( *oper != '"' && *oper != '\0')
		{
			int  digit ;

			if( sscanf( oper , "\\x%2x" , &digit) == 1)
			{
				*p = (char)digit ;

				oper += 4 ;
			}
			else
			{
				*p = *oper ;

				oper ++ ;
			}

			p ++ ;
		}

		*p = '\0' ;

		if( ( keymap->str_map = realloc( keymap->str_map ,
				sizeof( x_str_key_t) * (keymap->str_map_size + 1))) == NULL)
		{
			free( str) ;

			return  0 ;
		}

		keymap->str_map[keymap->str_map_size].ksym = ksym ;
		keymap->str_map[keymap->str_map_size].state = state ;
		keymap->str_map[keymap->str_map_size].str = str ;
		keymap->str_map_size ++ ;

		return  1 ;
	}
	
	for( count = 0 ; count < sizeof( key_func_table) / sizeof( key_func_table_t) ; count ++)
	{
		if( strcmp( oper , key_func_table[count].name) == 0)
		{
			if( strcmp( key , "UNUSED") == 0)
			{
				keymap->map[key_func_table[count].func].is_used = 0 ;
			}
			else
			{
				keymap->map[key_func_table[count].func].ksym = ksym ;
				keymap->map[key_func_table[count].func].is_used = 1 ;
			}

			keymap->map[key_func_table[count].func].state = state ;

			return  1 ;
		}
	}

	return  0 ;
}


/* --- global functions --- */

int
x_keymap_init(
	x_keymap_t *  keymap
	)
{
	x_key_t  default_key_map[] =
	{
		/* XIM_OPEN */
		{ XK_space , ShiftMask , 1 , } ,

		/* XIM_CLOSE(not used) */
		{ 0 , 0 , 0 , } ,

		/* OPEN SCREEN */
		{ XK_F1 , ControlMask , 1 , } ,

		/* OPEN PTY */
		{ XK_F2 , ControlMask , 1 , } ,

		/* PAGE_UP(compatible with kterm) */
		{ XK_Prior , ShiftMask , 1 , } ,

		/* PAGE_DOWN(compatible with kterm) */
		{ XK_Next , ShiftMask , 1 , } ,

		/* SCROLL_UP */
		{ XK_Up , ShiftMask , 1 , } ,

		/* SCROLL_DOWN */
		{ XK_Down , ShiftMask , 1 , } ,

		/* INSERT_SELECTION */
		{ XK_Insert , ShiftMask , 1 , } ,

		/* EXIT PROGRAM(only for debug) */
		{ XK_F1 , ControlMask | ShiftMask , 1 , } ,
	} ;

	memcpy( &keymap->map , &default_key_map , sizeof( default_key_map)) ;

	keymap->str_map = NULL ;
	keymap->str_map_size = 0 ;

	return  1 ;
}

int
x_keymap_final(
	x_keymap_t *  keymap
	)
{
	int  count ;
	
	free( keymap->str_map) ;

	for( count = 0 ; count < keymap->str_map_size ; count ++)
	{
		free( keymap->str_map[count].str) ;
	}

	return  1 ;
}

int
x_keymap_read_conf(
	x_keymap_t *  keymap ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  key ;
	char *  value ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &key , &value))
	{
		/*
		 * [shortcut key]=[operation]
		 */
		if( ! parse( keymap , key , value))
		{
			/*
			 * XXX
			 * Backward compatibility with 2.4.0 or before.
			 * [operation]=[shortcut key]
			 */
			 
			parse( keymap , value , key) ;
		}
	}

	kik_file_close( from) ;
	
	return  1 ;
}

int
x_keymap_match(
	x_keymap_t *  keymap ,
	x_key_func_t  func ,
	KeySym  ksym ,
	u_int  state
	)
{
	if( keymap->map[func].is_used == 0)
	{
		return  0 ;
	}
	
	if( keymap->map[func].state != 0)
	{
		/* ingoring except ModMask / ControlMask / ShiftMask */
		state &= (ModMask | ControlMask | ShiftMask) ;
		
		if( state & ModMask)
		{
			/* all ModNMasks are set. */
			state |= ModMask ;
		}

		if( state != keymap->map[func].state)
		{
			return  0 ;
		}
	}
	
	if( keymap->map[func].ksym == ksym)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

char *
x_keymap_str(
	x_keymap_t *  keymap ,
	KeySym  ksym ,
	u_int  state
	)
{
	int  count ;

	/* ingoring except ModMask / ControlMask / ShiftMask */
	state &= (ModMask | ControlMask | ShiftMask) ;

	if( state & ModMask)
	{
		/* all ModNMasks are set. */
		state |= ModMask ;
	}
	
	for( count = 0 ; count < keymap->str_map_size ; count ++)
	{
		if( keymap->str_map[count].state == state &&
			keymap->str_map[count].ksym == ksym)
		{
			return  keymap->str_map[count].str ;
		}
	}

	return  NULL ;
}
