/*
 *	$Id$
 */

#ifndef __EF_JA_JP_MAP_H__
#define __EF_JA_JP_MAP_H__

#include "ef_char.h"

int ef_map_ucs4_to_ja_jp(ef_char_t* jajp, ef_char_t* ucs4);

int ef_map_jisx0213_2000_1_to_jisx0208_1983(ef_char_t* jis2k, ef_char_t* jis83);

int ef_map_jisx0208_1983_to_jisx0213_2000_1(ef_char_t* jis83, ef_char_t* jis2k);

int ef_map_jisx0213_2000_2_to_jisx0212_1990(ef_char_t* jis2k, ef_char_t* jis90);

int ef_map_jisx0212_1990_to_jisx0213_2000_2(ef_char_t* jis90, ef_char_t* jis2k);

int ef_map_sjis_ibm_ext_to_jisx0208_1983(ef_char_t* jis, ef_char_t* ibm);

int ef_map_sjis_ibm_ext_to_jisx0212_1990(ef_char_t* jis, ef_char_t* ibm);

int ef_map_sjis_ibm_ext_to_jisx0213_2000(ef_char_t* jis, ef_char_t* ibm);

int ef_map_jisx0212_1990_to_sjis_ibm_ext(ef_char_t* ibm, ef_char_t* jis);

int ef_map_jisx0213_2000_2_to_sjis_ibm_ext(ef_char_t* ibm, ef_char_t* jis);

int ef_map_jisx0208_nec_ext_to_jisx0208_1983(ef_char_t* jis, ef_char_t* nec_ext);

int ef_map_jisx0208_nec_ext_to_jisx0212_1990(ef_char_t* jis, ef_char_t* nec_ext);

int ef_map_jisx0208_nec_ext_to_jisx0213_2000(ef_char_t* jis, ef_char_t* nec_ext);

int ef_map_jisx0212_1990_to_jisx0208_nec_ext(ef_char_t* nec_ext, ef_char_t* jis);

int ef_map_jisx0213_2000_2_to_jisx0208_nec_ext(ef_char_t* nec_ext, ef_char_t* jis);

int ef_map_jisx0208_necibm_ext_to_jisx0208_1983(ef_char_t* jis, ef_char_t* necibm);

int ef_map_jisx0208_necibm_ext_to_jisx0212_1990(ef_char_t* jis, ef_char_t* necibm);

int ef_map_jisx0208_necibm_ext_to_jisx0213_2000(ef_char_t* jis, ef_char_t* necibm);

int ef_map_jisx0212_1990_to_jisx0208_necibm_ext(ef_char_t* necibm, ef_char_t* jis);

int ef_map_jisx0213_2000_2_to_jisx0208_necibm_ext(ef_char_t* necibm, ef_char_t* jis);

int ef_map_jisx0208_mac_ext_to_jisx0208_1983(ef_char_t* jis, ef_char_t* mac);

int ef_map_jisx0208_mac_ext_to_jisx0212_1990(ef_char_t* jis, ef_char_t* mac);

int ef_map_jisx0208_mac_ext_to_jisx0213_2000(ef_char_t* jis, ef_char_t* mac);

int ef_map_jisx0212_1990_to_jisx0208_mac_ext(ef_char_t* mac, ef_char_t* jis);

int ef_map_jisx0213_2000_2_to_jisx0208_mac_ext(ef_char_t* mac, ef_char_t* jis);

#endif
