#ifndef __IM_SCIM_H__
#define __IM_SCIM_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The header files of kiklib must be included here, since it does not
 * supports c++.
 */
#include  <kiklib/kik_debug.h>		/* kik_*_printf() */
#include  <kiklib/kik_config.h>		/* u_int */

#define  CHAR_ATTR_UNDERLINE	(1U)
#define  CHAR_ATTR_REVERSE	(1U << 1)
#define  CHAR_ATTR_BOLD		(1U << 2)

typedef void * im_scim_context_t ;

/* callbacks */
typedef struct  im_scim_callbacks
{
	void  (*commit)( void * , char *) ;
	void  (*preedit_update)( void * , char * , int) ;
	void  (*candidate_update)( void * , int , u_int , char ** , int) ;
	void  (*candidate_show)( void *) ;
	void  (*candidate_hide)( void *) ;
	void  (*im_changed)( void * , char *) ;

}  im_scim_callbacks_t ;

int  im_scim_initialize( void) ;
int  im_scim_finalize( void) ;

im_scim_context_t  im_scim_create_context( char *  factory ,
					   char *  locale ,
					   void *  self ,
					   im_scim_callbacks_t *  callbacks) ;

int  im_scim_destroy_context(  im_scim_context_t  context) ;

int  im_scim_key_event( im_scim_context_t  context ,
			KeySym  ksym ,
			XKeyEvent *  event) ;

int  im_scim_focused( im_scim_context_t  context) ;
int  im_scim_unfocused( im_scim_context_t  context) ;

u_int  im_scim_preedit_char_attr( im_scim_context_t  context , u_int  index) ;

int  im_scim_get_panel_fd( void) ;
int  im_scim_receive_panel_event( void) ;

u_int  im_scim_get_number_of_factory(void) ;
char *  im_scim_get_default_factory_name( char *  locale) ;
char *  im_scim_get_factory_name( int  index) ;
char *  im_scim_get_language( int  index) ;

#ifdef __cplusplus
}
#endif

#endif

