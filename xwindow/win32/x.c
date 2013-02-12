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
	{ "1" , '1' } ,
	{ "2" , '2' } ,
	{ "3" , '3' } ,
	{ "4" , '4' } ,
	{ "5" , '5' } ,
	{ "6" , '6' } ,
	{ "7" , '7' } ,
	{ "8" , '8' } ,
	{ "9" , '9' } ,
	{ "0" , '0' } ,
	{ "BackSpace" , XK_BackSpace } ,
	{ "Delete" , XK_Delete } ,
	{ "Down" , XK_Down } ,
	{ "End" , XK_End } ,
	{ "Escape" , XK_Escape } ,
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
	{ "Henkan_Mode" , XK_Henkan_Mode } ,
	{ "Home" , XK_Home } ,
	{ "Insert" , XK_Insert } ,
	{ "Left" , XK_Left } ,
	{ "Muhenkan" , XK_Muhenkan } ,
	{ "Next" , XK_Next } ,
	{ "Prior" , XK_Prior } ,
	{ "Return" , XK_Return } ,
	{ "Right" , XK_Right } ,
	{ "Tab" , XK_Tab } ,
	{ "Up" , XK_Up } ,
	{ "Zenkaku_Hankaku" , XK_Zenkaku_Hankaku } ,
	{ "a" , 'a' } ,
	{ "b" , 'b' } ,
	{ "c" , 'c' } ,
	{ "d" , 'd' } ,
	{ "e" , 'e' } ,
	{ "f" , 'f' } ,
	{ "g" , 'g' } ,
	{ "h" , 'h' } ,
	{ "i" , 'i' } ,
	{ "j" , 'j' } ,
	{ "k" , 'k' } ,
	{ "l" , 'l' } ,
	{ "m" , 'm' } ,
	{ "n" , 'n' } ,
	{ "o" , 'o' } ,
	{ "p" , 'p' } ,
	{ "q" , 'q' } ,
	{ "r" , 'r' } ,
	{ "s" , 's' } ,
	{ "space" , ' ' } ,
	{ "t" , 't' } ,
	{ "u" , 'u' } ,
	{ "v" , 'v' } ,
	{ "w" , 'w' } ,
	{ "x" , 'x' } ,
	{ "y" , 'y' } ,
	{ "z" , 'z' } ,
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
