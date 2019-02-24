/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* getopt */
#include <pobl/bl_debug.h>

#include <mef/ef_iso8859_parser.h>
#include <mef/ef_xct_parser.h>
#include <mef/ef_8bit_parser.h>
#include <mef/ef_utf32_parser.h>
#include <mef/ef_sjis_parser.h>
#include <mef/ef_eucjp_parser.h>
#include <mef/ef_utf8_parser.h>
#include <mef/ef_iso2022jp_parser.h>
#include <mef/ef_euckr_parser.h>
#include <mef/ef_iso2022kr_parser.h>
#include <mef/ef_johab_parser.h>
#include <mef/ef_euccn_parser.h>
#include <mef/ef_iso2022cn_parser.h>
#include <mef/ef_hz_parser.h>
#include <mef/ef_big5_parser.h>
#include <mef/ef_euctw_parser.h>
#include <mef/ef_utf16_parser.h>

#include <mef/ef_iso8859_conv.h>
#include <mef/ef_xct_conv.h>
#include <mef/ef_8bit_conv.h>
#include <mef/ef_utf32_conv.h>
#include <mef/ef_sjis_conv.h>
#include <mef/ef_eucjp_conv.h>
#include <mef/ef_utf8_conv.h>
#include <mef/ef_iso2022jp_conv.h>
#include <mef/ef_euckr_conv.h>
#include <mef/ef_iso2022kr_conv.h>
#include <mef/ef_johab_conv.h>
#include <mef/ef_euccn_conv.h>
#include <mef/ef_iso2022cn_conv.h>
#include <mef/ef_hz_conv.h>
#include <mef/ef_big5_conv.h>
#include <mef/ef_euctw_conv.h>
#include <mef/ef_utf16_conv.h>

#include <mef/ef_ucs4_map.h>

typedef struct ef_factory_table {
  char *encoding;
  ef_parser_t* (*parser_new)(void);
  ef_conv_t* (*conv_new)(void);

} ef_factory_table_t;

/* --- static variables --- */

