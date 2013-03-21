/*
 *	$Id$
 */

#include  "x_shortcut.h"

#include  <stdio.h>		/* sscanf */
#include  <string.h>		/* strchr/memcpy */
#include  <kiklib/kik_def.h>	/* HAVE_WINDOWS_H */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_str.h>	/* strdup */


/*
 * !! Notice !!
 * Mod1Mask - Mod5Mask are not distinguished.
 */


typedef struct  key_func_table
{
	char *  name ;
	x_key_func_t  func ;
	
} key_func_table_t ;


/* --- static variables --- */

static char *  key_file = "mlterm/key" ;


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
	{ "SWITCH_OSC52" , SWITCH_OSC52 , } ,
	{ "EXIT_PROGRAM" , EXIT_PROGRAM , } ,

	/* obsoleted: alias of OPEN_SCREEN */
	{ "NEW_PTY" , OPEN_SCREEN , } ,
} ;


/* --- static functions --- */

static int
read_conf(
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
	#ifdef  ENABLE_BACKWARD_COMPAT
		/*
		 * XIM_OPEN and XIM_CLOSE are removed.
		 */
		if( strcmp( value, "XIM_OPEN") == 0 ||
		    strcmp( value, "XIM_CLOSE") == 0)
		{
			/* This warning will be removed. */
			kik_msg_printf( "%s in %s is no longer supported\n",
					 value , filename);
		}
	#endif

		/*
		 * [shortcut key]=[operation]
		 */
		if( ! x_shortcut_parse( shortcut , key , value))
		{
			/*
			 * XXX
			 * Backward compatibility with 2.4.0 or before.
			 * [operation]=[shortcut key]
			 */
			 
			x_shortcut_parse( shortcut , value , key) ;
		}
	}

	kik_file_close( from) ;
	
	return  1 ;
}


/* --- global functions --- */

int
x_shortcut_init(
	x_shortcut_t *  shortcut
	)
{
	char *  rcpath ;
	
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

		/* SWITCH_OSC52 */
		{ 0 , 0 , 0 , } ,

	#ifdef  DEBUG
		/* EXIT PROGRAM(only for debug) */
		{ XK_F1 , ControlMask | ShiftMask , 1 , } ,
	#else
		{ 0 , 0 , 0 , } ,
	#endif
	} ;

	memcpy( &shortcut->map , &default_key_map , sizeof( default_key_map)) ;

	if( ( shortcut->str_map = malloc( 2 * sizeof(x_str_key_t))))
	{
		shortcut->str_map_size = 2 ;

		shortcut->str_map[0].ksym = 0 ;
		shortcut->str_map[0].state = Button1Mask | ControlMask ;
		shortcut->str_map[0].str = strdup( "menu:mlterm-menu"
						#ifdef  HAVE_WINDOWS_H
							".exe"
						#endif
						) ;
		shortcut->str_map[1].ksym = 0 ;
		shortcut->str_map[1].state = Button3Mask | ControlMask ;
		shortcut->str_map[1].str = strdup( "menu:mlconfig"
						#ifdef  HAVE_WINDOWS_H
							".exe"
						#endif
						) ;
	}
	else
	{
		shortcut->str_map_size = 0 ;
	}

	if( ( rcpath = kik_get_sys_rc_path( key_file)))
	{
		read_conf( shortcut , rcpath) ;
		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( key_file)))
	{
		read_conf( shortcut , rcpath) ;
		free( rcpath) ;
	}

	return  1 ;
}

int
x_shortcut_final(
	x_shortcut_t *  shortcut
	)
{
	u_int  count ;
	
	for( count = 0 ; count < shortcut->str_map_size ; count ++)
	{
		free( shortcut->str_map[count].str) ;
	}

	free( shortcut->str_map) ;

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
		/* ingoring except these masks */
		state &= (ModMask|ControlMask|ShiftMask|ButtonMask) ;
		
		if( ((shortcut->map[func].state & ModMask) == ModMask) &&
		     (state & ModMask))
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
	u_int  count ;

	/* ingoring except these masks */
	state &= (ModMask|ControlMask|ShiftMask|ButtonMask) ;

	for( count = 0 ; count < shortcut->str_map_size ; count ++)
	{
                if( shortcut->str_map[count].ksym == ksym &&
		    shortcut->str_map[count].state ==
			( state |
			  ( (state & ModMask) &&
			    (shortcut->str_map[count].state & ModMask) == ModMask ?
			    ModMask : 0)) )
		{
			return  shortcut->str_map[count].str ;
		}
	}

	return  NULL ;
}

int
x_shortcut_parse(
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
		else if( strncmp( key , "Mod" , 3) == 0)
		{
			switch( key[3])
			{
			case 0:
				state |= ModMask ;
				break;
			case '1':
				state |= Mod1Mask ;
				break;
			case '2':
				state |= Mod2Mask ;
				break;
			case '3':
				state |= Mod3Mask ;
				break;
			case '4':
				state |= Mod4Mask ;
				break;
			case '5':
				state |= Mod5Mask ;
				break;
		#ifdef  DEBUG
			default:
				kik_warn_printf( KIK_DEBUG_TAG
					" unrecognized Mod mask(%s)\n" , key) ;
				break;
		#endif
			}
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG " unrecognized mask(%s)\n" , key) ;
		}
	#endif

		key = p ;
	}

	if( strncmp( key , "Button" , 6) == 0)
	{
		state |= (Button1Mask << (key[6] - '1')) ;
		ksym = 0 ;
	}
	else if( ( ksym = XStringToKeysym( key)) == NoSymbol)
	{
		return  0 ;
	}

	for( count = 0 ; count < sizeof( key_func_table) / sizeof( key_func_table_t) ; count ++)
	{
		x_key_t *  map_entry ;

		map_entry = shortcut->map + key_func_table[count].func ;
		if( map_entry->ksym == ksym &&
		    map_entry->state == state)
		{
			map_entry->is_used = 0 ;
			break ;
		}
	}

	for( count = 0 ; count < shortcut->str_map_size ; count++)
	{
		if( shortcut->str_map[count].ksym == ksym &&
		    shortcut->str_map[count].state == state)
		{
			free( shortcut->str_map[count].str) ;
			shortcut->str_map[count] = shortcut->str_map[-- shortcut->str_map_size] ;
			break ;
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

		if( ! ( str_map = realloc( shortcut->str_map ,
				   sizeof( x_str_key_t) * (shortcut->str_map_size + 1))))
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
