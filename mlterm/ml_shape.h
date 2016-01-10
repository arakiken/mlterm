/*
 *	$Id$
 */

#ifndef  __ML_SHAPING_H__
#define  __ML_SHAPING_H__


#include  <kiklib/kik_types.h>	/* u_int */

#include  "ml_char.h"
#include  "ml_iscii.h"
#include  "ml_line.h"	/* ctl_info_t */


#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)

u_int  ml_shape_arabic( ml_char_t *  dst , u_int  dst_len , ml_char_t *  src , u_int  src_len) ;

u_int16_t  ml_is_arabic_combining( ml_char_t *  prev2 , ml_char_t *  prev , ml_char_t *  ch) ;

#else

#define  ml_shape_arabic  (NULL)
#define  ml_is_arabic_combining( a , b , c)  (0)

#endif

#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)

u_int  ml_shape_iscii( ml_char_t *  dst , u_int  dst_len , ml_char_t *  src , u_int  src_len) ;

#else

#define  ml_shape_iscii  (NULL)

#endif

u_int  ml_shape_gsub( ml_char_t *  dst , u_int  dst_len ,
		ml_char_t *  src , u_int  src_len , ctl_info_t  ctl_info) ;


#endif	/* __ML_SHAPING_H__ */