static ef_factory_table_t factories[] = {
    {"iso8859-1", ef_iso8859_1_parser_new, ef_iso8859_1_conv_new},
    {"iso8859-2", ef_iso8859_2_parser_new, ef_iso8859_2_conv_new},
    {"iso8859-3", ef_iso8859_3_parser_new, ef_iso8859_3_conv_new},
    {"iso8859-4", ef_iso8859_4_parser_new, ef_iso8859_4_conv_new},
    {"iso8859-5", ef_iso8859_5_parser_new, ef_iso8859_5_conv_new},
    {"iso8859-6", ef_iso8859_6_parser_new, ef_iso8859_6_conv_new},
    {"iso8859-7", ef_iso8859_7_parser_new, ef_iso8859_7_conv_new},
    {"iso8859-8", ef_iso8859_8_parser_new, ef_iso8859_8_conv_new},
    {"iso8859-9", ef_iso8859_9_parser_new, ef_iso8859_9_conv_new},
    {"iso8859-10", ef_iso8859_10_parser_new, ef_iso8859_10_conv_new},
    {"tis620", ef_tis620_2533_parser_new, ef_tis620_2533_conv_new},
    {"iso8859-13", ef_iso8859_13_parser_new, ef_iso8859_13_conv_new},
    {"iso8859-14", ef_iso8859_14_parser_new, ef_iso8859_14_conv_new},
    {"iso8859-15", ef_iso8859_15_parser_new, ef_iso8859_15_conv_new},
    {"iso8859-16", ef_iso8859_16_parser_new, ef_iso8859_16_conv_new},
    {"tcvn5712", ef_tcvn5712_3_1993_parser_new, ef_tcvn5712_3_1993_conv_new},
    {"xct", ef_xct_parser_new, ef_xct_conv_new},
    {"viscii", ef_viscii_parser_new, ef_viscii_conv_new},
    {"koi8-r", ef_koi8_r_parser_new, ef_koi8_r_conv_new},
    {"koi8-u", ef_koi8_u_parser_new, ef_koi8_u_conv_new},
    {"cp1250", ef_cp1250_parser_new, ef_cp1250_conv_new},
    {"cp1251", ef_cp1251_parser_new, ef_cp1251_conv_new},
    {"cp1252", ef_cp1252_parser_new, ef_cp1252_conv_new},
    {"cp1253", ef_cp1253_parser_new, ef_cp1253_conv_new},
    {"cp1254", ef_cp1250_parser_new, ef_cp1254_conv_new},
    {"cp1255", ef_cp1250_parser_new, ef_cp1255_conv_new},
    {"cp1256", ef_cp1250_parser_new, ef_cp1256_conv_new},
    {"cp1257", ef_cp1250_parser_new, ef_cp1257_conv_new},
    {"cp1258", ef_cp1250_parser_new, ef_cp1258_conv_new},
    {"cp874", ef_cp874_parser_new, ef_cp874_conv_new},
    {
     "isciiassamese", ef_iscii_assamese_parser_new, ef_iscii_assamese_conv_new,
    },
    {
     "isciibengali", ef_iscii_bengali_parser_new, ef_iscii_bengali_conv_new,
    },
    {
     "isciigujarati", ef_iscii_gujarati_parser_new, ef_iscii_gujarati_conv_new,
    },
    {
     "isciihindi", ef_iscii_hindi_parser_new, ef_iscii_hindi_conv_new,
    },
    {
     "isciikannada", ef_iscii_kannada_parser_new, ef_iscii_kannada_conv_new,
    },
    {
     "isciimalayalam", ef_iscii_malayalam_parser_new, ef_iscii_malayalam_conv_new,
    },
    {
     "isciioriya", ef_iscii_oriya_parser_new, ef_iscii_oriya_conv_new,
    },
    {
     "isciipunjabi", ef_iscii_punjabi_parser_new, ef_iscii_punjabi_conv_new,
    },
    {
     "isciitamil", ef_iscii_tamil_parser_new, ef_iscii_tamil_conv_new,
    },
    {
     "isciitelugu", ef_iscii_telugu_parser_new, ef_iscii_telugu_conv_new,
    },
    {"eucjp", ef_eucjp_parser_new, ef_eucjp_conv_new},
    {"eucjisx0213", ef_eucjisx0213_parser_new, ef_eucjisx0213_conv_new},
    {"sjis", ef_sjis_parser_new, ef_sjis_conv_new},
    {"sjisx0213", ef_sjisx0213_parser_new, ef_sjisx0213_conv_new},
    {"utf8", ef_utf8_parser_new, ef_utf8_conv_new},
    {"utf16", ef_utf16_parser_new, ef_utf16_conv_new},
    {"utf16le", ef_utf16le_parser_new, ef_utf16le_conv_new},
    {"utf32", ef_utf32_parser_new, ef_utf32_conv_new},
    {"utf32le", ef_utf32le_parser_new, ef_utf32le_conv_new},
    {"junet8", ef_iso2022jp_8_parser_new, ef_iso2022jp_8_conv_new},
    {"junet7", ef_iso2022jp_7_parser_new, ef_iso2022jp_7_conv_new},
    {"iso2022jp2", ef_iso2022jp2_parser_new, ef_iso2022jp2_conv_new},
    {"iso2022jp3", ef_iso2022jp3_parser_new, ef_iso2022jp3_conv_new},
    {"euckr", ef_euckr_parser_new, ef_euckr_conv_new},
    {"uhc", ef_uhc_parser_new, ef_uhc_conv_new},
    {"iso2022kr", ef_iso2022kr_parser_new, ef_iso2022kr_conv_new},
    {"johab", ef_johab_parser_new, ef_johab_conv_new},
    {"euccn", ef_euccn_parser_new, ef_euccn_conv_new},
    {"gbk", ef_gbk_parser_new, ef_gbk_conv_new},
    {"gb18030", ef_gb18030_2000_parser_new, ef_gb18030_2000_conv_new},
    {"iso2022cn", ef_iso2022cn_parser_new, ef_iso2022cn_conv_new},
    {"hz", ef_hz_parser_new, ef_hz_conv_new},
    {"big5", ef_big5_parser_new, ef_big5_conv_new},
    {"big5hkscs", ef_big5hkscs_parser_new, ef_big5hkscs_conv_new},
    {"euctw", ef_euctw_parser_new, ef_euctw_conv_new},
};

