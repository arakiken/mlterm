/*
 *	$Id$
 */

#include  "x_shortcut.h"

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
	{ "IM_HOTKEY" , IM_HOTKEY , } ,
	{ "EXT_KBD" , EXT_KBD , } ,
	{ "OPEN_SCREEN" , OPEN_SCREEN , } ,
	{ "OPEN_PTY" , OPEN_PTY , } ,
	{ "NEXT_PTY" , NEXT_PTY , } ,
	{ "PREV_PTY" , PREV_PTY , } ,
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
	x_shortcut_t *  shortcut ,
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

	for( count = 0 ; count < sizeof( key_func_table) / sizeof( key_func_table_t) ; count ++)
	{
		x_key_t *  map_entry ;

		map_entry = shortcut->map + key_func_table[count].func ;
		if( (map_entry->ksym == ksym) &&
		    (map_entry->state = state))
		{
			map_entry->is_used = 0 ;
		}
	}


	if( *oper == '"')
	{
		char *  str ;
		char *  p ;
		x_str_key_t *  str_map ;

		if( ( str = malloc( strlen( oper))) == NULL)
		{
			return  0 ;
		}

		p = str ;

		oper ++ ;

		while( *oper != '"' && *oper != '\0')
		{
			u_int  digit ;

			if( sscanf( oper , "\\x%2x" , &digit) == 1)
			{
				*p = (char)digit ;

				oper += 4 ;
			}
			else
			{
				if( *oper == '\\')
				{
					oper ++ ;

					if( *oper == '\0')
					{
						break ;
					}
					else if( *oper == 'n')
					{
						*p = '\n' ;
					}
					else if( *oper == 'r')
					{
						*p = '\r' ;
					}
					else if( *oper == 't')
					{
						*p = '\t' ;
					}
					else if( *oper == 'e')
					{
						*p = '\033' ;
					}
					else
					{
						*p = *oper ;
					}
				}
				else
				{
					*p = *oper ;
				}

				oper ++ ;
			}

			p ++ ;
		}

		*p = '\0' ;

		str_map = realloc( shortcut->str_map ,
				   sizeof( x_str_key_t) * (shortcut->str_map_size + 1)) ;

		if( str_map == NULL)
		{
			free( str) ;

			return  0 ;
		}

		str_map[shortcut->str_map_size].ksym = ksym ;
		str_map[shortcut->str_map_size].state = state ;
		str_map[shortcut->str_map_size].str = str ;

		shortcut->str_map_size ++ ;
		shortcut->str_map = str_map ;

		return  1 ;
	}

	for( count = 0 ; count < sizeof( key_func_table) / sizeof( key_func_table_t) ; count ++)
	{
		if( strcmp( oper , key_func_table[count].name) == 0)
		{
			if( strcmp( key , "UNUSED") == 0)
			{
				shortcut->map[key_func_table[count].func].is_used = 0 ;
			}
			else
			{
				shortcut->map[key_func_table[count].func].ksym = ksym ;
				shortcut->map[key_func_table[count].func].is_used = 1 ;
			}

			shortcut->map[key_func_table[count].func].state = state ;

			return  1 ;
		}
	}

	return  0 ;
}


/* --- global functions --- */

int
x_shortcut_init(
	x_shortcut_t *  shortcut
	)
{
	x_key_t  default_key_map[] =
	{
		/* IM_HOTKEY */
		{ 0 , 0 , 0 , } ,

		/* EXT_KBD(obsolete) */
		{ 0 , 0 , 0 , } ,
		
		/* OPEN_SCREEN */
		{ XK_F1 , ControlMask , 1 , } ,

		/* OPEN_PTY */
		{ XK_F2 , ControlMask , 1 , } ,

		/* NEXT_PTY */
		{ XK_F3 , ControlMask , 1 , } ,

		/* PREV_PTY */
		{ XK_F4 , ControlMask , 1 , } ,

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

	memcpy( &shortcut->map , &default_key_map , sizeof( default_key_map)) ;

	shortcut->str_map = NULL ;
	shortcut->str_map_size = 0 ;

	return  1 ;
}

int
x_shortcut_final(
	x_shortcut_t *  shortcut
	)
{
	int  count ;
	
	for( count = 0 ; count < shortcut->str_map_size ; count ++)
	{
		free( shortcut->str_map[count].str) ;
	}

	free( shortcut->str_map) ;

	return  1 ;
}

int
x_read_shortcut_config(
	x_shortcut_t *  shortcut ,
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
		 * XIM_OPEN and XIM_CLOSE are removed.
		 */
		if( strcmp( value, "XIM_OPEN") == 0 ||
		    strcmp( value, "XIM_CLOSE") == 0)
		{
			/* This warning will be removed. */
			kik_warn_printf( "%s in %s is no longer supported\n",
					 value , filename);
		}

		/*
		 * [shortcut key]=[operation]
		 */
		if( ! parse( shortcut , key , value))
		{
			/*
			 * XXX
			 * Backward compatibility with 2.4.0 or before.
			 * [operation]=[shortcut key]
			 */
			 
			parse( shortcut , value , key) ;
		}
	}

	kik_file_close( from) ;
	
	return  1 ;
}

int
x_shortcut_match(
	x_shortcut_t *  shortcut ,
	x_key_func_t  func ,
	KeySym  ksym ,
	u_int  state
	)
{
	if( shortcut->map[func].is_used == 0)
	{
		return  0 ;
	}
	
	if( shortcut->map[func].state != 0)
	{
		/* ingoring except ModMask / ControlMask / ShiftMask */
		state &= (ModMask | ControlMask | ShiftMask) ;
		
		if( state & ModMask)
		{
			/* all ModNMasks are set. */
			state |= ModMask ;
		}

		if( state != shortcut->map[func].state)
		{
			return  0 ;
		}
	}

	if( shortcut->map[func].ksym == ksym)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

char *
x_shortcut_str(
	x_shortcut_t *  shortcut ,
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
	
	for( count = 0 ; count < shortcut->str_map_size ; count ++)
	{
		if( shortcut->str_map[count].state == state &&
			shortcut->str_map[count].ksym == ksym)
		{
			return  shortcut->str_map[count].str ;
		}
	}

	return  NULL ;
}
