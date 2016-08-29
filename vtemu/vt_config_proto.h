/*
 *	$Id$
 */

#ifndef __VT_CONFIG_PROTO_H__
#define __VT_CONFIG_PROTO_H__

/*
 * These functions are exported from vt_term.h.
 */

int vt_config_proto_init(void);

int vt_config_proto_final(void);

int vt_gen_proto_challenge(void);

char* vt_get_proto_challenge(void);

int vt_parse_proto_prefix(char** dev, char** str, int do_challenge);

int vt_parse_proto(char** dev, char** key, char** val, char** str, int do_challenge,
                   int sep_by_semicolon);

#endif