/* --- static functions --- */

static void usage() {
  bl_msg_printf("usage: mef -i [input code] -o [output code] ([file])\n");
  bl_msg_printf(
      "supported codes: iso8859-[1-10] tis620 iso8859-[13-16] tcvn5712 xct "
      "viscii iscii koi8-r koi8-u cp1250 cp1251 cp1252 cp1253 cp1254 cp1255 "
      "cp1256 cp1257 cp1258 cp874 "
      "iscii(assamese|bengali|gujarati|hindi|kannada|malayalam|oriya|punjabi|"
      "tamil|telugu) eucjp eucjisx0213 sjis sjisx0213 utf8 utf16 utf16le "
      "utf32 utf32le junet8 junet7 iso2022jp2 iso2022jp3 euckr uhc iso2022kr johab "
      "euccn gbk gb18030 iso2022cn hz big5 big5hkscs euctw\n");
}

/* --- global functions --- */

int main(int argc, char **argv) {
  extern char *optarg;
  extern int optind;

  int c;
  char *in;
  char *out;
  int count;
  FILE* fp;
  u_char output[1024];
  u_char input[1024];
  u_char *input_p;
  ef_parser_t *parser;
  ef_conv_t *conv;
  size_t size;

  if (argc != 5 && argc != 6) {
    usage();

    return 1;
  }

  in = NULL;
  out = NULL;
  while ((c = getopt(argc, argv, "i:o:")) != -1) {
    switch (c) {
      case 'i':
        in = optarg;
        break;

      case 'o':
        out = optarg;
        break;

      default:
        usage();

        return 1;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0) {
    fp = stdin;
  } else if (argc == 1) {
    if ((fp = fopen(*argv, "r")) == NULL) {
      bl_error_printf("%s not found.\n", *argv);
      usage();

      return 1;
    }
  } else {
    bl_error_printf("too many arguments.\n");
    usage();

    return 1;
  }

  parser = NULL;

  for (count = 0; count < sizeof(factories) / sizeof(factories[0]); count++) {
    if (strcmp(factories[count].encoding, in) == 0) {
      parser = (*factories[count].parser_new)();
    }
  }

  if (parser == NULL) {
    bl_error_printf("input encoding %s is illegal.\n", in);
    usage();

    return 1;
  }

  conv = NULL;

  for (count = 0; count < sizeof(factories) / sizeof(factories[0]); count++) {
    if (strcmp(factories[count].encoding, out) == 0) {
      conv = (*factories[count].conv_new)();
    }
  }

  if (conv == NULL) {
    bl_error_printf("output encoding %s is illegal.\n", out);
    usage();

    return 1;
  }

  input_p = input;
  while ((size = fread(input_p, 1, 1024 - parser->left, fp)) > 0) {
    (*parser->set_str)(parser, input, size + parser->left);
    if ((size = (*conv->convert)(conv, output, 1024, parser)) == 0) {
      break;
    }

    fwrite(output, 1, size, stdout);

    if (parser->left > 0) {
      memcpy(input, parser->str, parser->left);
      input_p = input + parser->left;
    } else {
      input_p = input;
    }
  }

  if (parser->left > 0) {
    (*parser->set_str)(parser, input, parser->left);
    size = (*conv->convert)(conv, output, 1024, parser);

    fwrite(output, 1, size, stdout);
  }

  (*parser->destroy)(parser);
  (*conv->destroy)(conv);

  return 0;
}
