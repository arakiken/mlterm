/*
 *	$Id$
 */

#ifndef  __MKF_ZH_CN_MAP_H__
#define  __MKF_ZH_CN_MAP_H__


#include  "mkf_char.h"


int  mkf_map_ucs4_to_zh_cn( mkf_char_t *  zhcn , mkf_char_t *  ucs4) ;

int  mkf_map_gbk_to_gb2312_80( mkf_char_t *  gb2312 , mkf_char_t *  gbk) ;

int  mkf_map_gb2312_80_to_gbk( mkf_char_t *  gbk , mkf_char_t *  gb2312) ;


#endif
