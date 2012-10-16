/*
 *	$Id$
 */

#include  "x.h"

#include  <stdio.h>		/* sscanf */
#include  <string.h>		/* strcmp */
#include  <kiklib/kik_types.h>	/* size_t */

#if  0
#define  SELF_TEST
#endif


#define  TABLE_SIZE  (sizeof(keysym_table) / sizeof(keysym_table[0]))


static struct
{
	char *  str ;
	KeySym /* WORD */ ksym ;	/* 16bit */

} keysym_table[] =
{
	{ "A" , 0x41 } ,
	{ "B" , 0x42 } ,
	{ "BackSpace" , XK_BackSpace } ,
	{ "C" , 0x43 } ,
	{ "D" , 0x44 } ,
	{ "Delete" , XK_Delete } ,
	{ "Down" , XK_Down } ,
	{ "E" , 0x45 } ,
	{ "End" , XK_End } ,
	{ "Escape" , XK_Escape } ,
	{ "F" , 0x46 } ,
	{ "F1" , XK_F1 } ,
	{ "F10" , XK_F10 } ,
	{ "F11" , XK_F11 } ,
	{ "F12" , XK_F12 } ,
	{ "F13" , XK_F13 } ,
	{ "F14" , XK_F14 } ,
	{ "F15" , XK_F15 } ,
	{ "F16" , XK_F16 } ,
	{ "F17" , XK_F17 } ,
	{ "F18" , XK_F18 } ,
	{ "F19" , XK_F19 } ,
	{ "F2" , XK_F2 } ,
	{ "F20" , XK_F20 } ,
	{ "F21" , XK_F21 } ,
	{ "F22" , XK_F22 } ,
	{ "F23" , XK_F23 } ,
	{ "F24" , XK_F24 } ,
	{ "F3" , XK_F3 } ,
	{ "F4" , XK_F4 } ,
	{ "F5" , XK_F5 } ,
	{ "F6" , XK_F6 } ,
	{ "F7" , XK_F7 } ,
	{ "F8" , XK_F8 } ,
	{ "F9" , XK_F9 } ,
	{ "G" , 0x47 } ,
	{ "H" , 0x48 } ,
	{ "Home" , XK_Home } ,
	{ "I" , 0x49 } ,
	{ "Insert" , XK_Insert } ,
	{ "J" , 0x4a } ,
	{ "K" , 0x4b } ,
	{ "L" , 0x4c } ,
	{ "Left" , XK_Left } ,
	{ "M" , 0x4d } ,
	{ "N" , 0x4e } ,
	{ "Next" , XK_Next } ,
	{ "O" , 0x4f } ,
	{ "P" , 0x50 } ,
	{ "Prior" , XK_Prior } ,
	{ "Q" , 0x51 } ,
	{ "R" , 0x52 } ,
	{ "Return" , XK_Return } ,
	{ "Right" , XK_Right } ,
	{ "S" , 0x53 } ,
	{ "T" , 0x54 } ,
	{ "Tab" , XK_Tab } ,
	{ "U" , 0x55 } ,
	{ "Up" , XK_Up } ,
	{ "V" , 0x56 } ,
	{ "W" , 0x57 } ,
	{ "X" , 0x58 } ,
	{ "Y" , 0x59 } ,
	{ "Z" , 0x5a } ,
	{ "space" , 0x20 /* VK_SPACE */ } ,
} ;


/* --- global functions --- */

int
XParseGeometry(
	char *  str ,
	int *  x ,
	int *  y ,
	unsigned int  *  width ,
	unsigned int  *  height
	)
{
	if( sscanf( str , "%ux%u+%d+%d" , width , height , x , y) == 4)
	{
		return  XValue|YValue|WidthValue|HeightValue ;
	}
	else if( sscanf( str , "%ux%u" , width , height) == 2)
	{
		return  WidthValue|HeightValue ;
	}
	else if( sscanf( str , "+%d+%d" , x , y) == 2)
	{
		return  XValue|YValue ;
	}
	else
	{
		return  0 ;
	}
}

KeySym
XStringToKeysym(
	char *  str
	)
{
#ifdef  SELF_TEST
	int  debug_count = 0 ;
#endif
	size_t  prev_idx ;
	size_t  idx ;
	size_t  distance ;

	prev_idx = -1 ;

	/* +1 => roundup */
	idx = (TABLE_SIZE + 1) / 2 ;

	/* idx + distance == TABLE_SIZE - 1 */
	distance = TABLE_SIZE - idx - 1 ;

	while( 1)
	{
		int  cmp ;

		if( ( cmp = strcmp( keysym_table[idx].str , str)) == 0)
		{
		#ifdef  SELF_TEST
			fprintf( stderr , "%.2d/%.2d:" , debug_count , TABLE_SIZE) ;
		#endif

			return  keysym_table[idx].ksym ;
		}
		else
		{
			size_t  next_idx ;

		#ifdef  SELF_TEST
			debug_count ++ ;
		#endif

			/* +1 => roundup */
			if( ( distance = (distance + 1) / 2) == 0)
			{
				break ;
			}

			if( cmp > 0)
			{
				if( idx < distance)
				{
					/* idx - distance == 0 */
					distance = idx ;
				}

				next_idx = idx - distance ;
			}
			else /* if( cmp < 0) */
			{
				if( idx + distance >= TABLE_SIZE)
				{
					/* idx + distance == TABLE_SIZE - 1 */
					distance = TABLE_SIZE - idx - 1 ;
				}

				next_idx = idx + distance ;
			}

			if( next_idx == prev_idx)
			{
				break ;
			}

			prev_idx = idx ;
			idx = next_idx ;
		}
	}

	return  NoSymbol ;
}

#ifdef  SELF_TEST

int
main()
{
	size_t  count ;

	for( count = 0 ; count < TABLE_SIZE ; count++)
	{
		fprintf( stderr , "%x %x\n" ,
			XStringToKeysym( keysym_table[count].str) ,
			keysym_table[count].ksym) ;
		/*
		 * stderr isn't flushed without fflush() if
		 * XStringToKeysym() falls to infinite-loop.
		 */
		fflush( stderr) ;
	}

	fprintf( stderr , "%x\n" , XStringToKeysym( "a")) ; fflush( stderr) ;
	fprintf( stderr , "%x\n" , XStringToKeysym( "hoge")) ; fflush( stderr) ;
	fprintf( stderr , "%x\n" , XStringToKeysym( "zzzz")) ; fflush( stderr) ;

	return  1 ;
}
#endif
