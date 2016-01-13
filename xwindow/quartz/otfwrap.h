/*
 *	$Id$
 */

#ifndef  __OTFWRAP_H__
#define  __OTFWRAP_H__


#include  <kiklib/kik_types.h>


void *  otf_open( const char *  name) ;

void  otf_close( void *  otf) ;

u_int  otf_convert_text_to_glyphs( void *  otf , u_int32_t *  gsub , u_int  gsub_len ,
	u_int32_t *  cmap , u_int32_t *  src , u_int  src_len ,
	const char *  script , const char *  features) ;


#endif	/* __OTFWRAP_H__ */

