/*
 *	update: <2001/10/23(03:28:04)>
 *	$Id$
 */

#ifndef  __MKF_JA_JP_MAP_H__
#define  __MKF_JA_JP_MAP_H__


#include  "mkf_char.h"


int  mkf_map_ucs4_to_ja_jp( mkf_char_t *  jajp , mkf_char_t *  ucs4) ;

int  mkf_map_jisx0213_2000_1_to_jisx0208_1983( mkf_char_t *  jis2k , mkf_char_t *  jis83) ;

int  mkf_map_jisx0208_1983_to_jisx0213_2000_1( mkf_char_t *  jis83 , mkf_char_t *  jis2k) ;

int  mkf_map_jisx0213_2000_2_to_jisx0212_1990( mkf_char_t *  jis2k , mkf_char_t *  jis90) ;

int  mkf_map_jisx0212_1990_to_jisx0213_2000_2( mkf_char_t *  jis90 , mkf_char_t *  jis2k) ;


int  mkf_map_ibm_ext_to_jisx0208_1983( mkf_char_t *  jis , mkf_char_t *  sjis_ibm) ;

int  mkf_map_ibm_ext_to_jisx0212_1990( mkf_char_t *  jis , mkf_char_t *  sjis_ibm) ;

int  mkf_map_ibm_ext_to_jisx0213_2000( mkf_char_t *  jis , mkf_char_t *  sjis_ibm) ;

int  mkf_map_jisx0212_1990_to_ibm_ext( mkf_char_t *  sjis_ibm , mkf_char_t *  jis) ;

int  mkf_map_jisx0213_2000_to_ibm_ext( mkf_char_t *  sjis_ibm , mkf_char_t *  jis) ;


int  mkf_map_nec_ext_to_jisx0208_1983( mkf_char_t *  jis , mkf_char_t *  nec_ext) ;

int  mkf_map_nec_ext_to_jisx0212_1990( mkf_char_t *  jis , mkf_char_t *  nec_ext) ;

int  mkf_map_nec_ext_to_jisx0213_2000( mkf_char_t *  jis , mkf_char_t *  nec_ext) ;

int  mkf_map_jisx0212_1990_to_nec_ext( mkf_char_t *  nec_ext , mkf_char_t *  jis) ;

int  mkf_map_jisx0213_2000_to_nec_ext( mkf_char_t *  nec_ext , mkf_char_t *  jis) ;


int  mkf_map_necibm_ext_to_jisx0208_1983( mkf_char_t *  jis , mkf_char_t *  ibm) ;

int  mkf_map_necibm_ext_to_jisx0212_1990( mkf_char_t *  jis , mkf_char_t *  ibm) ;

int  mkf_map_necibm_ext_to_jisx0213_2000( mkf_char_t *  jis , mkf_char_t *  ibm) ;

int  mkf_map_jisx0212_1990_to_necibm_ext( mkf_char_t *  ibm , mkf_char_t *  jis) ;

int  mkf_map_jisx0213_2000_to_necibm_ext( mkf_char_t *  ibm , mkf_char_t *  jis) ;


int  mkf_map_mac_ext_to_jisx0208_1983( mkf_char_t *  jis , mkf_char_t *  mac) ;

int  mkf_map_mac_ext_to_jisx0212_1990( mkf_char_t *  jis , mkf_char_t *  mac) ;

int  mkf_map_mac_ext_to_jisx0213_2000( mkf_char_t *  jis , mkf_char_t *  mac) ;

int  mkf_map_jisx0212_1990_to_mac_ext( mkf_char_t *  mac , mkf_char_t *  jis) ;

int  mkf_map_jisx0213_2000_to_mac_ext( mkf_char_t *  mac , mkf_char_t *  jis) ;


#endif
