/*
 *	$Id$
 */

#include  "ml_keymap.h"

#include  <string.h>		/* strchr */
#include  <X11/keysym.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>


typedef struct  key_func_table
{
	char *  name ;
	ml_key_func_t  func ;
	
} key_func_table_t ;


/* --- static functions --- */

static key_func_table_t  key_func_table[] =
{
	{ "XIM_OPEN" , XIM_OPEN , } ,
	{ "XIM_CLOSE" , XIM_CLOSE , } ,
	{ "NEW_PTY" , NEW_PTY , } ,
	{ "PAGE_UP" , PAGE_UP , } ,
	{ "SCROLL_UP" , SCROLL_UP , } ,
	{ "INSERT_SELECTION" , INSERT_SELECTION , } ,
	{ "EXIT_PROGRAM" , EXIT_PROGRAM , } ,
} ;


/* --- global functions --- */

int
ml_keymap_init(
	ml_keymap_t *  keymap
	)
{
	ml_key_t  default_key_map[] =
	{
		/* XIM_OPEN */
		{ XK_space , ShiftMask , 1 , } ,

		/* XIM_CLOSE(not used) */
		{ 0 , 0 , 0 , } ,

		/* NEW PTY */
		{ XK_F1 , ControlMask , 1 , } ,

		/* PAGE_UP(compatible with kterm) */
		{ XK_Prior , ShiftMask , 1 , } ,

		/* SCROLL_UP */
		{ XK_Up , ShiftMask , 1 , } ,

		/* INSERT_SELECTION */
		{ XK_Insert , ShiftMask , 1 , } ,

		/* EXIT PROGRAM(only for debug) */
		{ XK_F1 , ControlMask | ShiftMask , 1 , } ,
	} ;

	memcpy( &keymap->map , &default_key_map , sizeof( default_key_map)) ;

	return  1 ;
}

int
ml_keymap_final(
	ml_keymap_t *  keymap
	)
{
	return  1 ;
}

int
ml_keymap_read_conf(
	ml_keymap_t *  keymap ,
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
		char *  p ;
		u_int  state ;
		int  counter ;

		state = 0 ;
		
		while( ( p = strchr( value , '+')) != NULL)
		{
			*(p ++) = '\0' ;

			if( strcmp( value , "Control") == 0)
			{
				state |= ControlMask ;
			}
			else if( strcmp( value , "Shift") == 0)
			{
				state |= ShiftMask ;
			}
			else if( strcmp( value , "Mod") == 0)
			{
				state |= ModMask ;
			}
		#ifdef  DEBUG
			else
			{
				kik_warn_printf( KIK_DEBUG_TAG " unrecognized mask(%s)\n" , value) ;
			}
		#endif

			value = p ;
		}
		
		for( counter = 0 ; counter < sizeof( key_func_table) / sizeof( key_func_table_t) ;
			counter ++)
		{
			if( strcmp( key , key_func_table[counter].name) == 0)
			{
				if( strcmp( value , "UNUSED") == 0)
				{
					keymap->map[key_func_table[counter].func].is_used = 0 ;
				}
				else
				{
					keymap->map[key_func_table[counter].func].ksym =
						XStringToKeysym( value) ;
					keymap->map[key_func_table[counter].func].is_used = 1 ;
				}

				keymap->map[key_func_table[counter].func].state = state ;

				break ;
			}
		}
	}

	kik_file_close( from) ;
	
	return  1 ;
}

int
ml_keymap_match(
	ml_keymap_t *  keymap ,
	ml_key_func_t  func ,
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
