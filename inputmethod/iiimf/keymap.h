/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __IM_IIIMF_KEYMAP_H__
#define __IM_IIIMF_KEYMAP_H__

int xksym_to_iiimfkey(KeySym ksym, IIIMP_int32 *kchar, IIIMP_int32 *kcode);

#if 0
int xksym_to_iiimfkey_kana(KeySym ksym, IIIMP_int32 *kchar, IIIMP_int32 *kcode);

int xksym_to_iiimfkey_kana_shift(KeySym ksym, IIIMP_int32 *kchar, IIIMP_int32 *kcode);
#endif

#endif
