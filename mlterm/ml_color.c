/*
 *	$Id$
 */

#include  "ml_color.h"

#include  <stdio.h>		/* NULL */


/* --- static variables --- */

static char *  color_name_table[] =
{
	"black" ,
	"red" ,
	"green" ,
	"yellow" ,
	"blue" ,
	"magenta" ,
	"cyan" ,
	"white" ,
} ;


/* --- global functions --- */

char *
ml_get_color_name(
	ml_color_t  color
	)
{
	if( ML_BLACK <= color && color <= (ML_WHITE | ML_BOLD_COLOR_MASK))
	{
		return  color_name_table[color & ~ML_BOLD_COLOR_MASK] ;
	}
	else
	{
		return  NULL ;
	}
}
