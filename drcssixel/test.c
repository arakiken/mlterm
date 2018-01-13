/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) Araki Ken(arakiken@users.sourceforge.net)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
*/

#include "drcssixel.c"

static void pua_to_utf8(unsigned char *dst, unsigned int *src, unsigned int len) {
  unsigned int i;

  for(i = 0; i < len; i++) {
    unsigned int code = src[i];
    *(dst++) = ((code >> 18) & 0x07) | 0xf0;
    *(dst++) = ((code >> 12) & 0x3f) | 0x80;
    *(dst++) = ((code >> 6) & 0x3f) | 0x80;
    *(dst++) = (code & 0x3f) | 0x80;
  }
}

int main(int argc, char **argv) {
  char *sixel = "\x1bPq\"1;1;32;32#0;2;0;0;0#1;2;97;97;97#2;2;75;75;72#3;2;91;91;91#4;2;88;88;88#5;2;85;85;85#6;2;85;85;82#7;2;82;82;82#8;2;78;78;78#9;2;75;75;75#10;2;44;44;44#11;2;16;16;16#12;2;6;6;6#13;2;9;9;9#14;2;19;19;19#15;2;22;22;22#16;2;25;25;25#17;2;28;28;28#18;2;72;72;72#19;2;38;38;38#20;2;35;35;35#21;2;69;69;69#22;2;3;3;3#23;2;63;63;50#24;2;69;69;38#25;2;22;44;75#26;2;13;31;44#27;2;69;66;56#28;2;28;31;19#29;2;91;75;41#30;2;91;78;47#31;2;91;75;50#32;2;22;47;85#33;2;16;38;66#34;2;3;6;16#35;2;28;53;78#36;2;56;63;44#37;2;13;35;69#38;2;13;35;66#39;2;16;19;25#40;2;25;41;53#41;2;91;78;56#42;2;94;85;72#43;2;97;91;85#44;2;50;63;66#45;2;9;25;50#46;2;16;19;28#47;2;97;91;88#48;2;66;63;31#49;2;50;56;41#50;2;19;38;56#51;2;22;41;72#52;2;13;28;66#53;2;9;25;47#54;2;6;6;13#55;2;28;53;82#56;2;72;72;69#57;2;97;94;91#58;2;25;44;72#59;2;35;63;88#60;2;9;22;47#61;2;0;3;6#62;2;28;50;85#63;2;28;50;78#64;2;9;9;6#65;2;31;56;85#66;2;13;13;13#67;2;47;72;88#68;2;35;56;85#69;2;31;53;82#70;2;85;85;78#71;2;50;53;60#72;2;41;35;25#73;2;35;31;25#74;2;22;44;66#75;2;75;60;22#0~@@@pPPP!8@!9`!4@~~~$#1?}!23AaAA#10{$#3??{{!4KC#12OOO???_#7GKKKC#16?OO#9GC#18_o$#10!5?__#19_#11O???__!4O#14OO#8GKKKC#2G[KA$#4!8?GKKKC#6GKKC#17!7?OO$#20!8?_#17_#16_#15_!8?OO$#13!12?OO_$#5!12?GC-#0~???@!9?_w~^NN^^^N^_???~~~$#1?~!4?DA?CC#22!6?_?O#23_#24_#25_#26_#1?N#18FB#10~$#3??FB#13G!7?_OF!9?_#8O#21W[$#4??w{#12E!8?_WF!7?O#27???_$#11!4?o!6?_WN!4?_$#15!5?__?_?OD!6?O!7?_$#20!5?CIKIB!9?_$#16!5?O?_?OGA$#17!5?GOOOGB$#19!5?A??D#14__WF$#10!5?@?@-#0~!4?ooo}}}mY@A@#30AAABGO#24WQC#37A?_#20@?CO$#1?~#5A@#14@?@@@!4?A@#41C???So#21C#32@@#25a_#38EO#53G#46O#12_#14_$#6??{]#15A#12GGG!4?D#22C#6_#43GGKO#27?A@#36A#49_W#19@#34@#0@AFZN$#4??@#7_#16{#13CCC???P#34_#46G#18G#47O??g#31?Dg#44C#48G#33@#40C#51w#39AC#54G$#11!5?BAA?@@#55??O#40C#1__o#35???A#50???G#45?CO#60_$#59!13?_#56O#10A#42C?Cg#58!5?O#52?G_$#28!16?@#29@@???_C$#57!16?O-#0~!4?!6NK#65C_???_???O?E#51__@F#60oN#14C#0]$#1?~??\?!6O!6?@#44g??G#10_#40_#32PG#37o#52oN#46oG#22@$#8??ow_#9_#2___#18__#22Q#69GW?G!7?G#25?OM#38G#39??O#61_$#7??NF#16F#54!6?_#58Q!8?_#49@#63??@#54!4?_$#17!4?W#61!6?@#26`#59FW__!5?G#64!7?@$#41!14?@??CA?@#23E#66!8?A$#67!14?C!5?G#29@$#68!14?_O???K#62O?CPME$#7!14?A#57BB#30WOP#71_$#4!15?C#55O!5?Q$#43!16?CA#27C?C$#70!16?G#42?@#31AA$#24!19?_-#0~!11wy_O_#50Oo_#25C#53_!6?@#34G#12C#61@$#2?C??AAB@#19???@#34@C??_#51G#32?A#48INBA#28G#66_O??A$#1?B#10!10C#14C#20O_!5?O#72O?KFC#39C?C@$#8??B@#28@#49@#18?ABBB#27A#33?A!4?OO#44@#15_#16__oO#54_O#22?G$#9???A#63!9?@??K#55C#37?G#75C#73?OO?I#14G#0_oo}~$#22!13?G#26G!4?_#49??K#36@#38?@#45@#46EB$#65!14?@BA?C#62@#60!6?A#11G$#74!14?CG#59@BB$#69!14?A#58C??G$#46!15?O-#0!15BAB?A?AAA?AA!6B$#10!15?@#22?@!7?@$#19!17?A#61@A???A#12@$#39!19?@#46@#15@@#66@\x1b\\";
  unsigned int *buf;
  size_t sixel_len = strlen(sixel);
  char charset = '0';
  int is_96cs = 0;
  int num_cols;
  int num_rows;
  int col;
  int row;

  if (argc != 2 ||
      !(buf = drcs_sixel_from_file(argv[1], &charset, &is_96cs, &num_cols, &num_rows))) {
    if (!(buf = drcs_sixel_from_data(sixel, sixel_len, &charset, &is_96cs, &num_cols, &num_rows))) {
      return 1;
    }
  }

  pua_to_utf8(buf, buf, num_cols * num_rows);

  printf("cs: 0(94cs)-%c(%s)\n", charset, is_96cs ? "96cs" : "94cs");

  for(row = 0; row < num_rows; row++) {
    for (col = 0; col < num_cols; col++) {
      write(STDOUT_FILENO, buf++, 4);
    }
    write(STDOUT_FILENO, "\n", 1);
  }

  /* XXX buf is not freed */

  return 0;
}
