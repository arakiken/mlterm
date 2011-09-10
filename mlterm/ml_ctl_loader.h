/*
 *	$Id$
 */

#ifndef  __ML_CTL_LOADER_H__
#define  __ML_CTL_LOADER_H__


typedef enum  ml_ctl_bidi_id
{
	ML_LINE_SET_USE_BIDI ,
	ML_BIDI_CONVERT_LOGICAL_CHAR_INDEX_TO_VISUAL ,
	ML_BIDI_CONVERT_VISUAL_CHAR_INDEX_TO_LOGICAL ,
	ML_LINE_BIDI_COPY_LOGICAL_STR ,
	ML_LINE_IS_RTL ,
	ML_LOGVIS_BIDI_NEW ,
	ML_ARABIC_SHAPE_NEW ,
	ML_IS_ARABIC_COMBINING ,
	ML_BIDI_COPY ,
	ML_BIDI_RESET ,
	MAX_CTL_BIDI_FUNCS ,

} ml_ctl_bidi_id_t ;

typedef enum  ml_ctl_iscii_id
{
	ML_ISCIIKEY_STATE_NEW ,
	ML_ISCIIKEY_STATE_DELETE ,
	ML_CONVERT_ASCII_TO_ISCII ,
	ML_LINE_SET_USE_ISCII ,
	ML_ISCII_CONVERT_LOGICAL_CHAR_INDEX_TO_VISUAL ,
	ML_LOGVIS_ISCII_NEW ,
	ML_ISCII_SHAPE_NEW ,
	ML_ISCII_COPY ,
	ML_ISCII_RESET ,
	MAX_CTL_ISCII_FUNCS ,

} ml_ctl_iscii_id_t ;

void *  ml_load_ctl_bidi_func( ml_ctl_bidi_id_t  id) ;

void *  ml_load_ctl_iscii_func( ml_ctl_iscii_id_t  id) ;


#endif
