/*
 *	$Id$
 */

#include  "ml_drcs.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_debug.h>


/* --- static variables --- */

static ml_drcs_t *  drcs_fonts ;
static u_int  num_of_drcs_fonts ;
static mkf_charset_t  cached_font_cs = UNKNOWN_CS ;


/* --- global functions --- */

ml_drcs_t *
ml_drcs_get(
	mkf_charset_t  cs ,
	int  create
	)
{
	static ml_drcs_t *  cached_font ;
	u_int  count ;
	void *  p ;

	if( cs == cached_font_cs)
	{
		if( cached_font || ! create)
		{
			return  cached_font ;
		}
	}
	else
	{
		for( count = 0 ; count < num_of_drcs_fonts ; count++)
		{
			if( drcs_fonts[count].cs == cs)
			{
				return  &drcs_fonts[count] ;
			}
		}
	}

	if( ! create ||
	    /* XXX leaks */
	    ! ( p = realloc( drcs_fonts , sizeof(ml_drcs_t) * (num_of_drcs_fonts + 1))))
	{
		return  NULL ;
	}

	drcs_fonts = p ;
	memset( drcs_fonts + num_of_drcs_fonts , 0 , sizeof(ml_drcs_t)) ;
	cached_font_cs = drcs_fonts[num_of_drcs_fonts].cs = cs ;

	return  (cached_font = &drcs_fonts[num_of_drcs_fonts ++]) ;
}

int
ml_drcs_final(
	mkf_charset_t  cs
	)
{
	u_int  count ;

	for( count = 0 ; count < num_of_drcs_fonts ; count++)
	{
		if( drcs_fonts[count].cs == cs)
		{
			int  idx ;

			for( idx = 0 ; idx < 0x5f ; idx++)
			{
				free( drcs_fonts[count].glyphs[idx]) ;
			}

			memset( drcs_fonts[count].glyphs , 0 , sizeof(drcs_fonts[count].glyphs)) ;

			drcs_fonts[count] = drcs_fonts[--num_of_drcs_fonts] ;

			if( cached_font_cs == cs)
			{
				/* Clear cache in ml_drcs_get(). */
				cached_font_cs = UNKNOWN_CS ;
			}

			return  1 ;
		}
	}

	return  1 ;
}

int
ml_drcs_add(
	ml_drcs_t *  font ,
	int  idx ,
	char *  seq ,
	u_int  width ,
	u_int  height
	)
{
	free( font->glyphs[idx]) ;

	if( ( font->glyphs[idx] = malloc( 2 + strlen(seq) + 1)))
	{
		font->glyphs[idx][0] = width ;
		font->glyphs[idx][1] = height ;
		strcpy( font->glyphs[idx] + 2 , seq) ;
	}

	return  1 ;
}
