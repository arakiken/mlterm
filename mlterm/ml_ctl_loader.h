/*
 *	$Id$
 */

#ifndef  __ML_CTL_LOADER_H__
#define  __ML_CTL_LOADER_H__


#include  "ml_line.h"
#include  "ml_logical_visual.h"
#include  "ml_shape.h"


typedef enum  ml_ctl_bidi_id
{
	CTL_BIDI_API_COMPAT_CHECK ,
	ML_LINE_SET_USE_BIDI ,
	ML_LINE_BIDI_CONVERT_LOGICAL_CHAR_INDEX_TO_VISUAL ,
	ML_LINE_BIDI_CONVERT_VISUAL_CHAR_INDEX_TO_LOGICAL ,
	ML_LINE_BIDI_COPY_LOGICAL_STR ,
	ML_LINE_BIDI_IS_RTL ,
	ML_LOGVIS_BIDI_NEW ,
	ML_ARABIC_SHAPE_NEW ,
	ML_IS_ARABIC_COMBINING ,
	ML_BIDI_COPY ,
	ML_BIDI_RESET ,
	MAX_CTL_BIDI_FUNCS ,

} ml_ctl_bidi_id_t ;

typedef enum  ml_ctl_iscii_id
{
	CTL_ISCII_API_COMPAT_CHECK ,
	ML_ISCIIKEY_STATE_NEW ,
	ML_ISCIIKEY_STATE_DELETE ,
	ML_CONVERT_ASCII_TO_ISCII ,
	ML_LINE_SET_USE_ISCII ,
	ML_LINE_ISCII_CONVERT_LOGICAL_CHAR_INDEX_TO_VISUAL ,
	ML_LOGVIS_ISCII_NEW ,
	ML_ISCII_SHAPE_NEW ,
	ML_ISCII_COPY ,
	ML_ISCII_RESET ,
	MAX_CTL_ISCII_FUNCS ,

} ml_ctl_iscii_id_t ;


#define  CTL_API_VERSION  0x01
#define  CTL_API_COMPAT_CHECK_MAGIC			\
	 (((CTL_API_VERSION & 0x0f) << 28) |		\
	  ((sizeof( ml_line_t) & 0xff) << 20) |		\
	  ((sizeof( ml_logical_visual_t) & 0xff) << 12) | \
	  (sizeof( ml_shape_t) & 0xfff))


void *  ml_load_ctl_bidi_func( ml_ctl_bidi_id_t  id) ;

void *  ml_load_ctl_iscii_func( ml_ctl_iscii_id_t  id) ;


#endif
