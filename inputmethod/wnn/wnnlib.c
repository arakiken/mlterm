/* -*- c-basic-offset:2; tab-width:8; indent-tabs-mode:nil -*- */
/*
  Copyright (c) 2008-2013 uim Project http://code.google.com/p/uim/

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
  may be used to endorse or promote products derived from this software
  without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/
/*
 *	wnnlib -- $B$+$J4A;zJQ49MQ%i%$%V%i%j(B (jllib $BBP1~HG(B)
 *
 *	$B$3$N%i%$%V%i%j$O!"(Bkinput V2 $B$KIUB0$7$F$$$?!"(BSRA $B$N@PA>:,$5$s$N(B
 *	jclib 5.2 $B$r%Y!<%9$K:n@.$7$^$7$?!#(B
 *
 *                                                        $B?9It(B $B1QG7(B
 */

/*
 * Copyright (c) 1989  Software Research Associates, Inc.
 * Copyright (c) 1998  MORIBE, Hideyuki
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Software Research Associates not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Software Research
 * Associates makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * Author:  Makoto Ishisone, Software Research Associates, Inc., Japan
 *		ishisone@sra.co.jp
 *          MORIBE, Hideyuki
 */

/*
 * Portability issue:
 *
 *	+ define SYSV, SVR4 or USG if you don't have bcopy() or bzero().
 *
 *	  if you define USG (which should be defined if your OS is based
 *	  on System V Rel 2) or SYSV (in case of System V Rel 3),
 *	  memchr() is used for bzero(), and my own version of bcopy()
 *	  is used in order to handle overlapping regions.
 *
 *	  if you define SVR4 (yes, System V Rel4), memmove() is used for
 *	  bcopy(), and memchr() is used for bzero().
 *
 *	+ wnnlib assumes bcopy() can handle overlapping data blocks.
 *	  If your bcopy() can't, you should define OVERLAP_BCOPY,
 *	  which force to use my own bcopy() rather than the one
 *	  in libc.a.
 */

/*
 * $B35MW(B
 *
 * wnnlib $B$O(B Wnn6 $B$K$bBP1~$7$?(B kinput $B$N(B CcWnn $B%*%V%8%'%/%H8~$1$N9b%l%Y%k(B
 * $B$+$J4A;zJQ49%i%$%V%i%j$G$"$k!#(B
 *
 * $B=>Mh$N(B Kinput $B$K$*$$$F$O!"(BWnn $B$H$N%$%s%?%U%'!<%9$O!"(Bjslib $B%Y!<%9$N(B
 * jilib $B$H(B jclib $B$G$"$C$?!#$H$3$m$,!"(BWnn6 $B$G3HD%$5$l$?5!G=$r;HMQ$7$?$/$F(B
 * $B$b!"(Bjslib $B%l%Y%k$N;EMM$,$[$H$s$IH=$i$J$+$C$?!#$3$N$?$a!"(Bmule $B$N(B egg $B%$(B
 * $B%s%?%U%'!<%9$G;HMQ$7$F$$$k(B jllib $B$rMQ$$$F!"=>Mh$N(B jilib $B$H(B jclib $B$N%$(B
 * $B%s%?%U%'!<%9$r$G$-$k$@$1JQ99$7$J$$$h$&$K$7$F!"?7$?$K(B wnnlib $B$H$7$F=q$-(B
 * $B49$($?!#(B
 *
 * wnnlib $B$O!"(BWnn6 $B$@$1$G$J$/!"$=$l0JA0$N(B Wnn4 $B$K$bBP1~$7$F$$$k$O$:$G(B
 * $B$"$k$,%F%9%H$O$7$F$$$J$$!#(B
 *
 * wnnlib $B$O!"=>Mh$N(B jclib $B$HF1MM$K!"$+$J%P%C%U%!$HI=<(%P%C%U%!$H$$$&#2$D(B
 * $B$N%P%C%U%!$r;}$D!#$+$J%P%C%U%!$K$OFI$_J8;zNs$,F~$j!"I=<(%P%C%U%!$K$OJQ(B
 * $B497k2L(B($BI=<(J8;zNs(B)$B$,F~$k!#$+$J%P%C%U%!$H8@$&8F$SJ}$O$"$^$j@53N$G$O$J$$!#(B
 * Wnn Version 4 $B0J9_$G$O4A;z$+$JJQ49$b$G$-$k$+$i$G$"$k!#(B
 *
 * $B%I%C%H$H%+%l%s%HJ8@a$H$$$&35G0$r;}$A!"J8;z$NA^F~(B / $B:o=|$O%I%C%H$N0LCV$K(B
 * $BBP$7$F9T$J$o$l!"JQ49$=$NB>$NA`:n$O%+%l%s%HJ8@a$KBP$7$F9T$J$o$l$k!#(B
 * Wnn Version 4 $B0J9_$G$OBgJ8@a$H>.J8@a$H$$$&#2<oN`$NJ8@a$N35G0$,$"$k$,!"(B
 * $B$=$l$KBP1~$7$F(B wnnlib $B$bEvA3$3$N#2<oN`$r07$&$3$H$,$G$-$k!#(B
 *
 * $B$3$N%i%$%V%i%j$O<!$N$h$&$J5!G=$rDs6!$9$k!#(B
 *	$B!&$+$J%P%C%U%!$X$NJ8;z$NA^F~(B / $B:o=|(B
 *	$B!&$+$J4A;zJQ49(B / $B:FJQ49(B / $BL5JQ49(B
 *	$B!&$R$i$,$J"N%+%?%+%JJQ49(B
 *	$B!&3NDj(B
 *	$B!&J8@a$N3HBg(B / $B=L>.(B
 *	$B!&%+%l%s%HJ8@a(B / $B%I%C%H$N0\F0(B
 *	$B!&<!8uJd(B/$BA08uJdCV$-49$((B
 *	$B!&8uJd<h$j=P$7(B / $BA*Br(B
 *	$B!&%P%C%U%!$N%/%j%"(B
 *
 * $BJ8;z%3!<%I$H$7$F$O(B Wnn $B$HF1$8$/(B EUC $BFbIt%3!<%I(B (process code) $B$r;HMQ$9$k!#(B
 */

/*
 * Wnn Version 6 (FI Wnn) $BBP1~$K$"$?$C$F(B
 *
 * $B=>Mh$N(B Kinput2 $B$G$O!"(BWnn $B$H$N%$%s%?!<%U%'!<%9$O!"(Bjslib $B$r%Y!<%9$K(B
 * $B$7$?(Bjilib $B$H(B jclib $B$G!"$b$H$b$H(B Wnn Version 3 $B$N(B libjd $B$N>e$K:n$i(B
 * $B$l$?%i%$%V%i%j$G$"$k!#(B
 *
 * Wnn Version 6 $BBP1~$K$"$?$C$F!"(Bjslib $B%l%Y%k$NDI2C5!G=$d>\:Y%$%s%?%U%'!<(B
 * $B%9$,H=$i$J$+$C$?$?$a!"(Bjslib $B$NBe$o$j$K(B mule $B$N(B egg $B%$%s%?%U%'!<%9$G;H(B
 * $BMQ$5$l$F$$$k(B jllib $B$r%Y!<%9$K$7$F!"(Bjilib $B$H(B jclib $B$r?7$?$K(B wnnlib $B$H$7(B
 * $B$F=q$-49$($k$3$H$K$7$?!#=q$-49$($O!"0J2<$NJ}?K$G9T$C$?!#(B
 *
 * 1. $B%G!<%?9=B$!"%$%s%?%U%'!<%9(B ($B4X?tL>$dJQ?tL>$b(B) $B$r$J$k$Y$/=>Mh$N(B
 * jclib$B$HF1$8$K$9$k!#(B
 *
 * 2. $B$+$J%P%C%U%!$HI=<(%P%C%U%!$NFs$D$NJ8;z%P%C%U%!$r;}$A!"(B
 * $B$+$J%P%C%U%!$K$OFI$_!"I=<(%P%C%U%!$K$OJQ497k2L$,F~$k$H$+(B
 * $BMM!9$JA`:n$O%+%l%s%HJ8@a$H8F$P$l$kJ8@a$KBP$7$F9T$J$o$l$k$H$+$$$C$?(B
 * $B4pK\E*$J%3%s%;%W%H$OJQ$($J$$!#(B
 *
 * 3. $B=>Mh$N%i%$%V%i%j$r;H$C$?%"%W%j%1!<%7%g%s$,?7$7$$%i%$%V%i%j$K(B
 * $B0\9T$7$d$9$$$h$&$K!"4X?t%$%s%?!<%U%'%$%9$b$G$-$k$@$1;w$?$b$N$K$9$k!#(B
 *
 * 4. 1,2,3 $B$NJ}?K$r$G$-$k$@$1<i$j$D$D!"(BWnn6 $B$GF3F~$5$l$?<!$N$h$&$J(B
 * $B5!G=$r%5%]!<%H$9$k!#(B
 *	$B!&(BFI $BJQ49(B/$B3X=,(B
 *	$B!&L5JQ493X=,(B
 *
 * 5. 1 $B$+$i(B 4 $B$^$G$NJ}?K$K=>$$$D$D!"%/%$%C%/!&%O%C%/$9$k!#(B
 */

/*
 * $B%a%b(B ($BCm(B: $B:G=i$NItJ,$O!"@PA>:,$5$s$N(B jclib $B:n@.%a%b(B)
 *
 * ver 0.0	89/07/21
 *	$B$H$j$"$($::n$j$O$8$a$k(B
 * ver 0.1	89/08/02
 *	$BH>J,$/$i$$$+$1$?(B
 *	$B<!8uJd4XO"$,$^$@$G$-$F$$$J$$(B
 * ver 0.2	89/08/04
 *	jcInsertChar() / jcDeleteChar() $B$r:n@.(B
 * ver 0.3	89/08/07
 *	$B0l1~$G$-$?(B
 *	$B$^$@$$$/$D$+5?LdE@$,$"$k$1$l$I(B
 * ver 0.4	89/08/08
 *	$B:#;H$C$?$h%S%C%H$N07$$$r;D$7$F!"$[$\$G$-$?$N$G$O$J$$$+$H(B
 *	$B;W$o$l$k(B
 *	$B:Y$+$$%P%0$r$+$J$j=$@5(B
 * ver 0.5	89/08/09
 *	$BN)LZ$5$s(B@KABA $B$K<ALd$7$?=j!":#;H$C$?$h%S%C%H$rMn$9$N$b(B
 *	$B%/%i%$%"%s%HB&$N@UG$$G$"$k$3$H$,$o$+$k(B
 *	$B$3$l$X$NBP1~(B
 *	$B$D$$$G$K%G!<%?9=B$$N@bL@$rDI2C(B
 *	$B%U%!%$%k$N%5%$%:$,(B 80KB $B$r1[$($F$7$^$C$?(B
 *	$B%3%a%s%H$r$H$l$P$+$J$j>.$5$/$J$k$s$@$1$I(B
 * ver 0.6	89/08/22
 *	jcDeleteChar() $B$rA4LLE*$K=q$-D>$9(B
 *	$B$3$l$G0l1~@5$7$/F0:n$9$k$h$&$K$J$C$?(B
 *	jcInsertChar() $B$G:G8e$N(B clauseInfo $B$N@_Dj$,4V0c$C$F$$$?$N$G(B
 *	$B$=$l$r=$@5(B
 *	jcPrintDetail() $B$K4JC1$J(B clauseInfo $B%G!<%?$N(B consistency check $B$r(B
 *	$BF~$l$k(B
 * ver 0.7	89/08/26
 *	jcExpand() $B$N%P%0=$@5(B
 *	$B>.J8@a$NC1J8@aJQ49$r>/$7=$@5(B
 * ver 0.8	89/08/30
 *	changecinfo() $B$G(B conv $B%U%i%0$r%;%C%H$9$k$N$rK:$l$F$$$?(B
 *	moveKBuf()/moveDBuf()/moveCInfo() $B$r>/$7=$@5(B
 *	SYSV $B$,(B define $B$5$l$F$$$l$P(B bcopy()/bzero() $B$NBe$o$j$K(B
 *	memcpy()/memset() $B$r;H$&$h$&$K=$@5(B
 * ver 0.9	89/09/22
 *	setLCandData() $B$G<!8uJd%P%C%U%!$N8uJd?t$K%+%l%s%HBgJ8@a$N(B
 *	$BJ,$r2C$($k$N$rK:$l$F$$$?(B
 * ver 0.10	89/10/16
 *	wnn-4.0.1 $B$G(B commonheader.h -> commonhd.h $B$K$J$C$?$N$G(B
 *	$B$=$l$N=$@5(B
 * ver 0.11	89/10/18
 *	USG $B$,(B define $B$5$l$F$$$F$b(B memcpy()/memset() $B$r;H$&$h$&$K=$@5(B
 * ver 0.12	89/10/19
 *	resizeBuffer() $B$G%I%C%H$N:F@_Dj$rK:$l$F$$$k$H$$$&=EBg$J%P%0$r=$@5(B
 * ver 4.0	89/10/27
 *	$B%P!<%8%g%sHV9f$r=$@5$7$F(B 4.0 $B$K$9$k!#(B
 * --- kinput $B$r(B R4 $B$K(B contribute ---
 * ver 4.1	90/06/04
 *	$B%/%i%$%"%s%HB&$K$"$k<-=q!&IQEY%U%!%$%k$N%;!<%V$,$G$-$J$$$H$$$&(B
 *	$B=EBg$J%P%0$r=$@5(B
 * ver 4.2	90/06/15
 *	$B<-=q$,EPO?2DG=$+$I$&$+$NH=Dj$,4V0c$C$F$$$F!"5UJQ492DG=<-=q$N(B
 *	$B%;!<%V$,$G$-$J$$$H$$$&$^$?$^$?=EBg$J%P%0$r=$@5(B
 *	$B:#$N$H$3$m(B kinput/wterm $B$H$bC18lEPO?5!G=$,$D$$$F$J$$$N$G(B
 *	$B<B32$O$J$+$C$?$,(B
 * ver 4.3	91/08/15
 *	$BJ8;z%G!<%?7?$H$7$F(B wchar_t $B$G$O$J$/!"(Bwchar $B$r;H$&$h$&$K$9$k(B
 *	$B:G=*E*$K$O(B Wnn $B$N<!4|%P!<%8%g%s$N7?$K9g$o$;$k$D$b$j(B
 * ver 4.4	91/09/18
 *	SYSV $B$^$?$O(B USG $B$,Dj5A$5$l$F$$$k>l9g$K$O<+F0E*$K(B OVERLAP_BCOPY
 *	$B$bDj5A$9$k$h$&$K$7$?(B
 *	SVR4 $B$,Dj5A$5$l$F$$$k>l9g$K$O(B bcopy $B$NBe$o$j$K(B memmove() $B$r;HMQ(B
 *	$B$9$k$h$&$K$7$?(B
 * ver 4.5	91/09/23
 *	DEBUG $B$r(B DEBUG_JCLIB $B$KJQ99(B
 * ver 5.0	91/10/01
 *	kinput2 $B%j%j!<%98~$1$K%P!<%8%g%sHV9f$r=$@5$7$F(B 5.0 $B$K$9$k!#(B
 * --- kinput2 $B$r(B R5 $B$K(B contribute ---
 * ver 5.1	92/02/07
 *	John Yates $B$5$s(B (yates@bldrsoft.com) $B$+$i(B getLCandDataLen() $B$G(B
 *	$BJ8;z?t$r?t$(4V0c$($F$$$?$N$r;XE&$5$l$?$N$G$=$l$N=$@5(B
 * ver 5.2	92/12/24
 *	jcInsertChar() $B$G%G!<%?$N=i4|2=$r$7$F$$$J$+$C$?ItJ,$,$"$C$?(B
 *	$B$N$G=$@5(B ($BCM$,BeF~$5$l$k$^$G;HMQ$5$l$J$$$N$G%P%0$G$O$J$$$N$@$,(B
 *	$B$A$g$C$H5$;}$A$o$k$$$N$G(B)
 *
 * ---  wnnlib $B:n@.%a%b(B ---
 *
 * ver 0.1	98/03/12
 *	$B$H$j$"$($:!"(Bjllib $B%$%s%?%U%'!<%9$K=q49$($r;O$a$k!#(B
 * ver 0.2	98/03/16
 *	$B$^$@$$$/$D$+7|G0;v9`$O$"$k$b$N$N!"4pK\E*$J=q49$($,=*$o$C$?$N$G!"(B
 *	$B%G%P%C%0$r;O$a$k!#$=$l$J$j$K!"F0$$$F$$$kMM;R!#(B
 * ver 0.3	98/03/18
 *	$B$$$/$+%P%0$,8+$D$+$C$?(B ($B%3%"!&%@%s%W$7$?(B) $B$N$G!"$=$l$i$r=$@5!#(B
 *	$B$^$@!"(BWnn6 $B$N5!G=$,M-8z$K$J$C$F$$$k$+NI$/$o$+$i$J$$!#(B
 * ver 0.4	98/07/01
 *	$B0JA0$+$i5$$K$J$C$F$$$?%k!<%W$K4Y$k8=>]$N860x$,$d$C$HH=$C$?!#(B
 *	$B860x$O!"JQ49$N(B cancel $B$N1dD9$G8F$P$l$k(B expandOrShrink $B$NCf$G!"(B
 *	$BL5JQ49;XDj$N;~$G$b(B ltop ($BBgJ8@a(B) $B%U%i%C%0$r%j%;%C%H$7$F$$$J$+$C(B
 *	$B$?$?$a$G!"$=$l$r=$@5$7$?!#(B
 * ver 0.5	98/10/15
 *	$B:G8e$N=$@5$+$iLs(B 3 $B%v7n4V!";HMQ$7$?$,FC$KLdBj$,$J$+$C$?$N$G!"(B
 *	kinput2-fix5 $B$N(B alpha $BHG$,$G$?$N$r5!2q$K!"(Bkinput2 $B%a!<%j%s%0!&(B
 *      $B%j%9%H$XEj9F!#(B
 * ver 0.6	98/12/03
 *	$B@PA>:,$5$s$h$j!"J8@a3HBg$N$G$N%P%0$NJs9p$,$"$C$?$N$G(B (kinput2
 *      $B%a!<%j%s%0!&%j%9%H(B 2106 $B!A(B 2118 $B;2>H(B)$B!"$=$l$r=$@5!#(B
 *
 * --- kinput2-fix-alpha2 $B$K<h$j9~$^$l$k(B ---
 *
 * ver 0.7	98/12/23
 *	doKantanSCConvert() $B$G>.J8@a$H$7$FC1J8@aJQ49$7$J$1$l$P$$$1$J$$(B
 *	$B$H$3$m$r!"BgJ8@a$H$7$FJQ49$7$F$$$?%P%0$r=$@5!#(B
 *
 * ver 0.8	99/01/06
 *	kinput2-fix5-alpha4 $B$,$G$?$N$r5!2q$K!"%I%C%H0J9_$r:o=|$9$kJT=8(B
 *	$B5!G=(B (kill-line) $B$r<B8=$9$k(B jcKillLine() $B$rDI2C$9$k(B ($B<B$O!"(Bwnnlib
 *	$B:n@.;~$+$i<B8=$7$h$&$H;W$C$F$$$F!"%@%_!<4X?t$@$1$O(B wnnlib $B$KB8(B
 *	$B:_$7$F$$$?(B)$B!#$3$l$N%G%P%C%0Cf$K!">e$N(B ver 0.4 $B$G=$@5$7$?$O$:$N(B
 *	$B%P%0$,:F8=!#(B
 *
 * ver 0.9	99/01/18
 *	$B$d$O$j!"(Bcancel $B$N1dD9$N=hM}$,$&$^$/$J$$$3$H$,H=L@!#$D$^$j!"J8(B
 *	$B@a3HBg$K$h$k(B cancel $B=hM}$G$O!"J8@a>pJs$,(B CcWnn $B$,4|BT$9$k$b$N(B
 *	$B$H0[$C$F$$$k$?$a(B ($B$3$l$,!"(Bjclib $B$H(B wnnlib $B$N0c$$(B)$B!"8mF0:n$r$7(B
 *	$B$?!#$3$N$?$a!"FHN)$7$?(B cancel $B=hM}$r(B jcCancel() $B%U%!%s%/%7%g%s(B
 *	$B$H$7$F<B8=$9$k$3$H$K$7$?!#$G$b!"(Bexpand-noconv $B$d(B shrink-noconv
 *	$B$J$I$G$OF1MM$NLdBj$,B8:_$9$k$N$G!"(Bjclib $B$H$N8_49$rJ]$D0UL#$G!"(B
 *	expandOrShrink $B$NCf$GFCJL07$$$9$k$3$H$K$7$?(B ($B;H$&?M$O!"$$$J$$(B
 *	$B$H;W$&$,!#(Bdoc/ccdef $B;2>H(B)$B!#(B
 *	$B$^$?!"(BgetHint() $B$H(B forceStudy() $B$N=hM}$r<c43$N8+D>$7$?!#(B
 *
 * ver 0.99	99/03/05
 *	$BA02s$N(B getHint() $B$N=hM}$NI{:nMQ$G!"(BsetCandiate() $B$G<!8uJd<h$j=P$7(B
 *	$B8e$NBgJ8@a>pJs$NJQ99J}K!$K$"$C$?@x:_%P%0$r=$@5!#(B
 *
 * ver ?.??	99/03/29				ishisone
 *	$BA0$K<h$j=P$7$?8uJd0lMw$r:FMxMQ$9$k$+$I$&$+$NH=Dj$rJQ99!#(B
 *	$B:FMxMQ$N>r7o$r$-$D$/$9$k!#$^$?(B Wnn4 $B$N(B jl $B%i%$%V%i%j$NIT6q9g(B
 *	($B;EMM$+$b(B) $B$N2sHr:v$NAH$_9~$_!#(B
 *
 * ver ?.??	99/04/12				ishisone
 *	jcOpen() $B$K2C$($F(B jcOpen2() $B$r<BAu!#$3$l$O(B Wnn4 $B$H(B Wnn6 $B$=$l$>$l$N(B
 *	$B=i4|2=%U%!%$%k$r;XDj$9$k$3$H$,$G$-!"<B:]$K@\B3$7$?%5!<%P$N(B
 *	$B%P!<%8%g%s$r%A%'%C%/$7$F!"$I$A$i$r;HMQ$9$k$+7h$a$k$H$$$&$b$N!#(B
 *
 * ver ?.??	99/05/07				ishisone
 *	$B!VL5NLBg?t!WLdBj$N2sHr:v$N<BAu!#$H$O$$$C$F$bJ8@a?-=L$N:]$N(B
 *	$BA0J8@a$H$N@\B3$r$d$a$k$@$1!#!VL5NLBg?t!WLdBj$K$D$$$F$O(B
 *	expandOrShrink() $B$N%3%a%s%H;2>H!#(B
 *
 * ver ?.??	99/05/25				ishisone
 *	config.h $B$r%$%s%/%k!<%I$7$J$$$h$&$K$9$k!#I,MW$J$N$O(B LIBDIR $B$@$1(B
 *	$B$@$7!"(Bconfig.h $B$N(B LIBDIR $B$NCM$,@5$7$$$H$$$&J]>Z$b$J$$$?$a!#(B
 *	/usr/local/lib/wnn $B$K7h$a$&$A!#(B($B%*!<%P!<%i%$%I$9$k$3$H$O$G$-$k(B)
 *
 * --- kinput2 version 3.0 $B%j%j!<%9(B ---
 *
 * ver ?.??	01/01/10
 *	Wnn7 $BBP1~!#$H$O$$$C$F$b:G>.8B$NBP1~$G!"(BWnn7 $B$N?7$7$$5!G=$r(B
 *	$BMxMQ$G$-$k$o$1$G$O$J$$!#(B
 *	$B;HMQ$5$l$F$$$J$$JQ?t$r:o=|!#(B
 */


/*
 * $B%U%!%s%/%7%g%s(B
 *
 * struct wnn_buf jcOpen(char *servername, char *envname,
 *			 int override, char *rcfilename,
 *			 void (*errmsgfunc)(), int (*confirmfunc)(),
 *			 int timeout)
 *	jl_open $B$"$k$$$O(B jl_open_lang $B$KBP1~$7$?(B wnnlib $B$N%$%s%?%U%'!<(B
 *	$B%9$G!"$3$N4X?t$NCf$G<B:]$K(B jl_open $B$"$k$$$O(B jl_open_lang $B$r8F(B
 * 	$B$S=P$9!#(Boverride $B$,(B True $B$N>l9g!"4{$K4D6-$,%5!<%PB&$K$"$C$F$b!"(B
 *	$B4D6-$r:F=i4|2=$9$k!#(B
 *
 * void jcClose(struct wnn_buf *wnnbuf)
 *	jl_close $B$r8F$S=P$7!"(BjcOpen $B$G3MF@$7$?(B wnnbuf $B$N2rJ|$H%5!<%P$H(B
 *	$B$N@\B3$r@Z$k!#(B
 *
 * int jcIsConnect(struct wnn_buf *wnnbuf)
 *	$B%5!<%P$H$N@\B3>uBV$r(B jl_isconnect $B$GD4$Y$k!#(Bwnnbuf $B$,(B NULL$B!"(B
 *	$B4D6-$,:n@.$5$l$F$$$J$$!"$"$k$$$O%5!<%P$H@\B3$5$l$F$$$J$$>l9g$K$O(B 0$B!#(B
 *	wnnbuf $B$,%5!<%P$H@\B3$5$l$F$$$l$P!"(B1 $B$rJV$9!#(B
 *
 * jcConvBuf *jcCreateBuffer(struct wnn_env *env, int nclause, int buffersize)
 *	$B;XDj$5$l$?4D6-$r;H$C$FJQ49$N%P%C%U%!$r:n@.$9$k!#%P%C%U%!$O(B
 *	$BJ#?t:n$k$3$H$,$G$-$k!#0l$D$N%P%C%U%!$G$OF1;~$KJ#?t$NJ8$r(B
 *	$BJQ49$9$k$3$H$O$G$-$J$$$N$G!"J#?t$NJ8$rJB9T$7$FJQ49$7$?$$>l9g$K$O(B
 *	$B4v$D$+$N%P%C%U%!$rMQ0U$7$J$/$F$O$J$i$J$$!#(B
 *	$B4D6-$N@_Dj$^$G$rM=$a$d$C$F$*$/I,MW$,$"$k!#$D$^$j%5!<%P$H$N@\B3!"(B
 *	$B4D6-$N@8@.!"<-=q$N@_Dj$J$I$O(B jcOpen $B$G9T$C$F$*$/I,MW$,$"$k!#(B
 *	$B0z?t$N(B nclause $B$H(B buffersize $B$G!"$=$l$>$l=i4|2=;~$K%"%m%1!<%H$9$k(B
 *	$BJ8@a>pJs$*$h$S$+$J(B/$BI=<(%P%C%U%!$NBg$-$5$,;XDj$G$-$k!#(B
 *	$B$?$@$7$3$l$i$O!"%5%$%:$,B-$j$J$/$J$l$PI,MW$K1~$8$F<+F0E*$K(B
 *	$BA}$d$5$l$k$?$a!"$3$3$K;XDj$7$?0J>e$N?t$NJ8@a$d!"J8;zNs$,JQ49$G$-$J$$(B
 *	$B$o$1$G$O$J$$!#$=$l$>$l(B 0 $B$^$?$OIi$NCM$r;XDj$9$k$H!"%G%U%)%k%H$N(B
 *	$B%5%$%:$G%"%m%1!<%H$5$l$k!#=>$C$FDL>o$O(B nclause/buffersize $B$H$b(B
 *	0 $B$r;XDj$7$F$*$1$P$h$$!#(B
 *	$B%j%?!<%s%P%j%e!<$H$7$F%P%C%U%!$rJV$9!#%(%i!<$N;~$K$O(B NULL $B$,(B
 *	$BJV$5$l$k!#(B
 *
 * int jcDestroyBuffer(jcConvBuf *buf, int savedic)
 *	$B%P%C%U%!$N;HMQ$r=*N;$9$k!#4D6-$r>C$7$?$j!"%5!<%P$H$N@\B3$r@Z$C$?$j(B
 *	$B$9$k$3$H$O!"(BjcClose $B$G9T$&!#(B
 *	$B0z?t(B savedic $B$,(B 0 $B$G$J$1$l$P!"4D6-Cf$G;HMQ$5$l$F$$$kA4$F$N<-=q$r(B
 *	$B%;!<%V$9$k!#(B
 *
 * int jcClear(jcConvBuf *buf)
 *	$B%P%C%U%!$r%/%j%"$9$k!#?7$?$KJQ49$r;O$a$k:]$K$O:G=i$K$3$N(B
 *	$B%U%!%s%/%7%g%s$r8F$P$J$1$l$P$J$i$J$$!#(B
 *
 * int jcInsertChar(jcConvBuf *buf, int c)
 *	$B%I%C%H$K#1J8;zA^F~$9$k!#(B
 *	$B%+%l%s%HJ8@a$,4{$KJQ49$5$l$F$$$l$PL5JQ49$N>uBV$KLa$k!#(B
 *	$B%+%l%s%HJ8@a$OBgJ8@a$G$"$k!#(B
 *
 * int jcDeleteChar(jcConvBuf *buf, int prev)
 *	$B%I%C%H$NA0Kt$O8e$m$N#1J8;z$r:o=|$9$k!#(B
 *	$B%+%l%s%HJ8@a$,4{$KJQ49$5$l$F$$$l$PL5JQ49$N>uBV$KLa$k!#(B
 *	$B%+%l%s%HJ8@a$OBgJ8@a$G$"$k!#(B
 *
 * int jcConvert(jcConvBuf *buf, int small, int tan, int jump)
 *	$B%+%l%s%HJ8@a$+$i8e$m$rJQ49$9$k!#(B
 *	$B0z?t(B tan $B$,(B 0 $B$J$iO"J8@aJQ49!"$=$&$G$J$1$l$P%+%l%s%HJ8@a$r(B
 *	$BC1J8@aJQ49$7!"$=$N$"$H$rO"J8@aJQ49$9$k!#(B
 *	$B0z?t(B small $B$,(B 0 $B$G$J$1$l$P>.J8@a$,!"$=$&$G$J$1$l$PBgJ8@a$,(B
 *	$B%+%l%s%HJ8@a$H$7$F;H$o$l$k!#(B
 *	$B0z?t(B jump $B$G!"JQ498e$N%+%l%s%HJ8@a$N0LCV$,7h$^$k!#(Bjump $B$,(B
 *	0 $B$J$i%+%l%s%HJ8@a$N0LCV$OJQ49$7$F$bJQ$o$i$J$$(B ($B$?$@$7(B
 *	$B%+%l%s%HJ8@a$H$7$FBgJ8@a$r;XDj$7$?>l9g!"JQ498e$N%+%l%s%H(B
 *	$B>.J8@a$O%+%l%s%HBgJ8@a$N:G=i$N>.J8@a$K$J$k(B) $B$,!"(B0 $B$G$J$1$l$P(B
 *	$B:G8e$NJ8@a$N<!(B ($B6uJ8@a(B) $B$K0\F0$9$k!#C`<!JQ49$7$F$$$/$h$&$J(B
 *	$B%"%W%j%1!<%7%g%s$G$O$3$l$r(B 1 $B$K$9$k$H$h$$$@$m$&!#(B
 *
 * int jcUnconvert(jcConvBuf *buf)
 *	$B%+%l%s%HBgJ8@a$rL5JQ49$N>uBV$KLa$9!#(B
 *	$B%+%l%s%HBgJ8@a$,$$$/$D$+$N>.J8@a$+$i$G$-$F$$$?>l9g!"$3$l$i$N(B
 *	$B>.J8@a$O$^$H$a$i$l!"0l$D$NL5JQ49>uBV$NJ8@a$K$J$k!#(B
 *	$B%+%l%s%H>.J8@a$rL5JQ49$KLa$95!G=$OMQ0U$7$J$$!#$J$<$+$H$$$&$H!"(B
 *	$BBgJ8@a$NCf$N(B 1 $B>.J8@a$N$_$,L5JQ49$K$J$C$F$7$^$&$H!"$=$NJ8@a$K(B
 *	$B4X$7$F(B jcMove() $B$G0\F0$r9T$J$C$?;~!"$I$&0\F0$9$l$P$h$$$N$+(B
 *	$B$h$/$o$+$i$J$$!"$D$^$j0\F0$N%;%^%s%F%#%/%9$,ITL@3N$K$J$C$F$7$^$&(B
 *	$B$+$i$G$"$k!#(B
 *
 * int jcKana(jcConvBuf *buf, int small, int kind)
 *	$B%+%l%s%HJ8@a$r$+$J$K$9$k!#(B
 *	$B0z?t(B kind $B$,!"(BJC_HIRAGANA $B$J$i$R$i$,$J!"(BJC_KATAKANA $B$J$i%+%?%+%J$K(B
 *	$BJQ$o$k!#J8@a$NJQ49>uBV$OJQ2=$7$J$$!#$D$^$jJQ49$5$l$F$$$l$P(B
 *	$BJQ49>uBV$N$^$^!"L$JQ49$N>uBV$J$iL$JQ49$N$^$^$G$"$k!#(B
 *	$B0z?t(B small $B$,(B 0 $B$G$J$1$l$P%+%l%s%H>.J8@a$,!"$=$&$G$J$1$l$P(B
 *	$B%+%l%s%HBgJ8@a$,JQ$o$k!#(B
 *	$B%+%l%s%HBgJ8@a$r$+$J$K$9$k>l9g!"$=$NCf$N>.J8@a$O0l$D$K$^$H$a$i$l$k!#(B
 *
 * int jcFix(jcConvBuf *buf)
 *	$B8=:_!"%P%C%U%!$K$O$$$C$F$$$kJQ49J8;zNs$r3NDj$5$;$k!#(B
 *
 * int jcFix1(jcConvBuf *buf)
 *	$B8=:_!"%P%C%U%!$K$O$$$C$F$$$kJQ49J8;zNs$N@hF,0lJ8;z$@$1$r3NDj$5$;$k!#(B
 *
 * int jcExpand(jcConvBuf *buf, int small, int convf)
 *	$B%+%l%s%HJ8@a$ND9$5$r#1J8;z?-$P$9!#0z?t(B convf $B$,(B 0 $B$G$J$1$l$P(B
 *	$B?-$P$7$?$"$H:FJQ49$9$k!#(B
 *	$B0z?t(B small $B$,(B 0 $B$G$J$1$l$P>.J8@a$,!"$=$&$G$J$1$l$PBgJ8@a$,(B
 *	$B%+%l%s%HJ8@a$H$7$F;H$o$l$k!#(B
 *
 * int jcShrink(jcConvBuf *buf, int small, int convf)
 *	$B%+%l%s%HJ8@a$ND9$5$r#1J8;z=L$a$k!#0z?t(B convf $B$,(B 0 $B$G$J$1$l$P(B
 *	$B=L$a$?$"$H:FJQ49$9$k!#(B
 *	$B0z?t(B small $B$,(B 0 $B$G$J$1$l$P>.J8@a$,!"$=$&$G$J$1$l$PBgJ8@a$,(B
 *	$B%+%l%s%HJ8@a$H$7$F;H$o$l$k!#(B
 *
 * int jcNext(jcConvBuf *buf, int small, int prev)
 *	$B%+%l%s%HJ8@a$r<!8uJdKt$OA08uJd$GCV$-49$($k!#(B
 *	$B0z?t(B small $B$,(B 0 $B$G$J$1$l$P>.J8@a$,!"$=$&$G$J$1$l$PBgJ8@a$,(B
 *	$B%+%l%s%HJ8@a$H$7$F;H$o$l$k!#(B
 *
 * int jcCandidateInfo(jcConvBuf *buf, int small, int *ncandp, int *curcandp)
 *	$B<!8uJd$N>pJs$rJV$9!#(B
 *	$B<!8uJd0lMw$r=P$9$?$a$K$O:G=i$K$3$N4X?t$r8F$V$H$h$$!#(B
 *
 * int jcGetCandidate(jcConvBuf *buf, int n, wchar *candstr, int len)
 *	$B;XDj$5$l$?8uJdHV9f$NJ8;zNs$rJV$9!#%+%l%s%H8uJdHV9f$O$3$NHV9f$K(B
 *	$BJQ$o$k!#I=<(%P%C%U%!$OJQ2=$7$J$$!#(B
 *	$BBg@N$N(B wnnlib $B$O<!8uJd$,MQ0U$5$l$F$$$J$1$l$PMQ0U$7$?$,!"$3$N%P!<%8%g%s(B
 *	$B$G$O%(%i!<$K$J$k!#(BjcNext $B$d(B jcCandidateInfo $B$r@h$K8F$s$G$*$+$J$1$l$P(B
 *	$B$J$i$J$$!#(B
 *
 * int jcSelect(jcConvBuf *buf, int n)
 *	$B;XDj$5$l$?HV9f$N8uJd$GI=<(%P%C%U%!$rCV$-49$($k!#(B
 *	$B%+%l%s%H8uJdHV9f$O$3$NHV9f$KJQ$o$k!#(B
 *
 * int jcDotOffset(jcConvBuf *buf)
 *	$BBgJ8@a$N@hF,$+$i$N%I%C%H$N%*%U%;%C%H$rJV$9!#(B
 *	$BNc$($P(B 0 $B$J$i%I%C%H$,%+%l%s%HJ8@a$N@hF,$K$"$k$3$H$K$J$k!#(B
 *
 * int jcIsConverted(jcConvBuf *buf, int cl)
 *	$B;XDj$5$l$?J8@a$,JQ49$5$l$F$$$k$+$I$&$+$rJV$9(B
 *	0 $B$J$iL5JQ49>uBV(B
 *	1 $B$J$iJQ49>uBV(B
 *	-1 $B$J$i(B $B%(%i!<(B
 *
 * int jcMove(jcConvBuf *buf, int small, int dir)
 *	$B%I%C%H!&%+%l%s%HJ8@a$r0\F0$9$k!#(B
 *	$B%+%l%s%HJ8@a$,JQ49:Q$_$G$"$l$PJ8@a0\F0$7!"$=$&$G$J$1$l$P(B
 *	$B%I%C%H$N$_$,0\F0$9$k!#(B
 *	$BJ8@a0\F0;~$K!"0z?t(B small $B$,(B 0 $B$G$J$1$l$P>.J8@aC10L$G0\F0$7!"(B
 *	$B$=$&$G$J$1$l$PBgJ8@aC10L$K0\F0$9$k!#(B
 *
 * int jcTop(jcConvBuf *buf)
 *	$B%I%C%H!&%+%l%s%HJ8@a$rJ8$N@hF,$K0\F0$9$k!#%+%l%s%H>.J8@a!&(B
 *	$B%+%l%s%HBgJ8@a$H$b$K0\F0$9$k!#(B
 *
 * int jcBottom(jcConvBuf *buf)
 *	$B%I%C%H!&%+%l%s%HJ8@a$rJ8$N:G8e$K0\F0$9$k!#%+%l%s%H>.J8@a!&(B
 *	$B%+%l%s%HBgJ8@a$H$b$K0\F0$9$k!#(B
 *	$B$b$7!":G8e$NJ8@a$,L5JQ49>uBV$G$"$l$P%+%l%s%HJ8@a$O$=$NJ8@a$K$J$j!"(B
 *	$B%I%C%H$O$=$NJ8@a$N:G8e$KMh$k!#$=$&$G$J$1$l$P%+%l%s%HJ8@a$O(B
 *	$B:G8e$NJ8@a$N<!(B ($B$D$^$j6u$NJ8@a(B) $B$KMh$k!#(B
 *
 * int jcChangeClause(jcConvBuf *buf, wchar *str)
 *	$B%+%l%s%HBgJ8@a$r;XDj$5$l$?J8;zNs$GF~$l49$($k!#(B
 *	$BI=<(%P%C%U%!$@$1$G$O$J$/!"$+$J%P%C%U%!$NFbMF$b(B
 *	$BCV$-49$o$k!#J8@a$OL5JQ49>uBV$K$J$k!#(B
 *
 * int jcSaveDic(jcConvBuf *buf)
 *	$B;HMQCf$N4D6-$G;H$o$l$F$$$kA4$F$N<-=qJB$S$KIQEY%U%!%$%k$r(B
 *	$B%;!<%V$9$k!#(B
 *	$B$3$N%U%!%s%/%7%g%s$O>o$K(B 0 $B$rJV$9!#K\Ev$K%;!<%V$5$l$?$+$N(B
 *	$B%A%'%C%/$O$7$J$$!#(B
 *
 * int jcCancel(jcConvBuf *buf)
 *      $B8=:_F~NOCf$N$9$Y$F$NJ8;zNs$r!"JQ49:Q$_$N$b$N$r4^$a$F!"$9$Y$FL$(B
 *      $BJQ49>uBV$K$9$k!#%*%j%8%J%k$N(B CcWnn $B$H(B jclib $B%$%s%?%U%'!<%9$G$O!"(B
 *	$B@hF,J8@a$rA4F~NOJ8;zNs$ND9$5$^$G3HD%$9$k$3$H$G!"$3$N=hM}$r9T$J$C(B
 *	$B$F$$$?$,!"$3$N=hM}$H(B jllib $B$H$N%$%s%?%U%'!<%9$,$&$^$/9g$o$:!"(B
 *	wnnlib $B$G$OFHN)$7$?%U%!%s%/%7%g%s$H$7$?!#(B
 *
 * int jcKillLine(jcConvBuf *buf)
 *      $B8=:_$N%I%C%H$"$k$$$O%+%l%s%HJ8@a0J9_$r:o=|$9$k!#%I%C%H$,$"$kJ8(B
 *      $B@a$,4{$KJQ49$5$l$F$$$l$P!"$=$NJ8@a!"$D$^$j%+%l%s%HJ8@a$r4^$a$F(B
 *      $B:o=|$9$k!#%I%C%H$"$k$$$O%+%l%s%HJ8@a$,@hF,$G$"$l$P!"(BjcClear()
 *      $B$HF1$8F0:n$r$9$k!#$D$^$j!"(BjcClear() $B<+BN$OITMW$K$J$k$N$@$,!"5l(B
 *      $B%$%s%?%U%'!<%9$r9MN8$7$F!"(BjcClear() $B$O$=$N$^$^;D$9!#(B
 *      $B$J$*!":o=|8e$N%I%C%H$H%+%l%s%HJ8@a$O!"A4JQ49BP>]J8;zNs$NKvHx!"(B
 *      $B$"$k$$$O:G=*J8@a$NKvHx$K$"$k6uJ8@a$K$J$k!#(B
 *
 * $B$3$l$i$N%U%!%s%/%7%g%s$OFC$K=q$+$l$F$$$J$1$l$P@.8y$N>l9g$K$O(B 0,
 * $B%(%i!<$N>l9g$K$O(B -1 $B$rJV$9!#(B
 *
 */

/*
 * $B%0%m!<%P%kJQ?t(B
 *
 * wnnlib $B$G;H$o$l$k%0%m!<%P%kJQ?t$O(B jcErrno $B$?$@0l$D$G$"$k!#(B
 *
 * extern int jcErrno
 *	$B%(%i!<$N:]$K!"%(%i!<%3!<%I$,BeF~$5$l$k!#%(%i!<%3!<%I$O(B wnnlib.h $B$G(B
 *	$BDj5A$5$l$F$$$k!#(B
 */

/*
 * $B%G!<%?9=B$(B
 *
 * wnnlib $B$N;}$D%G!<%?$G!"%"%W%j%1!<%7%g%s$+$iD>@\%"%/%;%9$7$F$h$$$N$O(B
 * $BJQ49%P%C%U%!(B jcConvBuf $B7?$N(B public member $B$H=q$+$l$?ItJ,$N$_$G$"$k!#(B
 * $BD>@\%"%/%;%9$7$F$h$$$H$$$C$F$b!"CM$r;2>H$9$k$@$1$G!"CM$rJQ99$9$k$3$H$O(B
 * $B5v$5$l$J$$!#%"%W%j%1!<%7%g%s$,>!<j$KCM$rJQ99$7$?>l9g$N(B wnnlib $B$NF0:n$O(B
 * $BJ]>Z$5$l$J$$!#(B
 *
 * <$BJQ49%P%C%U%!(B>
 *
 * jcConvBuf $B7?$O(B wnnlib.h $B$G<!$N$h$&$KDj5A$5$l$F$$$k!#(B
 *
 * typedef struct {
 *    /-* public member *-/
 *	int		nClause;	$BJ8@a?t(B
 *	int		curClause;	$B%+%l%s%HJ8@aHV9f(B
 *	int		curLCStart;	$B%+%l%s%HBgJ8@a3+;OJ8@aHV9f(B
 *	int		curLCEnd;	$B%+%l%s%HBgJ8@a=*N;J8@aHV9f(B
 *	wchar		*kanaBuf;	$B$+$J%P%C%U%!$N@hF,(B
 *	wchar		*kanaEnd;	$B$+$J%P%C%U%!$NKvHx(B
 *	wchar		*displayBuf;	$BI=<(%P%C%U%!$N@hF,(B
 *	wchar		*displayEnd;	$BI=<(%P%C%U%!$NKvHx(B
 *	jcClause	*clauseInfo;	$BJ8@a>pJs(B
 *	struct wnn_env	*wnn;
 *    /-* private member *-/
 *	[ $B>JN,(B ]
 * } jcConvBuf;
 *
 * nClause $B$O8=:_$NJ8@a?t$rI=$9!#$3$l$O>.J8@a$N?t$G$"$k!#(B
 * curClause $B$O%+%l%s%H>.J8@a$NHV9f$G$"$k!#(B
 * curLCStart $B$H(B curLCEnd $B$O%+%l%s%HBgJ8@a$NHO0O$r<($9!#(BcurLCStart $B$+$i(B
 * curLCEnd-1 $B$NHO0O$NJ8@a$,%+%l%s%HBgJ8@a$G$"$k!#$D$^$j!"(BcurLCEnd $B$O(B
 * $B<!$NBgJ8@a$N@hF,$NHV9f$G$"$k!#(B
 *
 * kanaBuf $B$H(B displayBuf $B$,$+$J%P%C%U%!$HI=<(%P%C%U%!$G$"$k!#(B
 * jcInsertChar() $BEy$r;H$C$FF~NO$5$l$?FI$_$O$+$J%P%C%U%!$HI=<(%P%C%U%!$KF~$k!#(B
 * $B$3$l$rJQ49$9$k$H!"I=<(%P%C%U%!$NJ}$@$1$,4A;z$NJ8;zNs$K$J$k!#(B
 * kanaEnd $B$*$h$S(B displayEnd $B$O$=$l$>$l$N%P%C%U%!$KF~$l$i$l$?J8;zNs$N:G8e(B
 * $B$NJ8;z$N<!$r;X$7$F$$$k!#$+$J%P%C%U%!!&I=<(%P%C%U%!$O$I$A$i$b(B NULL $B%?!<%_(B
 * $B%M!<%H$5$l$J$$!#(B
 *
 * clauseInfo $B$OJ8@a>pJs$NF~$C$?G[Ns$G$"$k!#$3$l$O$"$H$G@bL@$9$k!#(B
 *
 * env $B$O$3$NJQ49%P%C%U%!$N;HMQ$9$k4D6-$G$"$k!#(B
 *
 * <$BJ8@a>pJs(B>
 *
 * $B3FJ8@a$N>pJs$O(B clauseInfo $B$H$$$&L>A0$N(B jcClause $B7?$NG[Ns$KF~$C$F$$$k!#(B
 * jcClause $B7?$O(B wnnlib.h $B$G<!$N$h$&$KDj5A$5$l$F$$$k!#(B
 *
 * typedef struct {
 *	wchar	*kanap;		$BFI$_J8;zNs(B ($B$+$J%P%C%U%!$NCf$r;X$9(B)
 *	wchar	*dispp;		$BI=<(J8;zNs(B ($BI=<(%P%C%U%!$NCf$r;X$9(B)
 *	char	conv;		$BJQ49:Q$_$+(B
 *				0: $BL$JQ49(B 1: $BJQ49:Q(B -1: wnnlib $B$G5?;wJQ49(B
 *	char	ltop;		$BBgJ8@a$N@hF,$+(B?
 * } jcClause;
 *
 * kanap $B$O!"$+$J%P%C%U%!>e$N!"$=$NJ8@a$NFI$_$N;O$^$j$N0LCV$r<($9%]%$%s%?(B
 * $B$G$"$k!#$^$?!"(Bdispp $B$O!"I=<(%P%C%U%!(B $B>e$G!"$=$NJ8@a$N;O$^$j$N0LCV$r<($9!#(B
 * $B=>$C$F!"(Bn $BHV$NJ8@a$O!"(B
 *	$B$h$_(B:	clauseInfo[n].kanap $B$+$i(B clauseInfo[n+1].kanap $B$NA0$^$G(B
 *	$B4A;z(B:	clauseInfo[n].dispp $B$+$i(B clauseInfo[n+1].dispp $B$NA0$^$G(B
 * $B$H$J$k!#$3$N$h$&$K(B n $BHVL\$NJ8@a$NHO0O$r<($9$N$K(B n+1 $BHVL\$N(B clauseInfo $B$,(B
 * $BI,MW$J$?$a!"(BclauseInfo $B$NG[Ns$NMWAG$O>o$K@hF,$+$iJ8@a?t(B+1$B8D$,M-8z$G$"$k!#(B
 * $B$J$*!"@hF,J8@a$O(B 0 $BHVL\$+$i;O$^$k$b$N$H$9$k!#(B
 *
 * conv $B$O$=$NJ8@a$NJQ49>uBV$rI=$9!#(B0 $B$J$iL$JQ49>uBV!"(B1 $B$J$iJQ49>uBV!"(B
 * -1 $B$J$i(B jcKana() $B$K$h$C$F5?;wJQ49$5$l$?$3$H$r<($9!#$3$l$O!"JQ49$N3X=,$H(B
 * $BIQEY>pJs$N99?7$N$?$a$K;HMQ$9$k!#(B
 *
 * ltop $B$,(B 0 $B$G$J$1$l$P$=$NJ8@a$,BgJ8@a$N@hF,$G$"$k$3$H$r<($9!#(Bimabit $B$O(B
 * $B$=$NJ8@a$N448l$N:#;H$C$?$h%S%C%H$,F~$C$F$$$k!#(B
 *
 * kanap, dispp $BEy$G!"(Bn $BHVL\$NJ8@a$NHO0O$r<($9$N$K(B n+1 $BHVL\$NJ8@a>pJs$,(B
 * $BI,MW$J$?$a!"(BclauseInfo $B$NG[Ns$NMWAG$O>o$K@hF,$+$iJ8@a?t(B+1$B8D$,M-8z$G$"$k!#(B
 * $BJ8@a?t(B+1 $B8DL\$NJ8@a>pJs(B (clauseInfo[nClause]) $B$O(B
 *	kanap, dispp: $B$=$l$>$l(B kanaEnd, displayEnd $B$KEy$7$$(B
 *	conv: 0 ($BL$JQ49>uBV(B)
 *	ltop: 1
 * $B$G$"$k!#(B
 *
 * $BJ8@a>pJs$N(B kanap, dispp $B$rNc$r;H$C$F<($7$F$*$/!#(B
 *
 * $BNcJ8(B: $B$3$l$O%G!<%?9=B$$r<($9$?$a$NNcJ8$G$9(B ($BJ8@a?t(B 6)
 *
 * kanap:   $B#0(B    $B#1(B    $B#2(B        $B#3(B    $B#4(B    $B#5(B          $B#6(B(=kanaEnd)
 *	    $B"-(B    $B"-(B    $B"-(B        $B"-(B    $B"-(B    $B"-(B          $B"-(B
 * kanaBuf: $B$3$l$O$G!<$?$3$&$>$&$r$7$a$9$?$a$N$l$$$V$s$G$9(B
 *
 * dispp:      $B#0(B    $B#1(B    $B#2(B    $B#3(B  $B#4(B    $B#5(B      $B#6(B(=displayEnd)
 *	       $B"-(B    $B"-(B    $B"-(B    $B"-(B  $B"-(B    $B"-(B      $B"-(B
 * displayBuf: $B$3$l$O%G!<%?9=B$$r<($9$?$a$NNcJ8$G$9(B
 */

#ifdef DEBUG_WNNLIB
#include	<stdio.h>
#endif
#include	"wnnlib.h"
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<sys/types.h>
#include	<pwd.h>
#include	<arpa/inet.h> /* htons */
#include	"gettext.h"

#ifndef WNNENVDIR
#define WNNENVDIR	WNNLIBDIR "/wnn"
#endif

/*
 * Wnn7 $B$G$OBgC@$K$b$$$/$D$+$N(B API $B4X?t$K%P%C%U%!%5%$%:$r;XDj$9$k(B
 * $B0z?t$rDI2C$7$F$$$k$?$a!"%P!<%8%g%s$rD4$Y!"$=$l$K$h$C$F0z?t$r(B
 * $BJQ99$7$J$1$l$P$J$i$J$$!#$H$j$"$($:K\%W%m%0%i%`$G$O(B Wnn7 $B$N0z?t$K9g$o$;$k!#(B
 */

/* Wnn7 $B$+$I$&$+$NH=Dj(B */
#ifdef WNN_RENSOU
#define WNN7
#endif

#ifdef WNN7
#define ki2_jl_get_yomi			jl_get_yomi
#define ki2_jl_get_kanji		jl_get_kanji
#define ki2_jl_get_zenkouho_kanji	jl_get_zenkouho_kanji
#define ki2_jl_fuzokugo_get		jl_fuzokugo_get
#else
#define ki2_jl_get_yomi(a, b, c, d, sz)		jl_get_yomi(a, b, c, d)
#define ki2_jl_get_kanji(a, b, c, d, sz)	jl_get_kanji(a, b, c, d)
#define ki2_jl_get_zenkouho_kanji(a, b, c, sz)	jl_get_zenkouho_kanji(a, b, c)
#define ki2_jl_fuzokugo_get(a, b, sz)		jl_fuzokugo_get(a, b)
#endif /* WNN7 */

#ifdef DEBUG_WNNLIB
static void showBuffers(jcConvBuf *, char *);
static void printBuffer(wchar *start, wchar *end);
#define	TRACE(f, m)	fprintf(stderr, "%s: %s\n", (f), (m));
#else
#define	TRACE(f, m)
#endif

#define CHECKFIXED(buf)	\
	{ if ((buf)->fixed) { jcErrno = JE_ALREADYFIXED; return -1; } }
#define Free(p)		{if (p) free((char *)(p));}
#define DotSet(buf)	(buf)->dot = (buf)->clauseInfo[(buf)->curLCStart].kanap

#define KANABEG	0xa4a1	/* '$B$!(B' */
#define KANAEND	0xa4f3	/* '$B$s(B' */
#define KATAOFFSET	0x100	/* $B%+%?%+%J$H$R$i$,$J$N%3!<%I!&%*%U%;%C%H(B */

/* 1$BJ8@a$NFI$_!&4A;z$r<h$j=P$9%P%C%U%!$N%5%$%:(B */
#define CL_BUFSZ	512

/* $B%G%U%)%k%H$N%P%C%U%!%5%$%:(B */
#define DEF_BUFFERSIZE	100	/* 100 $BJ8;z(B */
#define DEF_CLAUSESIZE	20	/* 20 $BJ8@a(B */
#define DEF_CANDSIZE	1024	/* 1K $B%P%$%H(B */
#define DEF_RESETSIZE	10	/* 10 $BC18l(B */

/* buf->candKind $B$NCM(B */
#define CAND_SMALL	0	/* $B>.J8@a8uJd(B */
#define CAND_LARGE	1	/* $BBgJ8@a8uJd(B */

#define MAXFZK	LENGTHBUNSETSU

#ifdef SVR4
#define bcopy(p, q, l)	memmove(q, p, l)
#define bzero(p, l)	memset(p, 0, l)
#else
#if defined(SYSV) || defined(USG)
#define OVERLAP_BCOPY
extern char	*memset();
#define bzero(p, l)	memset(p, 0, l)
#endif
#endif

/* $B%U%!%s%/%7%g%s%W%m%H%?%$%W@k8@(B */
static wchar *wstrncpy(wchar *, wchar *, int);
static int wstrlen(wchar *);
static void moveKBuf(jcConvBuf *, int, int);
static void moveDBuf(jcConvBuf *, int, int);
static void moveCInfo(jcConvBuf *, int, int);
static int resizeBuffer(jcConvBuf *, int);
static int resizeCInfo(jcConvBuf *, int);
static void setCurClause(jcConvBuf *, int);
static int getHint(jcConvBuf *, int, int);
static int renConvert(jcConvBuf *, int);
static int tanConvert(jcConvBuf *, int);
static int doKanrenConvert(jcConvBuf *, int);
static int doKantanDConvert(jcConvBuf *, int, int);
static int doKantanSConvert(jcConvBuf *, int);
static int unconvert(jcConvBuf *, int, int);
static int expandOrShrink(jcConvBuf *, int, int, int);
static int makeConverted(jcConvBuf *, int);
static int getCandidates(jcConvBuf *, int);
static int setCandidate(jcConvBuf *, int);
static void checkCandidates(jcConvBuf *, int, int);
static int forceStudy(jcConvBuf *, int);

/* $B%(%i!<HV9f(B */
int	jcErrno;

/*
 *	portability $B$N$?$a$N%U%!%s%/%7%g%s(B
 */

#ifdef OVERLAP_BCOPY
#undef bcopy
static void
bcopy(char *from, char *to, int n)
{
	if (n <= 0 || from == to) return;

	if (from < to) {
		from += n;
		to += n;
		while (n-- > 0)
			*--to = *--from;
	} else {
		while (n-- > 0)
			*to++ = *from++;
	}
}
#endif

/*
 *	wnnlib $BFbIt$G;H$o$l$k%U%!%s%/%7%g%s(B
 */
static int
wstrcmp(wchar *s1, wchar *s2)
{
        while (*s1 && *s1 == *s2)
                s1++, s2++;
        return (int)(*s1 - *s2);
}

wchar *
wstrncpy(wchar *s1, wchar *s2, int n)
{
	wchar	*ret = s1;

	while (n-- > 0 && (*s1++ = *s2++))
		;
	while (n-- > 0)
		*s1++ = 0;

	return ret;
}

/* wstrlen -- wchar $B7?J8;zNs$N(B strlen */
static int
wstrlen(wchar *s)
{
	int	n = 0;

	while (*s++)
		n++;
	return n;
}

static int
euctows(wchar *wstr, const char *euc, int len)
{
  int i, j;
  wchar wc;

  j = 0;
  for (i = 0; i < len; i++) {
    wc = (euc[j + 1] << 8) | (euc[j] & 0xff);
    wstr[i] = htons(wc);
    j += sizeof(wchar);
  }
  return j;
}

static int
wstoeuc(char *euc, const wchar *wstr, int len)
{
  int i, j;
  wchar wc;

  j = 0;
  for (i = 0; i < len; i += sizeof(wchar)) {
    wc = ntohs(wstr[j]);
    euc[i]     = wc & 0xff;
    euc[i + 1] = wc >> 8;
    j++;
  }
  return j;
}

/* moveKBuf -- $B$+$J%P%C%U%!$N;XDj$5$l$?J8@a$N@hF,$+$i$"$H$rF0$+$9(B */
static void
moveKBuf(jcConvBuf *buf, int cl, int move)
{
	jcClause	*clp = buf->clauseInfo + cl;
	jcClause	*clpend;
	int		movelen;

	TRACE("moveKBuf", "Enter")

	if (move == 0) return;

	if ((movelen = buf->kanaEnd - clp->kanap) > 0) {
		/* $B$+$J%P%C%U%!$NFbMF$rF0$+$9(B */
		(void)bcopy((char *)clp->kanap, (char *)(clp->kanap + move),
			    movelen * sizeof(wchar));
	}

	/* $B$+$J%P%C%U%!$NJQ99$K9g$o$;$F(B clauseInfo $B$r%"%C%W%G!<%H$9$k(B */
	clpend = buf->clauseInfo + buf->nClause;
	while (clp <= clpend) {
		clp->kanap += move;
		clp++;
	}

	/* kanaEnd $B$N%"%C%W%G!<%H(B */
	buf->kanaEnd += move;
}

/* moveDBuf -- $BI=<(%P%C%U%!$N;XDj$5$l$?J8@a$N@hF,$+$i$"$H$rF0$+$9(B */
static void
moveDBuf(jcConvBuf *buf, int cl, int move)
{
	jcClause	*clp = buf->clauseInfo + cl;
	jcClause	*clpend;
	int		movelen;

	TRACE("moveDBuf", "Enter")

	if (move == 0) return;

	if ((movelen = buf->displayEnd - clp->dispp) > 0) {
		/* $BI=<(%P%C%U%!$NFbMF$rF0$+$9(B */
		(void)bcopy((char *)clp->dispp, (char *)(clp->dispp + move),
			    movelen * sizeof(wchar));
	}

	/* $BI=<(%P%C%U%!$NJQ99$K9g$o$;$F(B clauseInfo $B$r(B
	 * $B%"%C%W%G!<%H$9$k(B
	 */
	clpend = buf->clauseInfo + buf->nClause;
	while (clp <= clpend) {
		clp->dispp += move;
		clp++;
	}

	/* displayEnd $B$N%"%C%W%G!<%H(B */
	buf->displayEnd += move;
}

/* moveCInfo -- ClauseInfo $B$N;XDj$5$l$?J8@a$N@hF,$+$i$"$H$rF0$+$9(B */
static void
moveCInfo(jcConvBuf *buf, int cl, int move)
{
	jcClause	*clp = buf->clauseInfo + cl;
	int		len;

	TRACE("moveCInfo", "Enter")

	/* move $B$K@5$N?t$r;XDj$9$l$PJ8@a$NA^F~!"Ii$J$iJ8@a$N:o=|$K$J$k(B */

	if (move == 0) return;

	if ((len = buf->nClause + 1 - cl) > 0) {
		(void)bcopy((char *)clp, (char *)(clp + move),
			    len * sizeof(jcClause));
	}
	buf->nClause += move;

	/*
	 * $B8uJd$r<h$j=P$7$F$$$kJ8@a$,$"$l$P!"L58z$K$7$F$*$/!#(B
	 *
	 * $B$?$@$7!"8uJd$r<h$j=P$7$?7k2L!"J8@a?t$,JQ2=$7$?>l9g$K$O!"(B
	 * setCandidate() $B$NCf$G@_Dj$7$J$*$5$l$k!"$^$?!"(Bjllib $BFb$G$b(B
	 * $BF1$8J8@a$KBP$9$kA48uJd<h$j=P$7$,$"$C$?>l9g$N9MN8$,$"$k!#(B
	 * $B$H$$$&$3$H$G!"$3$3$O0BA4%5%$%I$G$$$/!#(B
	 */
	if (buf->candClause >= 0) {
		buf->candClause = -1;
		buf->candClauseEnd = -1;
	}
}

/* resizeBuffer -- $B$+$J(B/$BI=<(%P%C%U%!$NBg$-$5$rJQ$($k(B */
static int
resizeBuffer(jcConvBuf *buf, int len)
{
	wchar	*kbufold, *dbufold;
	wchar	*kbufnew, *dbufnew;
	int	allocsize;
	jcClause	*clp, *clpend;

	TRACE("resizeBuffer", "Enter")

	kbufold = buf->kanaBuf;
	dbufold = buf->displayBuf;

	/* realloc $B$9$k(B */
	allocsize = (len + 1) * sizeof(wchar);
	kbufnew = (wchar *)realloc((char *)kbufold, allocsize);
	dbufnew = (wchar *)realloc((char *)dbufold, allocsize);

	if (kbufnew == NULL || dbufnew == NULL) {
		Free(kbufnew);
		Free(dbufnew);
		jcErrno = JE_NOCORE;
		return -1;
	}

	buf->bufferSize = len;

	if (kbufnew == kbufold && dbufnew == dbufold) {
		/* $B%]%$%s%?$OA0$HJQ$o$C$F$$$J$$(B */
		return 0;
	}

	/* $B3F<o%]%$%s%?$r$D$1JQ$($k(B */

	buf->kanaBuf = kbufnew;
	buf->kanaEnd = kbufnew + (buf->kanaEnd - kbufold);
	buf->displayBuf = dbufnew;
	buf->displayEnd = dbufnew + (buf->displayEnd - dbufold);

	buf->dot = kbufnew + (buf->dot - kbufold);

	clp = buf->clauseInfo;
	clpend = clp + buf->nClause;
	while (clp <= clpend) {
		clp->kanap = kbufnew + (clp->kanap - kbufold);
		clp->dispp = dbufnew + (clp->dispp - dbufold);
		clp++;
	}

	return 0;
}

/* resizeCInfo -- clauseInfo $B%P%C%U%!$NBg$-$5$rJQ$($k(B */
static int
resizeCInfo(jcConvBuf *buf, int size)
{
	jcClause	*cinfonew;

	TRACE("resizeCInfo", "Enter")

	/* realloc $B$9$k(B */
	cinfonew = (jcClause *)realloc((char *)buf->clauseInfo,
				       (size + 1) * sizeof(jcClause));
	if (cinfonew == NULL) {
		jcErrno = JE_NOCORE;
		return -1;
	}

	buf->clauseSize = size;
	buf->clauseInfo = cinfonew;
	return 0;
}

/* setCurClause -- $B%+%l%s%HJ8@a$r@_Dj$9$k(B */
static void
setCurClause(jcConvBuf *buf, int cl)
{
	jcClause	*clp = buf->clauseInfo;
	int		i;

	TRACE("setCurClause", "Enter")

	/* $B%+%l%s%H>.J8@a(B */
	buf->curClause = cl;

	/* $B%+%l%s%HBgJ8@a3+;OJ8@a(B */
	for (i = cl; i > 0 && !clp[i].ltop; i--)
		;
	buf->curLCStart = i;

	/* $B%+%l%s%HBgJ8@a=*N;J8@a(B ($B$N<!(B) */
	for (i = cl + 1; i <= buf->nClause && !clp[i].ltop; i++)
		;
	buf->curLCEnd = i;
}

/* getHint -- $BJ8@a$NA08e$N@\B3>pJs$rF@$k(B */
static int
getHint(jcConvBuf *buf, int start, int end)
{
	jcClause *cinfo = buf->clauseInfo;
	int hint = 0;

	TRACE("getHint", "Enter")

	/*
	 * $B:G=i$NJ8@a$ND>A0$NJ8@a$,JQ49$5$l$F$$$l$P!"A0$NJ8@a$H@\B3$r$9$k(B
	 */
	if (start > 0 && cinfo[start - 1].conv == 1)
		hint |= WNN_USE_MAE;

	/*
	 * $B:G8e$NJ8@a$ND>8e$,JQ49$5$l$F$$$F$$$l$P!"8e$NJ8@a$H@\B3$r$9$k(B
	 */
	if (end > 0 && end < jl_bun_suu(buf->wnn) && cinfo[end].conv == 1)
		hint |= WNN_USE_ATO;

	return hint;
}


/* renConvert -- $B%+%l%s%HJ8@a$+$i8e$m$rO"J8@aJQ49$9$k(B */
static int
renConvert(jcConvBuf *buf, int small)
{
	TRACE("renConvert", "Enter")

	/* $BO"J8@aJQ49$9$k(B */
	if (doKanrenConvert(buf,
			    small ? buf->curClause : buf->curLCStart) < 0) {
		return -1;
	}

	/*
	 * $B%+%l%s%HJ8@a$N@_Dj(B
	 * small $B$,(B 0 $B$J$i!"(B
	 *	$B%+%l%s%HBgJ8@a$N@hF,$O(B buf->curLCStart $B$GJQ$o$i$:(B
	 *	$B%+%l%s%HBgJ8@a=*$j$O(B ltop $B%U%i%0$r%5!<%A$7$FC5$9(B
	 *	$B%+%l%s%H>.J8@a$O%+%l%s%HBgJ8@a@hF,$K0\F0(B
	 * small $B$,(B 0 $B$G$J$$$J$i!"(B
	 *	$B%+%l%s%H>.J8@a$O(B buf->curClause $B$GJQ$o$i$:(B
	 *	$B%+%l%s%HBgJ8@a$N@hF,$*$h$S=*$j$O!"%+%l%s%H>.J8@a$N(B
	 *	$BA08e$r(B ltop $B%U%i%0$r%5!<%A$7$FC5$9(B
	 */
	setCurClause(buf, small ? buf->curClause : buf->curLCStart);

	/* $B%I%C%H$N@_Dj(B */
	DotSet(buf);

	return 0;
}

/* tanConvert -- $B%+%l%s%HJ8@a$rC1J8@aJQ49$9$k(B */
static int
tanConvert(jcConvBuf *buf, int small)
{
	TRACE("tanConvert", "Enter")

	/*
	 * $BC1J8@aJQ49$N>l9g!"4pK\E*$K(B 2 $BCJ3,$N=hM}$r9T$J$&$3$H$K$J$k(B
	 * $B$^$:!"%+%l%s%HJ8@a$rC1J8@aJQ49(B
	 * $B<!$K!"$=$N$"$H$rO"J8@aJQ49(B
	 */

	if (small) {
		/* $B$^$:C1J8@aJQ49$9$k(B */
		if (doKantanSConvert(buf, buf->curClause) < 0)
			return -1;

		/* $B%+%l%s%HJ8@a$N@_Dj(B
		 *	$B%+%l%s%H>.J8@a$O(B buf->curClause $B$GJQ$o$i$:(B
		 *	$B%+%l%s%HBgJ8@a$N@hF,$H:G8e$O%+%l%s%H>.J8@a$N(B
		 *	$BA08e$K(B ltop $B%U%i%0$r%5!<%A$7$FC5$9(B
		 */
		setCurClause(buf, buf->curClause);
		/* $B%I%C%H$N@_Dj(B */
		DotSet(buf);

		/* $BO"J8@aJQ49(B */
		if (buf->curClause + 1 < buf->nClause &&
		    buf->clauseInfo[buf->curClause + 1].conv == 0) {
			/* $B>.J8@a$NC1J8@aJQ49%b!<%I$G!"<!$NJ8@a$,(B
			 * $BL5JQ49$@$C$?>l9g!"(Bltop $B%U%i%0$r(B 0 $B$K$7$F(B
			 * $BA0$H@\B3$G$-$k$h$&$K$9$k(B
			 */
			buf->clauseInfo[buf->curClause + 1].ltop = 0;
		}
		if (doKanrenConvert(buf, buf->curClause + 1) < 0)
			return -1;

		/* $B$b$&0lEY%+%l%s%HJ8@a$N@_Dj(B
		 * $BO"J8@aJQ49$N7k2L$K$h$C$F$O%+%l%s%HBgJ8@a$N:G8e$,(B
		 * $B0\F0$9$k$3$H$,$"$k(B
		 */
		setCurClause(buf, buf->curClause);

		/* $B%I%C%H$O0\F0$7$J$$$N$G:F@_Dj$7$J$/$F$h$$(B */
	} else {
		/* $B$^$:C1J8@aJQ49$9$k(B */
		if (doKantanDConvert(buf, buf->curLCStart, buf->curLCEnd) < 0)
			return -1;

		/* $B%+%l%s%HJ8@a$N@_Dj(B
		 *	$B%+%l%s%HBgJ8@a$N@hF,$O(B buf->curLCStart $B$GJQ$o$i$:(B
		 *	$B%+%l%s%HBgJ8@a=*$j$O(B ltop $B%U%i%0$r%5!<%A$7$FC5$9(B
		 *	$B%+%l%s%H>.J8@a$O%+%l%s%HBgJ8@a@hF,$K0\F0(B
		 */
		setCurClause(buf, buf->curLCStart);
		DotSet(buf);

		/* $BO"J8@aJQ49(B */
		if (doKanrenConvert(buf, buf->curLCEnd) < 0)
			return -1;
		/* $B$3$A$i$O(B small $B$N;~$H0c$C$FO"J8@aJQ49$N7k2L%+%l%s%HJ8@a$,(B
		 * $B0\F0$9$k$3$H$O$J$$(B
		 */
	}

	return 0;
}

/* doKanrenConvert -- $B;XDj$5$l$?J8@a$+$i8e$m$rO"J8@aJQ49$9$k(B */
static int
doKanrenConvert(jcConvBuf *buf, int cl)
{
	jcClause	*clp;
	wchar	*kanap, *dispp;
	wchar	savechar;
	int	nsbun;
	int	len, n;

	TRACE("doKanrenConvert", "Enter")

	/*
	 * $B;XDj$5$l$?J8@a$+$i8e$m$rO"J8@aJQ49$9$k(B
	 * $B%+%l%s%HJ8@a$N:F@_Dj$J$I$O$7$J$$(B
	 */

	if (cl >= buf->nClause) {
		/* $B;XDj$5$l$?J8@a$O$J$$(B
		 * $B%(%i!<$K$O$7$J$$(B
		 * $B6u$NJ8@a$rJQ49$7$h$&$H$7$?;~$K!"$=$l$r;vA0$K%A%'%C%/$7$F(B
		 * $B%(%i!<$K$9$k$N$O>e0L$N4X?t$N@UG$$G$"$k(B
		 */
		return 0;
	}

	/*
	 * $BJQ49$9$kA0$K!">/$J$/$H$b;XDj$5$l$?J8@a$ND>A0$^$G$,JQ49$5$l$F(B
         * $B$$$k$3$H$rJ]>Z$9$k(B
	 */
	if (makeConverted(buf, cl) < 0) {
		return -1;
	}

	clp = buf->clauseInfo + cl;

	/* $B$+$J%P%C%U%!$r(B NULL $B%?!<%_%M!<%H$5$;$F$*$/(B */
	*(buf->kanaEnd) = 0;

	/* $BO"J8@aJQ49$9$k(B */
#ifdef WNN6
	nsbun = jl_fi_ren_conv(buf->wnn, clp->kanap,
				 cl, -1, getHint(buf, cl, -1));
#else
	nsbun = jl_ren_conv(buf->wnn, clp->kanap,
				 cl, -1, getHint(buf, cl, -1));
#endif

	if (nsbun < 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}

	/* clauseInfo $B$N%5%$%:$N%A%'%C%/(B */
	if (nsbun > buf->clauseSize) {
		if (resizeCInfo(buf, cl + nsbun) < 0)
			return -1;
	}

	/* $B<!$KJQ49J8;zNs$ND9$5$N%A%'%C%/(B */
	clp = buf->clauseInfo + cl;
	len = (clp->dispp - buf->displayBuf) + jl_kanji_len(buf->wnn, cl, -1);

	if (len > buf->bufferSize) {
		if (resizeBuffer(buf, len) < 0)
			return -1;
	}

	buf->nClause = nsbun;

	/* $B$G$O(B clauseInfo $B$KJQ497k2L$rF~$l$F$$$/(B */
	clp = buf->clauseInfo + cl;
	kanap = clp->kanap;
	dispp = clp->dispp;
	while (cl < buf->nClause) {
		n = cl + 1;

		/* $BJ8@a>pJs$N@_Dj(B */
		clp->conv = 1;
		clp->kanap = kanap;
		clp->dispp = dispp;
		clp->ltop = jl_dai_top(buf->wnn, cl);

		/* $BI=<(%P%C%U%!$XJQ49J8;zNs$r%3%T!<$9$k(B */
		/* jl_get_kanji $B$O!"(BNULL $B$^$G%3%T!<$9$k$N$GCm0U(B */
		len = jl_kanji_len(buf->wnn, cl, n);
		savechar = dispp[len];
		(void)ki2_jl_get_kanji(buf->wnn, cl, n, dispp, len);
		dispp[len] = savechar;
		dispp += len;

		/* $B$+$J%P%C%U%!$N0LCV$rJ8@a$N:G8e$K$9$k(B */
		kanap += jl_yomi_len(buf->wnn, cl, n);

		/* $B%+%l%s%HJ8@a$N99?7(B */
		cl = n;
		clp++;
	}

	/* $B:G8e$N(B clauseInfo $B$N@_Dj(B */
	clp->kanap = buf->kanaEnd;
	clp->dispp = buf->displayEnd = dispp;
	clp->conv = 0;
	clp->ltop = 1;

#ifdef DEBUG_WNNLIB
	showBuffers(buf, "after doKanrenConvert");
#endif

	return 0;
}

/* doKantanDConvert -- $B;XDj$5$l$?HO0O$NJ8@a$rBgJ8@a$H$7$FC1J8@aJQ49$9$k(B */
static int
doKantanDConvert(jcConvBuf *buf, int cls, int cle)
{
	jcClause	*clps, *clpe;
	int	len, diff, newlen;
	int	cldiff, nclausenew;
	wchar	*kanap, *dispp;
	wchar	savechar;
	wchar	*savep;
	int	nsbunnew, nsbunold;
	int	i, n;

	TRACE("doKantanDConvert", "Enter")

	/*
	 * $BJQ49$9$kA0$K!">/$J$/$H$b;XDj$5$l$?J8@a$ND>A0$^$G$,JQ49$5$l$F(B
         * $B$$$k$3$H$rJ]>Z$9$k(B
	 */
	if (makeConverted(buf, cls) < 0)
		return -1;

	/*
	 * $B;XDj$5$l$?HO0O$NJ8@a$rBgJ8@a$H$7$FC1J8@aJQ49$9$k(B
	 * $B%+%l%s%HJ8@a$N:F@_Dj$J$I$O$7$J$$(B
	 */

	clps = buf->clauseInfo + cls;
	clpe = buf->clauseInfo + cle;
	nsbunold = jl_bun_suu(buf->wnn);
	if (nsbunold < 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}

	/*
	 * $BFI$_$r(B NULL $B%?!<%_%M!<%H$9$k(B
	 * $BC1$K(B 0 $B$rF~$l$k$H<!$NJ8@a$,2u$l$k$N$G!"$=$NA0$K%;!<%V$7$F$*$/(B
	 */
	savep = clpe->kanap;
	savechar = *savep;
	*savep = 0;

	/* $BC1J8@aJQ49$9$k(B */
	nsbunnew = jl_tan_conv(buf->wnn, clps->kanap, cls, cle,
				 getHint(buf, cls, cle), WNN_DAI);

	/* $B$9$+$5$:%;!<%V$7$F$"$C$?J8;z$r$b$H$KLa$9(B */
	*savep = savechar;

	if (nsbunnew < 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}

	cldiff = (cle - cls) - (nsbunold - nsbunnew);
	nclausenew = buf->nClause + cldiff;
	/* clauseInfo $B$N%5%$%:$N%A%'%C%/(B */
	if (nclausenew > buf->clauseSize) {
		if (resizeCInfo(buf, nclausenew) < 0)
			return -1;
	}

	/* $BJQ49J8;zNs$ND9$5$N%A%'%C%/(B */
	len = jl_kanji_len(buf->wnn, cls, cle + cldiff);
	diff = len - (clpe->dispp - clps->dispp);
	newlen = (buf->displayEnd - buf->displayBuf) + diff;
	if (newlen > buf->bufferSize) {
		if (resizeBuffer(buf, newlen) < 0)
			return -1;
	}

	/*
	 * $BJ8@a$rA^F~$9$k$N$G!"I=<(%P%C%U%!$NFbMF$r0\F0$5$;$k!#(B
	 *
	 * $B$I$&$;$"$H$+$iO"J8@aJQ49$9$k$+$i$$$$$G$O$J$$$+$H$$$&9M$(J}$b$"$k$,!"(B
	 * $B$I$3$G%(%i!<$,5/$3$C$F$b0l1~$N(B consistency $B$,J]$?$l$k$h$&$K(B
	 * $B$9$k$H$$$&$N$,L\I8$G$"$k(B
	 */
	moveDBuf(buf, cle, diff);

	/* clauseInfo $B$rF0$+$9(B ($BF1;~$K(B nClause $B$b%"%C%W%G!<%H$5$l$k(B) */
	moveCInfo(buf, cle, cldiff);

	/* $B$G$O(B clauseInfo $B$KJQ497k2L$rF~$l$k(B */
	clps = buf->clauseInfo + cls;
	kanap = clps->kanap;
	dispp = clps->dispp;
	cldiff += (cle - cls);
	for (i = 0; i < cldiff; i++) {
		n = cls + 1;

		/* $BJ8@a>pJs$r@_Dj$9$k(B */
		clps->conv = 1;
		clps->ltop = jl_dai_top(buf->wnn, cls);
		clps->kanap = kanap;
		clps->dispp = dispp;

		/* $BI=<(%P%C%U%!$X$NJQ49J8;zNs$N%3%T!<(B */
		/* jl_get_kanji $B$O!"(BNULL $B$^$G%3%T!<$9$k$N$GCm0U(B */
		len = jl_kanji_len(buf->wnn, cls, n);
		savechar = dispp[len];
		(void)ki2_jl_get_kanji(buf->wnn, cls, n, dispp, len);
		dispp[len] = savechar;
		dispp += len;

		/* $B$+$J%P%C%U%!$N0LCV$r99?7(B */
		kanap += jl_yomi_len(buf->wnn, cls, n);

		/* $B<!$NJ8@a>pJs$N99?7(B */
		cls = n;
		clps++;
	}

	/* $B<!$N(B clauseInfo $B$N@_Dj(B */
	if (cls < jl_bun_suu(buf->wnn))
		clps->ltop = jl_dai_top(buf->wnn, cls);
	else
		clps->ltop = 1;

	return 0;
}

/* doKantanSConvert -- $B;XDj$5$l$?J8@a$r>.J8@a$H$7$FC1J8@aJQ49$9$k(B */
static int
doKantanSConvert(jcConvBuf *buf, int cl)
{
	int	next = cl + 1;
	jcClause	*clp;
	int	len, newlen, diff;
	wchar	savechar;
	wchar	*savep;
	int	nsbun;

	TRACE("doKantanSConvert", "Enter")

	/*
	 * $BJQ49$9$kA0$K!">/$J$/$H$b;XDj$5$l$?J8@a$ND>A0$^$G$,JQ49$5$l$F(B
         * $B$$$k$3$H$rJ]>Z$9$k(B
	 */
	if (makeConverted(buf, cl) < 0)
		return -1;

	/*
	 * $B;XDj$5$l$?J8@a$r>.J8@a$H$7$FC1J8@aJQ49$9$k(B
	 * $B%+%l%s%HJ8@a$N:F@_Dj$J$I$O$7$J$$(B
	 */

	clp = buf->clauseInfo + cl;

	/*
	 * $BFI$_$r(B NULL $B%?!<%_%M!<%H$9$k(B
	 * $BC1$K(B 0 $B$rF~$l$k$H<!$NJ8@a$,2u$l$k$N$G!"$=$NA0$K%;!<%V$7$F$*$/(B
	 */
	savep = (clp + 1)->kanap;
	savechar = *savep;
	*savep = 0;

	/* $BC1J8@aJQ49$9$k(B */
	nsbun = jl_tan_conv(buf->wnn, clp->kanap, cl, next,
				getHint(buf, cl, next), WNN_SHO);


	/* $B$9$+$5$:%;!<%V$7$F$"$C$?J8;z$r$b$H$KLa$9(B */
	*savep = savechar;

	if (nsbun < 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}

	/* $BJQ49J8;zNs$ND9$5$N%A%'%C%/(B */
	clp = buf->clauseInfo + cl;
	len = jl_kanji_len(buf->wnn, cl, -1);
	diff = len - ((clp + 1)->dispp - clp->dispp);
	newlen = (buf->displayEnd - buf->displayBuf) + diff;
	if (newlen > buf->bufferSize) {
		if (resizeBuffer(buf, newlen) < 0)
			return -1;
	}

	/* $BJ8@a$rA^F~$9$k$N$G!"I=<(%P%C%U%!$NFbMF$r0\F0$5$;$k(B */
	/* $B$I$&$;$"$H$+$iO"J8@aJQ49$9$k$+$i$$$$$G$O$J$$$+$H$$$&9M$(J}$b$"$k$,!"(B
	 * $B$I$3$G%(%i!<$,5/$3$C$F$b0l1~$N(B consistency $B$,J]$?$l$k$h$&$K(B
	 * $B$9$k$H$$$&$N$,L\I8$G$"$k(B
	 */
	moveDBuf(buf, next, diff);

	/* $B$G$O(B clauseInfo $B$KJQ497k2L$rF~$l$k(B */
	clp = buf->clauseInfo + cl;
	clp->conv = 1;
	clp->ltop = jl_dai_top(buf->wnn, cl);

	/* $BI=<(%P%C%U%!$XJQ49J8;zNs$r%3%T!<(B */
	/* jl_get_kanji $B$G$O!":G8e$N(B NULL $B$b%3%T!<$5$l$k$N$GCm0U(B */
	savechar = clp->dispp[len];
	(void)ki2_jl_get_kanji(buf->wnn, cl, next, clp->dispp, len);
	clp->dispp[len] = savechar;

	/* $B<!$N(B clauseInfo $B$N@_Dj(B */
	if (next < jl_bun_suu(buf->wnn))
		(clp + 1)->ltop = jl_dai_top(buf->wnn, next);

	return 0;
}


/* makeConverted -- $B;XDj$5$l$?J8@a$ND>A0$^$G$,(B jllib $B$GJQ49$5$l$F$$$k(B
   $B$3$H$rJ]>Z$9$k(B */
static int
makeConverted(jcConvBuf *buf, int cl)
{
	int	nsbun;
	int	next;
	int	status;
	wchar	savechar;
	jcClause	*clpc, *clpn;

	TRACE("makeConverted", "Enter")

#ifdef DEBUG_WNNLIB
	showBuffers(buf, "before makeConverted");
#endif

	/* $B4{$KJQ49$5$l$F$$$k$+%A%'%C%/$9$k(B */
	nsbun = jl_bun_suu(buf->wnn);
	if (cl <= nsbun)
		return 0;

	/* $BJQ49$5$l$F$$$J$$J8@a$rEPO?$9$k(B */
	clpc = buf->clauseInfo + nsbun;
	for (; nsbun < cl; nsbun = next, clpc = clpn) {
		clpn = clpc + 1;
		next = nsbun + 1;

		/* $B4{$KEPO?$5$l$F$$$l$P!"2?$b$7$J$$(B */
		if (clpc->conv == 1)
			continue;

		/* $BI=<(J8;zNs$r(B NULL $B%?!<%_%M!<%H$9$k(B */
		savechar = *clpn->dispp;
		*clpn->dispp = 0;

		/*
		 * jllib $B$K$OL5JQ49$NJ8@a$rEPO?$9$k5!G=$,$J$$$N$G!"(B
		 * $B$H$j$"$($:A08e$N@\B3$J$7$GC1J8@aJQ49$9$k$3$H$K$9$k(B
		 */
		status = jl_tan_conv(buf->wnn, clpc->dispp,
					nsbun, next, WNN_NO_USE, WNN_SHO);

		/* $B%;!<%V$7$?J8;z$rLa$9(B */
		*clpn->dispp = savechar;

		if (status < 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}
	}

#ifdef DEBUG_WNNLIB
	showBuffers(buf, "after makeConverted");
#endif

	return 0;
}

/* unconvert -- $B;XDj$5$l$?HO0O$NJ8@a$r0l$D$NL5JQ49$NJ8@a$K$9$k(B */
static int
unconvert(jcConvBuf *buf, int start, int end)
{
	jcClause	*clps, *clpe;
	int	diff, len;
	wchar	savechar;

	TRACE("unconvert", "Enter")

	if (end <= start)
		return 0;

	if (start >= buf->nClause)
		return 0;

#ifdef DEBUG_WNNLIB
	showBuffers(buf, "before unconvert");
#endif

	clps = buf->clauseInfo + start;
	clpe = buf->clauseInfo + end;

	/*
	 * $BI=<(%P%C%U%!$NFbMF$r$+$J%P%C%U%!$NFbMF$GCV$-49$($k(B
	 * $B!D$H$$$C$F$b<B:]$NF0:n$O$=$l$[$I4JC1$G$O$J$$(B
	 *
	 * $B!&$^$:!"CV$-49$($?7k2L!"I=<(%P%C%U%!$,$"$U$l$J$$$+D4$Y!"(B
	 *   $B$"$U$l$k$h$&$J$i%P%C%U%!$N%5%$%:$rBg$-$/$9$k(B
	 * $B!&I=<(%P%C%U%!$K!"$+$J%P%C%U%!$+$i%G!<%?$r0\$9(B
	 * $B!&(BclauseInfo $B$r=q$-49$($F!"(Bstart $B$+$i(B end-1 $B$^$G$NJ8@a$r(B
	 *   $B0l$D$NL5JQ49$NJ8@a$K$^$H$a$k(B
	 * $B!&$b$A$m$s(B nClause $B$bJQ$($k(B
	 * $B!&(Bstart+1 $B$+$i:G8e$^$G$NJ8@a$N(B clauseInfo $B$N(B dispp $B$r(B
	 *   $BI=<(%P%C%U%!$N$:$l$K1~$8$FD4@0$9$k(B
	 *
	 * $B$=$NB>$K<!$N$3$H$b9T$J$&I,MW$,$"$k$,!"$3$N4X?t$G$O$d$i$J$$(B
	 * $B>e0L$N4X?t$G@_Dj$9$k$3$H(B
	 * $B!&BgJ8@a%U%i%0(B (ltop) $B$N@_Dj(B
	 * $B!&%+%l%s%HJ8@a!"$*$h$S<!8uJdJ8@a$N0\F0(B
	 *   $B<!8uJdJ8@a$,L5JQ49$NJ8@a$K$J$C$F$7$^$C$?;~$N=hM}(B
	 * $B!&%I%C%H$N0\F0(B
	 */

	/* $BFI$_$ND9$5$H4A;z$ND9$5$N:9$rD4$Y$k(B */
	diff = (clpe->kanap - clps->kanap) - (clpe->dispp - clps->dispp);
	/* $BCV$-49$($?>l9g$NI=<(%P%C%U%!$ND9$5(B */
	len = (buf->displayEnd - buf->displayBuf) + diff;
	/* $B%P%C%U%!$N%5%$%:$,B-$j$J$1$l$P%5%$%:$rBg$-$/$9$k(B */
	if (len > buf->bufferSize) {
		if (resizeBuffer(buf, len) < 0) {
			/* $B%5%$%:$,JQ$($i$l$J$+$C$?(B */
			return -1;
		}
	}

	/* $BCV$-49$((B */
	/* $B$^$:8e$m$NItJ,$rF0$+$7$F$+$i(B */
	moveDBuf(buf, end, diff);
	/* $BFI$_$rF~$l$k(B */
	(void)bcopy((char *)clps->kanap, (char *)clps->dispp,
		    (clpe->kanap - clps->kanap) * sizeof(wchar));

	/*
	 * start $B$+$i(B end $B$^$G$NJ8@a$r0l$D$K$^$H$a$k(B
	 */

	/* $BL5JQ49>uBV$K$J$C$?J8@a$N(B clauseInfo $B$N@_Dj(B */
	clps->conv = 0;

	/* end $B$+$i$"$H$N(B clauseInfo $B$r(B'$B$D$a$k(B' */
	moveCInfo(buf, end, start + 1 - end);

	/* $BJ8@a$rEPO?$9$k(B */
	/* $BEPO?$5$l$F$$$kJ8@a$ND9$5$r%A%'%C%/(B */
	if (jl_bun_suu(buf->wnn) < end)
		end = -1;

	/* $BEPO?$9$kA0$K!"FI$_$r(B NULL $B%?!<%_%M!<%H$7$F$*$/(B */
	clpe = clps + 1;
	savechar = *clpe->kanap;
	*clpe->kanap = 0;

	/* $BL5JQ49$GEPO?$7$?$$$,$G$-$J$$$N$G!"A08e$N@\B3$J$7$G!"C1J8@a(B
	 * $BJQ49$9$k(B
	 */

	len = jl_tan_conv(buf->wnn, clps->kanap,
				 start, end, WNN_NO_USE, WNN_SHO);

	/* $BFI$_$r85$K!"La$7$F$*$/(B */
	*clpe->kanap = savechar;

#ifdef DEBUG_WNNLIB
	showBuffers(buf, "after unconvert");
#endif

	/* $BEPO?$G$-$?$+$r!"%A%'%C%/(B */
	if (len < 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}

	return 0;
}

static int
expandOrShrink(jcConvBuf *buf, int small, int expand, int convf)
{
	jcClause	*clp, *clpe;
	wchar	*kanap, *dispp;
	int	start, end;
	int	len;
	int	nsbun;

	TRACE("expandOrShrink", "Enter")

	start = small ? buf->curClause : buf->curLCStart;
	end = small ? start + 1 : buf->curLCEnd;

	clp = buf->clauseInfo + start;
	clpe = buf->clauseInfo + end;

	/*
	 * $B?-$S=L$_$G$-$k$+$N%A%'%C%/(B
	 */
	if (expand) {
		/*
		 * $B%+%l%s%HJ8@a$,:G8e$NJ8@a$N;~$K$O(B
		 * $B$b$&9-$2$i$l$J$$(B
		 */
		if (end >= buf->nClause) {
			jcErrno = JE_CANTEXPAND;
			return -1;
		}
		len = 1;
	} else {
		if (buf->curClause == buf->nClause ||
		    clpe->kanap - clp->kanap <= 1) {
			/* $B%+%l%s%HJ8@a$,6u$+!"$"$k$$$OD9$5$,#10J2<(B */
			jcErrno = JE_CANTSHRINK;
			return -1;
		}
		len = -1;
	}

	/* $BA48uJdJ8@a$,%+%l%s%HBgJ8@a$+$=$l0J9_$K$"$l$PL58z$K$9$k(B */
	checkCandidates(buf, start, buf->nClause);

	/* jclib $B$H8_49$rJ]$D$?$a!":FJQ49;XDj$G$J$$>l9g$O!"FCJL$K=hM}$9$k(B */
	if (!convf) {
		/* jclib $B$HF1MM$K(B unconvert() $B$r;H$C$F!"=hM}$r$7$F$bNI$$$N(B
		 * $B$@$,!"L5BL$,$"$k$N$GFH<+$N=hM}$H$9$k(B
		 */
		int ksize;
		int dsize;

		/* jllib $B$N>pJs$,$"$l$P!"%+%l%s%HJ8@a0J9_$rL58z$K$9$k(B */
		if (start < jl_bun_suu(buf->wnn))
			jl_kill(buf->wnn, start, -1);

		/* $B%+%l%s%HJ8@a0J9_$NI=<(%P%C%U%!$NFbMF$r!"$+$J%P%C%U%!(B
		 * $B$NFbMF$GCV49$($k(B (unconvert() $B;2>H(B)
		 */

		clp = buf->clauseInfo + start;

		/* $B$^$:!"I=<(%P%C%U%!$NBg$-$5$rD4$Y!"I,MW$J$i$P%P%C%U%!(B
		 * $B$r3HD%$9$k(B
		 */
		ksize = buf->kanaEnd - clp->kanap;
		dsize = ksize + (clp->dispp - buf->displayBuf);
		if (dsize > buf->bufferSize) {
			if (resizeBuffer(buf, dsize))
				return -1;
		}

		/* $BI=<(%P%C%U%!$NFbMF$r!"$+$J%P%C%U%!$NFbMF$GCV49$($k(B */
		bcopy(clp->kanap, clp->dispp, ksize * sizeof (wchar));

		/* $BI=<(%P%C%U%!$N=*$j$r@_Dj$9$k(B */
		buf->displayEnd = clp->dispp + ksize;

		/* $B%+%l%s%HJ8@a$r@_Dj$9$k(B
		 */
		buf->curClause = buf->curLCStart = start;
		buf->dot = clp->kanap;
		clp->conv = 0;
		clp->ltop = 1;

		/* $B?-=L$7$?7k2L!"J8@a?t$O(B start + 1 ($B%+%l%s%HJ8@a$ND9(B
		 * $B$5$,(B 1 $B$G$"$C$?;~!"=L$a$?7k2L%+%l%s%HJ8@a$,$J$/$J$k!#(B
		 * $B$^$?$O!"%+%l%s%HJ8@a$N8e$K$R$H$D$NJ8@a$7$+$J$/!"$=(B
		 * $B$NJ8@a$ND9$5$,(B 1 $B$G$"$C$?>l9g!"?-$P$7$?7k2L%+%l%s%H(B
		 * $BJ8@a$h$j8e$NJ8@a$,$J$/$J$k(B) $B$+(B start + 2 $B$K$J$k(B
		 */

		/* $B$^$:!"?-=L8e$N%+%l%s%HJ8@a$ND9$5$r7W;;$9$k(B */
		ksize = buf->clauseInfo[end].kanap - clp->kanap + len;

		/* $B$=$7$F!"%+%l%s%HJ8@a$N8e$K$"$kJ8@a$r@_Dj$9$k(B */
		if (ksize == 0 || buf->displayEnd == clp->dispp + ksize) {
			/* $B=L$a$?7k2L%+%l%s%HJ8@a$,$J$/$J$C$?$+!"(B
			 * $B?-$P$7$?7k2L%+%l%s%HJ8@a$N8e$NJ8@a$,$J$/$J$C$?(B
			 *
			 * $B$3$l$i$N>l9g$O!"A0$N%+%l%s%HJ8@a0J9_$r$R$H(B
			 * $B$^$H$a(B ($B$R$H$D$NBgJ8@a(B) $B$K$7$F!"$=$l$r%+%l(B
			 * $B%s%HJ8@a(B ($BBgJ8@a(B) $B$H$7$F$7$^$&(B
			 *
			 * $B$3$N;~!"(BclauseInfo $B$NBg$-$5$O!"I,$:(B start + 1
			 * $B$h$jBg$-$$$3$H$,J]>Z$5$l$F$$$k(B
			 */
			buf->nClause = buf->curLCEnd = start + 1;

			/* $BKvHxJ8@a$r%]%$%s%H$5$;$k(B */
			clp++;
		} else if (start + 2 > buf->clauseSize
				&& resizeCInfo(buf, start + 1) < 0) {
			/* $B=L$a$h$&$H$9$kJ8@a$,:G8e$NJ8@a$@$C$?>l9g!"(B
			 * $BD9$5$,(B 1 $B$NJ8@a$,A}$($k$3$H$K$J$k!#(B
			 * $B$,!"(BclauseInfo $B$NBg$-$5$r%A%'%C%/$7!"$=$l$r(B
			 * $BA}$d$;$J$+$C$?$N$G!"%+%l%s%HJ8@a0J9_$rA4It$R(B
			 * $B$H$^$H$a$K$9$k(B ($B%P%C%U%!$N@09g@-$rJ]$D$?$a(B)
			 */
			buf->nClause = buf->curLCEnd = start + 1;
			clp++;
			clp->kanap = buf->kanaEnd;
			clp->dispp = buf->displayEnd;
			clp->conv = 0;
			clp->ltop = 1;

			/* $B$G$b!"%(%i!<$O%(%i!<$J$N$G!"%(%i!<$H$7$FJV$9(B */
#ifdef DEBUG_WNNLIB
			showBuffers(buf,
				"after expandOrShrink [noconv, error]");
#endif
			return -1;
		} else {
			/* $B?-=L$G$-$?$N$G!"%+%l%s%HJ8@a$N8e$NJ8@a$r@_Dj$9$k(B
			 * ($B$"$^$j!"0UL#$O$J$$$H$O;W$&$,!">.J8@a$N?-=L8e$N(B
			 *  $BBgJ8@a$N@_Dj$O!"(Bjclib $B$N@_Dj$HF1$8$K$7$F$*$/(B)
			 */
			buf->curLCEnd = start + (small ? 2 : 1);
			buf->nClause = start + 2;
			clpe = clp + 1;
			clpe->kanap = clp->kanap + ksize;
			clpe->dispp = clp->dispp + ksize;
			clpe->conv = 0;
			clpe->ltop = small ? 0 : 1;

			/* $BKvHxJ8@a$r%]%$%s%H$5$;$k(B */
			clp += 2;
		}

		/* $BKvHxJ8@a$N>pJs$r@_Dj$9$k(B */
		clp->kanap = buf->kanaEnd;
		clp->dispp = buf->displayEnd;
		clp->conv = 0;
		clp->ltop = 1;

#ifdef DEBUG_WNNLIB
		showBuffers(buf, "after expandOrShrink [noconv]");
#endif
		return 0;
	}

	/* $B$9$Y$F$NJ8@a$,JQ49$5$l$F$$$k$3$H$rJ]>Z$9$k(B */
	makeConverted(buf, buf->nClause);

	/*
	 * $BJ8@a$ND9$5$rJQ99$9$k!#$3$N;~!"A0J8@a$K@\B32DG=$K$7$F$*$/$H(B
	 * $B:$$k$3$H$,$"$k!#Nc$($P!VL5NLBg?t!W$HF~NO$7$h$&$H$7$F!"(B
	 *   a) "$B$`$j$g$&$?$$$9$&(B" $B$rJQ49$9$k$H(B"$BL5NA(B $BBP?t(B" $B$H$J$k!#(B
	 *   b) "$BL5NA(B" $B$r(B "$BL5NL(B" $B$KD>$9!#(B
	 *   c) "$BBP?t(B" $B$r(B "$BBg?t(B" $B$KD>$=$&$H;W$C$?$,8uJd$K$J$$$N$G(B2$BJ8;zJ,(B
	 *      $BJ8@a$r=L$a$F(B "$BBg(B $B?t(B" $B$KJ,$1$h$&$H$9$k!#(B
	 *   d) $B$H$3$m$,(B "$B$?$$(B" $B$,A08uJd$K@\B3$7$F$7$^$$!"(B"$BL5NLBN(B $B?t(B" $B$K$J$k!#(B
	 *   e) "$BL5NLBg(B" $B$H$$$&8uJd$O$J$$$N$G!";EJ}$J$/(B2$BJ8;zJ8@a$r=L$a$k$H(B
	 *      "$BL5NA(B $BBP?t(B" $B$K$J$C$F$7$^$C$?!#(B
	 *   f) b) $B$KLa$k!#(B
	 * ($B$^!"$3$N>l9g$K$O$O$8$a$+$i!VL5NLBg?t!W$rEPO?$7$F$*$1$P$$$$$N$@$,(B)
	 */
       len += jl_yomi_len(buf->wnn, start, end);
#ifdef WNN6
	nsbun = jl_fi_nobi_conv(buf->wnn, start, len, -1, 0,
					small ? WNN_SHO : WNN_DAI);
#else
	nsbun = jl_nobi_conv(buf->wnn, start, len, -1, 0,
				small ? WNN_SHO : WNN_DAI);
#endif

	if (nsbun < 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}

	/* clauseInfo $B$N%5%$%:$N%A%'%C%/$7$F!"I,MW$J$i$PA}$d$9(B */
	if (nsbun > buf->clauseSize) {
		if (resizeCInfo(buf, nsbun) < 0)
			return -1;
	}
	buf->nClause = nsbun;

	/* $BI=<(%P%C%U%!$NBg$-$5$r%A%'%C%/$7$F!"I,MW$J$i$PA}$d$9(B */
	clp = buf->clauseInfo + start;
	len = clp->dispp - buf->displayBuf + jl_kanji_len(buf->wnn, start, -1);
	if (len > buf->bufferSize) {
		if (resizeBuffer(buf, len) < 0)
			return -1;
	}

	/* $B%+%l%s%HJ8@a$r3P$($F$*$/(B */
	buf->curClause = start;

	/* $BJQ497k2L$r!"I=<(%P%C%U%!$KF~$l$F$$$/(B */
	clp = buf->clauseInfo + start;
	kanap = clp->kanap;
	dispp = clp->dispp;
	while (start < nsbun) {
		end = start + 1;

		/* $BJ8@a>pJs$N@_Dj(B */
		clp->kanap = kanap;
		clp->dispp = dispp;

		/* $BI=<(%P%C%U%!$KJQ49J8;zNs$r%3%T!<(B
		 * jl_get_kanji $B$O:G8e$N(B NULL $B$^$G%3%T!<$5$l$k$N$GCm0U(B
		 */
		{
			int i = jl_kanji_len(buf->wnn, start, end);
			wchar c = dispp[i];

			(void)ki2_jl_get_kanji(buf->wnn, start, end, dispp, i);
			dispp[i] = c;	/* $B85$KLa$9(B */
			dispp += i;	/* $B0LCV$N99?7(B */
			clp->conv = 1;
			clp->ltop = jl_dai_top(buf->wnn, start);
		}

		/* $B$+$J%P%C%U%!$N0LCV$r99?7(B */
		kanap += jl_yomi_len(buf->wnn, start, end);

		/* $B<!$NJ8@a$X(B */
		start = end;
		clp++;
	}

	/* $B:G8e$N(B clauseInfo $B$N@_Dj(B */
	clp->kanap = buf->kanaEnd;
	clp->dispp = buf->displayEnd = dispp;
	clp->conv = 0;
	clp->ltop = 1;

	/* $B%+%l%s%HJ8@a$r:F@_Dj$9$k(B */
	setCurClause(buf, buf->curClause);

	/* $B%I%C%H$N:F@_Dj(B */
	DotSet(buf);

#ifdef DEBUG_WNNLIB
	showBuffers(buf, "after expand_or_shrink");
#endif
	return 0;
}

/* getCandidates -- $BA48uJd$r<h$j=P$9!#$?$@$7!"4{$K<h$j=P$7:Q$_$J$i2?$b$7$J$$(B */
static int
getCandidates(jcConvBuf *buf, int small)
{
	int start, end;

	TRACE("getCandidates", "Enter")

	/*
	 * $B4{$K8uJd$,<h$j=P$5$l$F$$$k>l9g!"%+%l%s%HJ8@a$H8uJdJ8@a$,0lCW(B
	 * $B$7$J$$$3$H$b$"$k$,!"8uJdJ8@a$N@_Dj$rM%@h$9$k$3$H$K$9$k!#$3$N(B
	 * $B>l9g!"8uJdJ8@a$O!"I,$:%+%l%s%HJ8@a$KEy$7$$$+$=$l$K4^$^$l$F$$(B
	 * $B$k$O$:!#(B
	 */
	if (small) {
		/* $B8uJd$,<h$j=P$7:Q$_$J$i!"2?$b$7$J$$(B */
		if (buf->candKind == CAND_SMALL &&
		    buf->candClause == buf->curClause)
			return 0;

		/* $B%+%l%s%H>.J8@a$N8uJd$r<h$j=P$9(B */
		start = buf->curClause;
		end = start + 1;
		if (jl_zenkouho(buf->wnn,
				 start,
				 getHint(buf, start, end) & WNN_USE_MAE,
				 WNN_UNIQ) < 0) {
			buf->candClause = -1;
			jcErrno = JE_WNNERROR;
			return -1;
		}
	} else {
		/* $B8uJd$,<h$j=P$7:Q$_$J$i!"2?$b$7$J$$(B */
#if 0
		if (buf->candKind == CAND_LARGE &&
		    buf->candClause >= buf->curLCStart &&
		    buf->candClauseEnd <= buf->curLCEnd)
			return 0;
#else
		if (buf->candKind == CAND_LARGE &&
		    buf->candClause >= buf->curLCStart &&
		    buf->candClauseEnd <= buf->curLCEnd &&
		    buf->candClause <= buf->curClause &&
		    buf->candClauseEnd > buf->curClause)
			return 0;
#endif

		/* $B%+%l%s%HBgJ8@a$N8uJd$r<h$j=P$9(B */
		start = buf->curLCStart;
		end = buf->curLCEnd;
#ifndef WNN6
		/*
		 * jl $B%i%$%V%i%j$N8uJd%P%C%U%!FbMF$rGK4~$9$k!#(B
		 * curLCStart $B$,0JA0$HF1$8$G$+$D(B curLCEnd $B$,(B
		 * $B0[$J$k>l9g(B ($B$D$^$j%+%l%s%HBgJ8@a$,8e$m$K?-$S$?>l9g(B)$B!"(B
		 * $B$3$&$7$J$$$H(B Wnn4 $B$N(B jl $B%i%$%V%i%j$O8uJd$r:F<hF@(B
		 * $B$7$F$/$l$J$$!#(B
		 */
		jl_kill(buf->wnn, 0, 0);
#endif
		if (jl_zenkouho_dai(buf->wnn,
					start,
					end,
					getHint(buf, start, end),
					WNN_UNIQ) < 0) {
			buf->candClause = -1;
			jcErrno = JE_WNNERROR;
			return -1;
		}
	}

	/* $B<!8uJd$N<h$j=P$7$N$?$a$N>pJs$r3P$($F$*$/(B */
	buf->candKind = small ? CAND_SMALL : CAND_LARGE;
	buf->candClause = start;
	buf->candClauseEnd = end;
	return 0;
}

/* setCandidate -- $B;XDj$5$l$?8uJd$G%P%C%U%!$rCV$-49$($k(B */
static int
setCandidate(jcConvBuf *buf, int n)
{
	int	start = buf->candClause;
	int 	end = buf->candClauseEnd;
	int	oldlen, newlen, bdiff;
	int	oldclen, newclen, cdiff;
	int	newend;
	jcClause	*clp;

	TRACE("setCandidate", "Enter")
#ifdef DEBUG_WNNLIB
	fprintf(stderr, "setCandidate for %d as %s\n",
		n, buf->candKind == CAND_SMALL ? "small" : "large");
	showBuffers(buf, "setCandiate (before)");
#endif

	clp = buf->clauseInfo + start;
	oldlen = (buf->clauseInfo + end)->dispp - clp->dispp;
	oldclen = jl_bun_suu(buf->wnn);

	if (buf->candKind == CAND_SMALL) {
		/* $B%+%l%s%H>.J8@a$r!";XDj8uJd$GCV$-49$($k(B */
		if (jl_set_jikouho(buf->wnn, n) < 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}
	} else {
		/* $B%+%l%s%HBgJ8@a$r!";XDj8uJd$GCV$-49$($k(B */
		if (jl_set_jikouho_dai(buf->wnn, n) < 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}
	}

	/* $BJQ498e$NJ8@a?t$N%A%'%C%/$9$k(B */
	newclen = jl_bun_suu(buf->wnn);
	if (newclen < 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}
	cdiff = newclen - oldclen;
	newend = end + cdiff;

	/* $BJQ498e$N%G%#%9%W%l%$%P%C%U%!$N%5%$%:$r%A%'%C%/$9$k(B */
	newlen = jl_kanji_len(buf->wnn, start, newend);
	if (newlen <= 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}
	bdiff = newlen - oldlen;

#ifdef DEBUG_WNNLIB
	{
		wchar	candstr[1024];

		fprintf(stderr, "Candidate[%s]: '",
			buf->candKind == CAND_SMALL ? "small" : "large");
		if (newlen >= 1024) {
			fprintf(stderr,
				 "* candidate string is too large [%d] *",
				 newlen);
		} else {
			candstr[0] = 0;
			ki2_jl_get_zenkouho_kanji(buf->wnn, n, candstr, sizeof(candstr));
			printBuffer (candstr, candstr + newlen);
		}
		fprintf(stderr, "'\n");
	}
#endif

	/*
	 * $B%G%#%9%W%l%$%P%C%U%!$r:F@_Dj$9$k(B
	 *
	 * $B:G=i$K!"=<J,$J%G%#%9%W%l%$%P%C%U%!$NBg$-$5$r3NJ]$7$F$*$/!#(B
	 * $B<!$K!"CV$-49$(8e$N8uJdJ8;zNs$N$?$a$N>l=j$r3NJ]$9$k!#:G8e$K!"(B
	 * $BCV$-49$($k8uJdJ8;zNs$r%G%#%9%W%l%$%P%C%U%!$KA^F~$9$k!#(B
	 */
	{
		int	buflen = (buf->displayEnd - buf->displayBuf) + bdiff;
		wchar	*dispp = clp->dispp;
		wchar	tmp;

		if (buflen > buf->bufferSize
		    && resizeBuffer(buf, buflen) < 0) {
			return -1;
		}

		moveDBuf(buf, end, bdiff);

		/*
		 * $B8uJdJ8;zNs$NA^F~$O!"(Bjl_get_kanji() $B$rMQ$$$k$N$G!"(B
		 * $B$=$l$,@_Dj$9$k:G8e$N(B NUL $BJ8;z$KCm0U!#(B
		 */
		tmp = dispp[newlen];
		if (ki2_jl_get_kanji(buf->wnn, start, newend,
				     dispp, newlen) <= 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}
		dispp[newlen] = tmp;
	}


	/*
	 * clauseInfo$B$r:F@_Dj$9$k(B
	 *
	 * $B:G=i$K!"=<J,$J(B clauseInfo $B$NBg$-$5$r3NJ]$7$F$*$/!#<!$K!"(B
	 * $B8uJdCV$-49$(8e$NJ8@a>pJs$N$?$a$N>l=j$r3NJ]$9$k!#:G8e$K!"(B
	 * $BCV$-49$($?8uJd$N%P%C%U%!>pJs$r@_Dj$9$k!#(B
	 */
	{
		wchar	*kanap, *dispp;
		int	i, j;

		if (buf->nClause + cdiff > buf->clauseSize
        	    && resizeCInfo(buf, buf->nClause + cdiff) < 0) {
			return -1;
		}

		moveCInfo(buf, end, cdiff);

		kanap = clp->kanap;
		dispp = clp->dispp;
		for (i = start; i < newend; i = j) {
			clp->kanap = kanap;
			clp->dispp = dispp;
			clp->conv = 1;
			clp->ltop = jl_dai_top(buf->wnn, i);
			j = i + 1;
			kanap += jl_yomi_len(buf->wnn, i, j);
			dispp += jl_kanji_len(buf->wnn, i, j);
			clp++;
		}

		/*
		 * $B8uJd$N<h$j=P$7$K$h$C$F!"A08e$NBgJ8@a$,JQ99$K$J$C$F(B
		 * $B$$$k$+$b$7$l$J$$$N$G!"$=$l$i$r:F@_Dj$9$k!#D>8e$NJ8(B
		 * $B@a$@$1$GNI$$$O$:$@$,!"G0$N$?$a!"$9$Y$F$r%A%'%C%/$9(B
		 * $B$k$3$H$K$9$k!#(B
		 */
		for (i = 0; i < start; i++)
			buf->clauseInfo[i].ltop = jl_dai_top(buf->wnn, i);
		for (i = newend; i < newclen; i++)
			buf->clauseInfo[i].ltop = jl_dai_top(buf->wnn, i);
	}

	/*
	 * $B<!8uJd$GCV$-49$($?7k2L!"CV$-49$(BP>]J8@a$H$=$NA08e$NBgJ8@a(B
	 * $B$,0\F0$7$F$$$k$+$b$7$l$J$$$N$G!"%+%l%s%HJ8@a$r:F@_Dj$9$k!#(B
	 */
	setCurClause(buf, start);

	/*
	 * $BJ8@a$N0\F0$KH<$$!"8uJdJ8@a$b0\F0$7$F$$$k$O$:$J$N$G!":F@_Dj(B
	 * $B$7$F$*$/(B (moveCInfo() $B;2>H(B)
	 */
	buf->candClause = start;
	buf->candClauseEnd = end + cdiff;

#ifdef DEBUG_WNNLIB
	showBuffers(buf, "setCandiate (after)");
#endif
	return 0;
}

/* checkCandidates -- $BA48uJd$,M-8z$+%A%'%C%/$7$F!"I,MW$J=hM}$r9T$J$&(B */
static void
checkCandidates(jcConvBuf *buf, int cls, int cle)
{
	/* $BJ8@aHV9f(B cls $B$+$i(B cle - 1 $B$^$G$NJ8@a$,JQ99$5$l$k(B
	 * $B<!8uJd%P%C%U%!$K$O$$$C$F$$$k8uJdJ8@a$,$3$NCf$K4^$^$l$F$$$l$P(B
	 * $B<!8uJd%P%C%U%!$NFbMF$rL58z$K$7$J$/$F$O$J$i$J$$(B
	 *
	 * $B$I$N$h$&$J>l9g$+$H$$$&$H!"(B
	 * 1. buf->candKind $B$,(B CAND_SMALL $B$G!"(B
	 *      cls <= buf->candClause < cle
	 * 2. buf->candKind $B$,(B CAND_LARGE $B$G!"(B
	 *      buf->candClause < cle $B$+$D(B cls < buf->candClauseEnd
         */
	if (buf->candKind == CAND_SMALL)
		buf->candClauseEnd = buf->candClause + 1; /* $BG0$N$?$a(B */
        if (buf->candClause < cle && cls < buf->candClauseEnd) {
		/* $BL58z$K$9$k(B */
		buf->candClause = buf->candClauseEnd = -1;
	}
}


/* forceStudy -- $BL$JQ49$*$h$S5?;wJQ49J8@a$N3X=,(B */
static int
forceStudy(jcConvBuf *buf, int n)
{
	int i, j, k;
	int status;
	wchar yomi[CL_BUFSZ], kanji[CL_BUFSZ];

	TRACE("forceStudy", "Enter")

#ifdef DEBUG_WNNLIB
	showBuffers(buf, "forceStudy");
#endif

	if (n < 0 || n > buf->nClause)
		n = buf->nClause;

	/* $B$3$3$G$$$&3X=,$H$O!"IQEY>pJs$N99?7$H9M$($F$h$$(B */

	/*
	 * Wnn6 $B$G$O!"L5JQ493X=,5!G=$,$"$j!"(Bwnnlib $B$K$h$k5?;wJQ49$dL$(B
	 * $BJQ49$NJ8@a$r3X=,$5$;$k$3$H$,$G$-$k!#$?$@$7!"L$JQ49$NJ8@a$K(B
	 * $BBP$7$FC1=c$KIQEY$N99?7$O$G$-$J$$$N$GCm0U(B
	 */

	/*
	 * $B:G=i$K!"JQ49:Q$_$NJ8@a?t$rD4$Y$k!#F~NO$5$l$?J8@a$,$9$Y$FJQ(B
	 * $B49:Q$_(B (conv == 1) $B$G$"$l$P!"A4J8@a$r$NJ8@a$NIQEY>pJs$r$^$H(B
	 * $B$a$F99?7$9$k!#JQ49:Q$_$G$J$$J8@a$,$"$C$?>l9g!"$H$j$"$($:JQ(B
	 * $B49$7$F!"JQ497k2L$,I=<(%P%C%U%!$NFbMF$H0lCW$7$F$$$l$P!"IQEY(B
	 * $B>pJs$r99?7$9$k$3$H$K$9$k(B
	 */
	status = 0;
	for (i = 0; i < n; i++) {
		if (buf->clauseInfo[i].conv == 1)
			status++;
	}

	/* $B$9$Y$F$NJ8@a$,JQ49$5$l$F$$$?$i!"A4$F$NJ8@a$NIQEY$r99?7$9$k(B */
	if (status == n) {
#ifdef WNN6
		status = jl_optimize_fi(buf->wnn, 0, -1);
#else
		status = jl_update_hindo(buf->wnn, 0, -1);
#endif
		if (status < 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}
		return 0;
	}

	/*
	 * $BJ8@aC10L$GIQEY>pJs$r99?7$9$k(B
	 * $BL$JQ49$NJ8@a$,$"$l$P!"L$JQ49$H$7$FIQEY>pJs$r99?7$9$k(B (Wnn6
	 * $B$NL5JQ493X=,5!G=(B)
	 */

	/* $BIQEY>pJs$r99?7$9$kA0$K!"A4J8@a$rJQ49$7$F$*$/(B */
	if (makeConverted(buf, n) < 0)
		return -1;

	for (i = 0; i < n; i = j) {
		j = i + 1;
		/*
		 * $BJQ49:Q$_$NJ8@a$G$"$l$P!"$=$N$^$^IQEY>pJs$r99?7$9$k(B
		 */
		if (buf->clauseInfo[i].conv == 1) {
#ifdef WNN6
			status = jl_optimize_fi(buf->wnn, i, j);
#else
			status = jl_update_hindo(buf->wnn, i, j);
#endif
			if (status < 0) {
				jcErrno = JE_WNNERROR;
				return -1;
			}
			continue;
		}

		/*
		 * $BL$JQ49$H5?;wJQ49$NJ8@a$KBP$7$F$O!"FI$_$r3X=,$9$k(B
		 * $BL$JQ49$H5?;wJQ49$N>l9g$G$b!"(Bwnnlib $B$G$OI=<(%P%C%U%!$H$+(B
		 * $B$J%P%C%U%!$NN>J}$,0lCW$7$F$$$k$N$G(B (jcKana() $B;2>H(B)$B!"(B
		 * $B$3$3$G$O(B jllib $B$NFI$_%G!<%?$r;HMQ$9$k(B
		 */

		/* $BFI$_J8;zNs$HJQ49:QJ8;zNs$ND9$5%A%'%C%/(B */
		if (jl_yomi_len(buf->wnn, i, j) >= CL_BUFSZ ||
		    jl_kanji_len(buf->wnn, i, j) >= CL_BUFSZ) {
			/* $B%P%C%U%!%*!<%P%U%m!<$rHr$1$k(B */
			continue;
		}

		/* $BFI$_J8;zNs$N<h$j=P$7(B */
		if (ki2_jl_get_yomi(buf->wnn, i, j, yomi, CL_BUFSZ) < 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}
		/* $BJQ49:Q$_J8;zNs$r<h$j=P$9(B */
		if (ki2_jl_get_kanji(buf->wnn, i, j, kanji, CL_BUFSZ) < 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}

		/*
		 * $BFI$_$HJQ498e$,0lCW$7$F$$$l$P!"3X=,:Q$_$H$_$J$7$F!"(B
		 * $B$=$N$^$^IQEY>pJs$r99?7$9$k(B
		 */
		if (wstrcmp (yomi, kanji) == 0) {
#ifdef WNN6
			status = jl_optimize_fi(buf->wnn, i, j);
#else
			status = jl_update_hindo(buf->wnn, i, j);
#endif
			if (status < 0) {
				jcErrno = JE_WNNERROR;
				return -1;
			}
			continue;
		}

		/*
		 * $BFI$_$HJQ498e$,0lCW$7$J$$$N$G!"A48uJd$NCf$+$iC5$9(B
		 * $B$b$7!"0lCW$9$k$b$N$,$"$l$P!"IQEY>pJs$r99?7$7!"$=$&(B
		 * $B$G$J$1$l$PIQEY>pJs$O99?7$7$J$$(B
		 */
		if (jl_zenkouho(buf->wnn, i,
			 	getHint(buf, -1, -1), WNN_UNIQ) < 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}
		status = jl_zenkouho_suu(buf->wnn);
		if (status < 0) {
			jcErrno = JE_WNNERROR;
			return -1;
		}
		for (k = 0; k < status; k++) {
			ki2_jl_get_zenkouho_kanji(buf->wnn, k, kanji,
						  CL_BUFSZ);
			/* $BI,$:(B NUL $B%?!<%_%M!<%H$5$l$k$h$&$K$7$F$*$/(B */
			kanji[CL_BUFSZ - 1] = 0;
			if (wstrcmp(yomi, kanji) != 0)
				continue;
			if (jl_set_jikouho(buf->wnn, k) < 0) {
				jcErrno = JE_WNNERROR;
				return -1;
			}
#ifdef WNN6
			status = jl_optimize_fi(buf->wnn, i, j);
#else
			status = jl_update_hindo(buf->wnn, i, j);
#endif
			if (status < 0) {
				jcErrno = JE_WNNERROR;
				return -1;
			}
			break;
		}
	}

	return 0;
}


/*
 *	$B$3$3$+$i(B Public $B$J%U%!%s%/%7%g%s(B
 */

/* jcCreateBuf -- $BJQ49%P%C%U%!$N:n@.(B */
jcConvBuf *
jcCreateBuffer(struct wnn_buf *wnn, int nclause, int buffersize)
{
	jcConvBuf	*buf;

	TRACE("jcCreateBuffer", "Enter")

	/* $B$^$:(B jcConvBuf $B$N3NJ](B */
	if ((buf = (jcConvBuf *)malloc(sizeof(jcConvBuf))) == NULL) {
		jcErrno = JE_NOCORE;
		return NULL;
	}
	(void)bzero((char *)buf, sizeof(jcConvBuf));
	buf->wnn = wnn;

	/* $B<!$K3F<o%P%C%U%!$N3NJ](B */

	/* $B$^$:!"$+$J%P%C%U%!$HI=<(%P%C%U%!(B */
	buf->bufferSize = (buffersize <= 0) ? DEF_BUFFERSIZE : buffersize;
	/* $B%P%C%U%!$N:G8e$r(B NULL $B%?!<%_%M!<%H$9$k$3$H$,$"$k$N$G!"(B
	 * 1$BJ8;zJ8Bg$-$/$7$F$*$/(B
	 */
	buf->kanaBuf = (wchar *)malloc((buf->bufferSize + 1) *
					 sizeof(wchar));
	buf->displayBuf = (wchar *)malloc((buf->bufferSize + 1) *
					    sizeof(wchar));

	/* $B<!$K(B clauseInfo $B%P%C%U%!(B */
	buf->clauseSize = (nclause <= 0) ? DEF_CLAUSESIZE : nclause;
	/* clauseInfo $B%P%C%U%!$O(B nclause + 1 $B8D%"%m%1!<%H$9$k(B
	 * $B$J$<$+$H$$$&$H(B clauseinfo $B$O%G%j%_%?$H$7$FMWAG$r(B
	 * 1$B8D;H$&$N$G(B nclause $B8D$NJ8@a$r07$&$?$a$K$O(B nclause + 1 $B8D$N(B
	 * $BBg$-$5$r;}$?$J$1$l$P$J$i$J$$$+$i$G$"$k(B
	 */
	buf->clauseInfo = (jcClause *)malloc((buf->clauseSize + 1)
					     * sizeof(jcClause));

	if (buf->kanaBuf == NULL || buf->displayBuf == NULL ||
	    buf->clauseInfo == NULL) {
		/* malloc() $B$G$-$J$+$C$?(B */
		Free(buf->kanaBuf);
		Free(buf->displayBuf);
		Free(buf->clauseInfo);
		Free(buf);
		jcErrno = JE_NOCORE;
		return NULL;
	}

	(void)jcClear(buf);
	return buf;
}

/* jcDestroyBuffer -- $BJQ49%P%C%U%!$N>C5n(B */
int
jcDestroyBuffer(jcConvBuf *buf, int savedic)
{
	TRACE("jcDestroyBuffer", "Enter")

	if (buf == NULL)
		return 0;

	/* $B%"%m%1!<%H$7$?%a%b%j$N2rJ|(B */
	Free(buf->kanaBuf);
	Free(buf->displayBuf);
	Free(buf->clauseInfo);

	/* savedic $B$,(B 0 $B$G$J$1$l$P!"4D6-$K%m!<%I$5$l$F$$$kA4$F$N%U%!%$%k$r(B
	 * save $B$9$k(B
	 */
	if (savedic && jl_dic_save_all(buf->wnn) < 0) {
		jcErrno = JE_WNNERROR;
		return -1;
	}

	Free(buf);

	return 0;
}

/* jcClear -- wnnlib $B$N=i4|2=(B ($B?7$?$JJQ49$r;O$a$kKh$K8F$P$J$1$l$P$J$i$J$$(B) */
int
jcClear(jcConvBuf *buf)
{
	TRACE("jcClear", "Enter")

	/* $B=i4|CM$N@_Dj(B */
	buf->nClause = buf->curClause = buf->curLCStart = 0;
	buf->curLCEnd = 1;
	buf->candClause = buf->candClauseEnd = -1;
	buf->kanaEnd = buf->kanaBuf;
	buf->displayEnd = buf->displayBuf;
	buf->clauseInfo[0].kanap = buf->kanaBuf;
	buf->clauseInfo[0].dispp = buf->displayBuf;
	buf->clauseInfo[0].conv = 0;
	buf->clauseInfo[0].ltop = 1;
	buf->dot = buf->kanaBuf;
	buf->fixed = 0;
	jcErrno = JE_NOERROR;

	if (jl_bun_suu(buf->wnn) > 0)
		jl_kill(buf->wnn, 0, -1);

	return 0;
}

/* jcConvert -- $B%+%l%s%HJ8@a0J9_$r$+$J4A;zJQ49$9$k(B */
int
jcConvert(jcConvBuf *buf, int small, int tan, int jump)
{
	int	ret;

	TRACE("jcConvert", "Enter")

	CHECKFIXED(buf);

	if (buf->curClause == buf->nClause) {
		/* $B%+%l%s%HJ8@a$,:G8e$NJ8@a$G$7$+$b6u(B */
		jcErrno = JE_CLAUSEEMPTY;
		return -1;
	}

	/* $BA48uJdJ8@a$,%+%l%s%HBgJ8@a$+$=$l0J9_$K$"$l$PL58z$K$9$k(B */
	checkCandidates(buf,
			small ? buf->curClause : buf->curLCStart,
			buf->nClause);

	if (tan) {
		ret = tanConvert(buf, small);
	} else {
		ret = renConvert(buf, small);
	}

	if (ret < 0)
		return ret;

	if (jump) {
		/* $B%I%C%H$H%+%l%s%HJ8@a$rJ8$N:G8e$K0\F0$5$;$k(B */
		buf->curClause = buf->curLCStart = buf->nClause;
		buf->curLCEnd = buf->nClause + 1;
		buf->dot = buf->kanaEnd;
	}
	return 0;
}

/* jcUnconvert -- $B%+%l%s%HBgJ8@a$rL5JQ49$N>uBV$KLa$9(B */
int
jcUnconvert(jcConvBuf *buf)
{
	jcClause	*clp = buf->clauseInfo + buf->curClause;

	TRACE("jcUnconvert", "Enter")

	CHECKFIXED(buf);

	if (buf->curClause == buf->nClause) {
		/* $B%+%l%s%HJ8@a$,:G8e$NJ8@a$G$7$+$b6u(B */
		jcErrno = JE_CLAUSEEMPTY;
		return -1;
	}

	if (!clp->conv) {
		/* $B%+%l%s%HJ8@a$OJQ49$5$l$F$$$J$$(B */
		/* $BL5JQ49$NJ8@a$O(B wnnlib $BFbIt$G$O>o$KBgJ8@a$H$7$F(B
		 * $B07$o$l$k$N$G!"%+%l%s%H>.J8@a$NJQ49>uBV$r8+$F!"(B
		 * $B$=$l$,JQ49>uBV$J$i%+%l%s%HBgJ8@aFb$N(B
		 * $BA4$F$N>.J8@a$OJQ49>uBV!"$=$&$G$J$1$l$PL5JQ49>uBV!"(B
		 * $B$K$J$k(B
		 */
		jcErrno = JE_NOTCONVERTED;
		return -1;
	}

	/* $BA48uJdJ8@a$,%+%l%s%HBgJ8@a$+$=$l0J9_$K$"$l$PL58z$K$9$k(B */
	checkCandidates(buf, buf->curLCStart, buf->nClause);

	/* $BL5JQ49>uBV$K$9$k(B */
	if (unconvert(buf, buf->curLCStart, buf->curLCEnd) < 0)
		return -1;

	/* $BBgJ8@a$N@_Dj(B */
	clp = buf->clauseInfo + buf->curLCStart;
	clp->ltop = 1;
	(++clp)->ltop = 1;

	/* $B%+%l%s%HJ8@a$N:F@_Dj(B */
	buf->curClause = buf->curLCStart;
	buf->curLCEnd = buf->curLCStart + 1;

	/* $B%I%C%H$N@_Dj(B */
	DotSet(buf);

	return 0;
}

/* jcCancel -- $BF~NOCf$NA4J8@a$rL5JQ49>uBV$K$9$k(B */
int
jcCancel(jcConvBuf *buf)
{
	TRACE("jcCancel", "Enter")

	CHECKFIXED(buf);

	if (buf->nClause <= 0)
		return 0;

	/*
	 * $BI=<(%P%C%U%!$NFbMF$r$+$J%P%C%U%!$NFbMF$GCV49$($k(B
	 * $B$3$N:]!"%P%C%U%!$NBg$-$5$O5$$K$9$kI,MW$,L5$$!#$J$<$J$i!"I=(B
	 * $B<(%P%C%U%!$H$+$J%P%C%U%!$NBg$-$5$O>o$KF1$8$@$+$i(B
	 */
	bcopy(buf->kanaBuf, buf->displayBuf, buf->bufferSize * sizeof (wchar));

	/*
	 * $B:#$"$kA4J8@a$r0l$D$NL5JQ49>uBV$NBgJ8@a$K$9$k(B
	 * $B$3$N:]$b!"J8@a?t$r5$$K$9$kI,MW$O$J$$!#$J$<$J$i!">/$/$H$b0l$D(B
	 * $B$NJ8@a$O$"$C$?$O$:$@$+$i(B
	 */
	buf->curClause = buf->curLCStart = 0;
	buf->nClause = buf->curLCEnd = 1;
	buf->displayEnd = buf->displayBuf + (buf->kanaEnd - buf->kanaBuf);
	buf->clauseInfo[0].conv = 0;
	buf->clauseInfo[0].ltop = 1;
	buf->clauseInfo[1].kanap = buf->kanaEnd;
	buf->clauseInfo[1].dispp = buf->displayEnd;
	buf->clauseInfo[1].conv = 0;
	buf->clauseInfo[1].ltop = 1;

	/* $BA48uJdJ8@a$bL58z$K$9$k(B */
	buf->candClause = buf->candClauseEnd = -1;

	/* jllib $B$NJQ49>uBV$bL58z$K$9$k(B */
	if (jl_bun_suu(buf->wnn) > 0)
		jl_kill(buf->wnn, 0, -1);

	return 0;
}

/* jcExpand -- $B%+%l%s%HJ8@a$r#1J8;z9-$2$k(B */
int
jcExpand(jcConvBuf *buf, int small, int convf)
{
	TRACE("jcExpand", "Enter")

	CHECKFIXED(buf);

	return expandOrShrink(buf, small, 1, convf);
}

/* jcShrink -- $B%+%l%s%HJ8@a$r#1J8;z=L$a$k(B */
int
jcShrink(jcConvBuf *buf, int small, int convf)
{
	TRACE("jcShrink", "Enter")

	CHECKFIXED(buf);

	return expandOrShrink(buf, small, 0, convf);
}

/* jcKana -- $B%+%l%s%HJ8@a$r$+$J$K$9$k(B */
int
jcKana(jcConvBuf *buf, int small, int kind)
{
	jcClause	*clp;
	wchar		*kanap, *kanaendp, *dispp;
	int		start, end;
	int		conv;
	int		c;

	TRACE("jcKana", "Enter")

	CHECKFIXED(buf);

	/* $BJ8@aHV9f$N%A%'%C%/(B */
	if (buf->curClause >= buf->nClause) {
		/* $B%+%l%s%HJ8@a$,:G8e$NJ8@a$G$7$+$b6u$@$C$?>l9g(B
		 * $B$3$N>l9g%(%i!<$K$7$F$b$h$$$,(B...
		 */
		return 0;
	}

	/*
	 * $B%+%l%s%HJ8@a$,JQ49$5$l$F$$$l$P$$$C$?$sL5JQ49$K$9$k(B
	 */

	/* $B$"$H$GJQ49>uBV$r$b$H$KLa$9$?$a!"JQ49>uBV$r%;!<%V$7$F$*$/(B */
	conv = buf->clauseInfo[buf->curClause].conv;

	if (small) {
		start = buf->curClause;
		end = start + 1;
	} else {
		start = buf->curLCStart;
		end = buf->curLCEnd;
	}

	/* $BA48uJdJ8@a$N%A%'%C%/(B */
	checkCandidates(buf, start, end);

	/* $BL5JQ49>uBV$K$9$k(B */
	if (unconvert(buf, start, end) < 0) {
		return -1;
	}

	/*
	 * small $B$,(B 0$B!"$D$^$j%+%l%s%HJ8@a$H$7$FBgJ8@a$rA*Br$7$?>l9g!"(B
	 * $B$=$NCf$N>.J8@a$O0l$D$K$^$H$a$i$l$k$N$G!"(BcurClause $B$H(B
	 * curLCEnd $B$rJQ$($kI,MW$,$"$k(B
	 */
	if (!small) {
		buf->curClause = buf->curLCStart;
		buf->curLCEnd = buf->curLCStart + 1;
	}

	/*
	 * $B$+$JJQ49$9$k(B
	 *
	 * $BI=<(%P%C%U%!$@$1$G$O$J$/!"$+$J%P%C%U%!$bJQ49$9$k(B
	 *
	 * $B$3$l$K$O$5$7$?$kM}M3$O$J$$$,!"$^$"!"(BVer3 $BHG$N(B jclib $B$,(B
	 * $B$=$&$@$C$?$N$G!D(B
	 */
	clp = buf->clauseInfo + buf->curClause;
	kanap = clp->kanap;
	kanaendp = (clp + 1)->kanap;
	dispp = clp->dispp;

	if (kind == JC_HIRAGANA) {	/* $B%+%?%+%J"*$R$i$,$J(B */
		/* $B%+%?%+%J$r$R$i$,$J$KJQ49$9$k:]$K$O$R$i$,$J$K$J$$;z(B
		 * "$B%t%u%v(B" $B$,$"$k$N$G$$$-$*$$$GJQ49$7$F$7$^$o$J$$$h$&$K(B
		 * $B5$$rIU$1$J$1$l$P$J$i$J$$(B
		 * ($B$^$"<B:]$O5$$r$D$1$k$H$$$&$[$I$N$b$N$G$O$J$$$,(B)
		 */
		while (kanap < kanaendp) {
			c = *kanap;
			if ((KANABEG + KATAOFFSET) <= c &&
					c <= (KANAEND + KATAOFFSET)) {
				*kanap = *dispp = c - KATAOFFSET;
			}
			kanap++, dispp++;
		}
	} else {	/* $B$R$i$,$J"*%+%?%+%J(B */
		while (kanap < kanaendp) {
			c = *kanap;
			if (KANABEG <= c && c <= KANAEND) {
				*kanap = *dispp = c + KATAOFFSET;
			}
			kanap++, dispp++;
		}
	}

	/*
	 * $BJQ49>uBV$r$b$H$KLa$7$F$*$/(B
	 */

	/* $B$H$O$$$C$F$b4{$KJQ49$5$l$?J8@a$N>l9g!"$3$l$NIQEY>pJs$r(B
	 * $B%5!<%P$KAw$k$H$^$:$$$N$G!"$"$H$G$+$JJQ49$7$?$3$H$,$o$+$k$h$&$K(B
	 * jcClause.conv $B$O(B -1 $B$K%;%C%H$9$k(B
	 */
	clp->conv = conv ? -1 : 0;

	return 0;
}

/* jcFix -- $B3NDj$9$k(B */
int
jcFix(jcConvBuf *buf)
{
	TRACE("jcFix", "Enter")

	if (buf->fixed) {
		/* $B4{$K3NDj$5$l$F$$$k(B
		 * $B%(%i!<$K$7$F$b$h$$$,!D(B
		 */
		return 0;
	}

	if (forceStudy(buf, buf->nClause) < 0)
		return -1;

	/* $B3NDj%U%i%0$rN)$F$k(B */
	buf->fixed = 1;

	return 0;
}

/* jcFix1 -- $B:G=i$N0lJ8;z$@$1$r3NDj$9$k(B */
int
jcFix1(jcConvBuf *buf)
{
	TRACE("jcFix1", "Enter")

	if (buf->fixed) {
		/* $B4{$K3NDj$5$l$F$$$k(B
		 * $B%(%i!<$K$7$F$b$h$$$,!D(B
		 */
		return 0;
	}

	if (buf->nClause >= 1) {
		/* $B:G=i$NJ8@a$@$1$r3X=,$9$k(B */
		if (forceStudy(buf, 1) < 0)
			return -1;

		/* $B:G=i$NJ8@a$N0lJ8;z$@$1$K$9$k(B */
		buf->nClause = 1;
		buf->curClause = buf->curLCStart = 0;
		buf->curLCEnd = 1;
		buf->kanaEnd = buf->kanaBuf + 1; /* $B%@%_!<(B */
		buf->displayEnd = buf->displayBuf + 1;
		buf->clauseInfo[0].kanap = buf->kanaBuf;
		buf->clauseInfo[0].dispp = buf->displayBuf;
		buf->clauseInfo[0].ltop = 1;
		buf->clauseInfo[1].kanap = buf->kanaBuf + 1;  /* $B%@%_!<(B */
		buf->clauseInfo[1].dispp = buf->displayBuf + 1;
		buf->clauseInfo[1].ltop = 1;
		buf->dot = buf->kanaBuf + 1;
		buf->candClause = buf->candClauseEnd = -1;
	}


	/* $B3NDj%U%i%0$rN)$F$k(B */
	buf->fixed = 1;

	return 0;
}

/* jcNext -- $B%+%l%s%HJ8@a$r<!8uJd(B/$BA08uJd$GCV$-49$($k(B */
int
jcNext(jcConvBuf *buf, int small, int prev)
{
	int	n;

	TRACE("jcNext", "Enter")

	CHECKFIXED(buf);

	if (!buf->clauseInfo[buf->curClause].conv) {
		/* $B$^$@JQ49$5$l$F$$$J$$(B */
		jcErrno = JE_NOTCONVERTED;
		return -1;
	}

	/* $BA48uJd$,F@$i$l$F$$$J$1$l$P!"A48uJd$rF@$k(B */
	if (getCandidates(buf, small) < 0)
		return -1;

	n = jl_zenkouho_suu(buf->wnn);
	if (n <= 1) {
		/* $B<!8uJd$,$J$$(B */
		jcErrno = n < 0 ? JE_WNNERROR : JE_NOCANDIDATE;
		return -1;
	}

	/* $B<!8uJdHV9f$rF@$k(B */
	n = jl_c_zenkouho(buf->wnn) + (prev ? -1 : 1);
	if (n < 0) {
		n = jl_zenkouho_suu(buf->wnn) - 1;
	} else if (n >= jl_zenkouho_suu(buf->wnn)) {
		n = 0;
	}

	if (setCandidate(buf, n) < 0) {
		/* $B<!8uJd$,F@$i$l$J$+$C$?(B */
		jcErrno = JE_WNNERROR;
		return -1;
	}

	return 0;
}

/* jcCandidateInfo -- $B<!8uJd$N?t$H8=:_$N8uJdHV9f$rD4$Y$k(B
 *		      $B$b$7<!8uJd$,$^$@%P%C%U%!$KF~$C$F$$$J$1$l$PMQ0U$9$k(B
 */
int
jcCandidateInfo(jcConvBuf *buf, int small, int *ncandp, int *curcandp)
{
	int 	cand, ncand;

	TRACE("jcCandidateInfo", "Enter")

	CHECKFIXED(buf);

	if (!buf->clauseInfo[buf->curClause].conv) {
		/* $B$^$@JQ49$5$l$F$$$J$$(B */
		jcErrno = JE_NOTCONVERTED;
		return -1;
	}

	/* $BA48uJd$,F@$i$l$F$$$J$1$l$P!"A48uJd$rF@$k(B */
	if (getCandidates(buf, small) < 0)
		return -1;

	ncand = jl_zenkouho_suu(buf->wnn);
	if (ncand <= 1) {
		/* $B8uJd$,$J$$(B */
		jcErrno = (ncand < 0) ? JE_WNNERROR : JE_NOCANDIDATE;
		return -1;
	}

	/* $B8=:_$N8uJdHV9f$rF@$k(B */
	cand = jl_c_zenkouho(buf->wnn);
	if (cand < 0) {
		/* $B8uJd$,F@$i$l$J$$(B */
		jcErrno = JE_WNNERROR;
		return -1;
	}

	if (ncandp != NULL) *ncandp = ncand;
	if (curcandp != NULL) *curcandp = cand;

	return 0;
}

/* jcGetCandidate -- $B;XDj$5$l$?HV9f$N8uJd$r<h$j=P$9(B */
int
jcGetCandidate(jcConvBuf *buf, int n, wchar *candstr, int len)
{
	wchar	tmp[CL_BUFSZ];

	TRACE("jcGetCandidate", "Enter")

	CHECKFIXED(buf);

	/* $BJ8@a$N%A%'%C%/(B */
	if (buf->candClause < 0) {
		jcErrno = JE_NOCANDIDATE;
		return -1;
	}

	/* $B8uJdHV9f$N%A%'%C%/(B */
	if (n < 0 || n >= jl_zenkouho_suu(buf->wnn)) {
		jcErrno = JE_NOSUCHCANDIDATE;
		return -1;
	}

	/* $BJ8;zNs$r%3%T!<(B */
	ki2_jl_get_zenkouho_kanji(buf->wnn, n, tmp, CL_BUFSZ);
	tmp[CL_BUFSZ - 1] = 0;
	wstrncpy(candstr, tmp, len / sizeof(wchar));

	return 0;
}

/* jcSelect -- $BI=<(%P%C%U%!$r;XDj$5$l$?8uJd$HCV$-49$($k(B */
int
jcSelect(jcConvBuf *buf, int n)
{
	TRACE("jcSelect", "Enter")

	CHECKFIXED(buf);

#ifdef DEBUG_WNNLIB
	fprintf(stderr,
		"Select: %d [%s for %d - %d]\n",
		n,
		buf->candKind == CAND_SMALL ? "small" : "large",
		buf->candClause,
		buf->candClauseEnd);
#endif

	/* $BJ8@a$N%A%'%C%/(B */
	if (buf->candClause < 0) {
		jcErrno = JE_NOCANDIDATE;
		return -1;
	}

	/* $B8uJdHV9f$N%A%'%C%/(B */
	if (n < 0 || n >= jl_zenkouho_suu(buf->wnn)) {
		jcErrno = JE_NOSUCHCANDIDATE;
		return -1;
	}

	/* $B8uJd$,%;%C%H$5$l$F$$$J$1$l$P!"%;%C%H$9$k(B */
	if (jl_c_zenkouho(buf->wnn) != n && setCandidate(buf, n) < 0)
		return -1;

	return 0;
}

/* jcDotOffset -- $BBgJ8@a$N@hF,$+$i$N%I%C%H$N%*%U%;%C%H$rJV$9(B */
int
jcDotOffset(jcConvBuf *buf)
{
	TRACE("jcDotOffset", "Enter")

	return buf->dot - buf->clauseInfo[buf->curLCStart].kanap;
}

/* jcIsConverted -- $B;XDj$5$l$?J8@a$,JQ49$5$l$F$$$k$+$I$&$+$rJV$9(B */
int
jcIsConverted(jcConvBuf *buf, int cl)
{
	TRACE("jcIsConverted", "Enter")

	if (cl < 0 || cl > buf->nClause) {
		/* cl == jcNClause $B$N$H$-$r%(%i!<$K$7$F$b$$$$$N$@$1$l$I(B
		 * $B%+%l%s%HJ8@a$,(B jcNClause $B$N$H$-$,$"$k$N$G(B
		 * $B%(%i!<$H$O$7$J$$$3$H$K$7$?(B
		 */
		return -1;
	}
	return (buf->clauseInfo[cl].conv != 0);
}

/* jcMove -- $B%I%C%H!&%+%l%s%HJ8@a$r0\F0$9$k(B */
int
jcMove(jcConvBuf *buf, int small, int dir)
{
	jcClause	*clp = buf->clauseInfo + buf->curClause;
	int		i;

	TRACE("jcMove", "Enter")

	if (!clp->conv) {
		/* $B%+%l%s%HJ8@a$,JQ49$5$l$F$$$J$$$N$G!"%I%C%H$N0\F0$K$J$k(B */
		if (dir == JC_FORWARD) {
			if (buf->curClause == buf->nClause) {
				/* $B$9$G$K0lHV:G8e$K$$$k(B */
				jcErrno = JE_CANTMOVE;
				return -1;
			} else if (buf->dot == (clp + 1)->kanap) {
				/* $B%I%C%H$,%+%l%s%HJ8@a$N:G8e$K$"$k$N$G(B
				 * $BJ8@a0\F0$9$k(B
				 */
				goto clausemove;
			} else {
				buf->dot++;
			}
		} else {
			if (buf->dot == clp->kanap) {
				/* $B%I%C%H$,%+%l%s%HJ8@a$N@hF,$K$"$k$N$G(B
				 * $BJ8@a0\F0$9$k(B
				 */
				goto clausemove;
			} else
				buf->dot--;
		}
		return 0;
	}

clausemove:	/* $BJ8@a0\F0(B */
	clp = buf->clauseInfo;

	if (small) {
		/* $B>.J8@aC10L$N0\F0(B */
		if (dir == JC_FORWARD) {
			if (buf->curClause == buf->nClause) {
				jcErrno = JE_CANTMOVE;
				return -1;
			}
			buf->curClause++;
			if (buf->curClause >= buf->curLCEnd) {
				/* $BBgJ8@a$b0\F0$9$k(B */
				buf->curLCStart = buf->curLCEnd;
				for (i = buf->curLCStart + 1;
				     i <= buf->nClause && !clp[i].ltop; i++)
					;
				buf->curLCEnd = i;
			}
		} else {	/* JC_BACKWARD */
			if (buf->curClause == 0) {
				jcErrno = JE_CANTMOVE;
				return -1;
			}
			buf->curClause--;
			if (buf->curClause < buf->curLCStart) {
				/* $BBgJ8@a$b0\F0$9$k(B */
				buf->curLCEnd = buf->curLCStart;
				for (i = buf->curClause; !clp[i].ltop; i--)
					;
				buf->curLCStart = i;
			}
		}
	} else {
		/* $BBgJ8@aC10L$N0\F0(B */
		if (dir == JC_FORWARD) {
			if (buf->curLCStart == buf->nClause) {
				jcErrno = JE_CANTMOVE;
				return -1;
			}
			buf->curLCStart = buf->curClause = buf->curLCEnd;
			for (i = buf->curLCStart + 1;
			     i <= buf->nClause && !clp[i].ltop; i++)
				;
			buf->curLCEnd = i;
		} else {
			if (buf->curLCStart == 0) {
				jcErrno = JE_CANTMOVE;
				return -1;
			}
			buf->curLCEnd = buf->curLCStart;
			for (i = buf->curLCEnd - 1; !clp[i].ltop; i--)
				;
			buf->curLCStart = buf->curClause = i;
		}
	}

	/* $BJ8@a0\F0$7$?$i%I%C%H$O$=$NJ8@a$N@hF,$K0\F0$9$k(B */
	buf->dot = clp[buf->curClause].kanap;

	return 0;
}

/* jcTop -- $B%I%C%H!&%+%l%s%HJ8@a$rJ8$N@hF,$K0\F0$9$k(B */
int
jcTop(jcConvBuf *buf)
{
	TRACE("jcTop", "Enter")

	/* $B%+%l%s%HJ8@a$r(B 0 $B$K$7$F%I%C%H$r@hF,$K;}$C$F$/$k(B */
	setCurClause(buf, 0);
	buf->dot = buf->kanaBuf;

	return 0;
}

/* jcBottom -- $B%I%C%H!&%+%l%s%HJ8@a$rJ8$N:G8e$K0\F0$9$k(B */
int
jcBottom(jcConvBuf *buf)
{
	TRACE("jcBottom", "Enter")

	/*
	 * Ver3 $BBP1~$N(B jclib $B$G$O!"%+%l%s%HJ8@a$r(B jcNClause $B$K$7$F(B
	 * $B%I%C%H$r:G8e$K;}$C$F$/$k$@$1$@$C$?(B
	 * $B$3$l$@$H!":G8e$NJ8@a$K$+$J$rF~$l$F$$$F!"%+!<%=%k$rF0$+$7$F(B
	 * jcBottom() $B$G85$KLa$C$F:F$S$+$J$rF~$l$k$H!"JL$NJ8@a$K(B
	 * $B$J$C$F$7$^$&(B
	 * $B$=$3$G!":G8e$NJ8@a$,L5JQ49>uBV$N;~$K$O!"%+%l%s%HJ8@a$O(B
	 * buf->nClause $B$G$O$J$/!"(Bbuf->nClause - 1 $B$K$9$k$3$H$K$9$k(B
	 */
	if (buf->nClause > 0 && !buf->clauseInfo[buf->nClause - 1].conv) {
		buf->curClause = buf->curLCStart = buf->nClause - 1;
		buf->curLCEnd = buf->nClause;
	} else {
		buf->curClause = buf->curLCStart = buf->nClause;
		buf->curLCEnd = buf->nClause + 1;
	}
	buf->dot = buf->kanaEnd;
	return 0;
}

/* jcInsertChar -- $B%I%C%H$N0LCV$K0lJ8;zA^F~$9$k(B */
int
jcInsertChar(jcConvBuf *buf, int c)
{
	jcClause	*clp;
	wchar	*dot, *dispdot;
	int	ksizenew, dsizenew;

	TRACE("jcInsertChar", "Enter")

	CHECKFIXED(buf);

	/* $BA48uJdJ8@a$,%+%l%s%HBgJ8@a$K$"$l$PL58z$K$9$k(B */
	checkCandidates(buf, buf->curLCStart, buf->curLCEnd);

	/*
	 * $B!&%+%l%s%HJ8@aHV9f$,(B buf->nClause $B$G$"$k>l9g(B
	 *	- $B$3$l$O%I%C%H$,:G8e$NJ8@a$N<!$K$"$k$H$$$&$3$H$J$N$G(B
	 *	  $B?7$7$$J8@a$r:n$k(B
	 * $B!&JQ49:Q$_$NJ8@a$N>l9g(B
	 *	- $BL5JQ49$N>uBV$KLa$7$F$+$iA^F~(B
	 * $B!&$=$NB>(B
	 *	- $BC1$KA^F~$9$l$P$h$$(B
	 */
	clp = buf->clauseInfo + buf->curLCStart;
	if (buf->curLCStart == buf->nClause) {
		/* $B?7$?$KJ8@a$r:n$k(B */
		/* clauseInfo $B$N%5%$%:$N%A%'%C%/(B */
		if (buf->nClause >= buf->clauseSize &&
		    resizeCInfo(buf, buf->nClause + 1) < 0) {
			return -1;
		}
		/* buf->nClause $B$N%"%C%W%G!<%H$H(B clauseInfo $B$N@_Dj(B */
		buf->nClause += 1;
		clp = buf->clauseInfo + buf->nClause;
		clp->conv = 0;
		clp->ltop = 1;
		clp->kanap = buf->kanaEnd;
		clp->dispp = buf->displayEnd;
	} else if (clp->conv) {
		/* $BL5JQ49>uBV$K$9$k(B */
		if (unconvert(buf, buf->curLCStart, buf->curLCEnd) < 0)
			return -1;
		buf->curClause = buf->curLCStart;
		buf->curLCEnd = buf->curLCStart + 1;
		DotSet(buf);
	}

	clp = buf->clauseInfo + buf->curLCStart;

	/* $B%P%C%U%!$NBg$-$5$N%A%'%C%/(B */
	ksizenew = (buf->kanaEnd - buf->kanaBuf) + 1;
	dsizenew = (buf->displayEnd - buf->displayBuf) + 1;
	if ((ksizenew > buf->bufferSize || dsizenew > buf->bufferSize) &&
	    resizeBuffer(buf, ksizenew > dsizenew ? ksizenew : dsizenew) < 0) {
		    return -1;
	}

	/* $B$+$J%P%C%U%!$r%"%C%W%G!<%H(B */
	dot = buf->dot;
	/* $B%+%l%s%HJ8@a$N8e$m$r0lJ8;z$:$i$9(B */
	moveKBuf(buf, buf->curLCStart + 1, 1);
	/* $B%+%l%s%HJ8@aFb$N%I%C%H0J9_$r0lJ8;z$:$i$9(B */
	(void)bcopy((char *)dot, (char *)(dot + 1),
		    ((clp + 1)->kanap - dot) * sizeof(wchar));
	/* $BA^F~(B */
	*dot = c;

	/* $BI=<(%P%C%U%!$r%"%C%W%G!<%H(B */
	dispdot = clp->dispp + (dot - clp->kanap);
	/* $B%+%l%s%HJ8@a$N8e$m$r0lJ8;z$:$i$9(B */
	moveDBuf(buf, buf->curLCStart + 1, 1);
	/* $B%+%l%s%HJ8@aFb$N%I%C%H0J9_$r0lJ8;z$:$i$9(B */
	(void)bcopy((char *)dispdot, (char *)(dispdot + 1),
	      ((clp + 1)->dispp - dispdot) * sizeof(wchar));
	/* $BA^F~(B */
	*dispdot = c;

	/* $B%I%C%H$r99?7(B */
	buf->dot++;

	return 0;
}

/* jcDeleteChar -- $B%I%C%H$NA0$^$?$O8e$m$N0lJ8;z$r:o=|$9$k(B */
int
jcDeleteChar(jcConvBuf *buf, int prev)
{
	jcClause	*clp;
	wchar		*dot, *dispdot;

	TRACE("jcDeleteChar", "Enter")

	CHECKFIXED(buf);

	clp = buf->clauseInfo;
	if (buf->nClause == 0) {
		/* $BJ8@a?t$,(B 0$B!"$D$^$j2?$bF~$C$F$$$J$$;~(B:
		 *	- $B%(%i!<(B
		 */
		jcErrno = JE_CANTDELETE;
		return -1;
	} else if (buf->curClause >= buf->nClause) {
		/* $B%+%l%s%HJ8@a$,:G8e$NJ8@a$N<!$K$"$k;~(B:
		 *	- prev $B$G$"$l$P!"A0$NJ8@a$N:G8e$NJ8;z$r:o=|(B
		 *	  $B%+%l%s%HJ8@a$OA0$NJ8@a$K0\F0$9$k(B
		 *	  $BI,MW$J$i$PA0$NJ8@a$rL5JQ49$KLa$7$F$+$i:o=|$9$k(B
		 *	  $BA0$NJ8@a$,$J$$$3$H$O$"$jF@$J$$(B
		 *	- !prev $B$J$i$P%(%i!<(B
		 */
		if (!prev) {
			jcErrno = JE_CANTDELETE;
			return -1;
		}
		(void)jcMove(buf, 0, JC_BACKWARD);
	} else if (clp[buf->curLCStart].conv) {
		/* $B%+%l%s%HJ8@a$,JQ49$5$l$F$$$k;~(B:
		 *	- prev $B$G$"$l$PA0$NJ8@a$N:G8e$NJ8;z$r:o=|(B
		 *	  $B%+%l%s%HJ8@a$OA0$NJ8@a$K0\F0$9$k(B
		 *	  $BI,MW$J$i$PA0$NJ8@a$rL5JQ49$KLa$7$F$+$i:o=|$9$k(B
		 *	  $B%+%l%s%HJ8@a$,@hF,$J$i$P%(%i!<(B
		 *	- !prev $B$J$i%+%l%s%HJ8@a$rL5JQ49$KLa$7$F!"J8@a$N(B
		 *	  $B:G=i$NJ8;z$r:o=|(B
		 */
		if (prev) {
			if (buf->curLCStart == 0) {
				jcErrno = JE_CANTDELETE;
				return -1;
			}
			(void)jcMove(buf, 0, JC_BACKWARD);
		}
	} else {
		/* $B%+%l%s%HJ8@a$,JQ49$5$l$F$$$J$$;~(B:
		 *	- prev $B$G$"$l$P%I%C%H$NA0$NJ8;z$r:o=|(B
		 *	  $B$?$@$7%I%C%H$,J8@a$N@hF,$K$"$l$PA0$NJ8@a$N(B
		 *	  $B:G8e$NJ8;z$r:o=|(B
		 *	  $B$=$N;~$K$O%+%l%s%HJ8@a$OA0$NJ8@a$K0\F0$9$k(B
		 *	  $BI,MW$J$i$PA0$NJ8@a$rL5JQ49$KLa$7$F$+$i:o=|$9$k(B
		 *	  $B%+%l%s%HJ8@a$,@hF,$J$i$P%(%i!<(B
		 *	- !prev $B$J$i%I%C%H$N<!$NJ8;z$r:o=|(B
		 *	  $B%I%C%H$,J8@a$N:G8e$NJ8;z$N<!$K$"$l$P%(%i!<(B
		 */
		if (prev) {
			if (buf->dot == clp[buf->curLCStart].kanap) {
				if (buf->curLCStart == 0) {
					jcErrno = JE_CANTDELETE;
					return -1;
				}
				(void)jcMove(buf, 0, JC_BACKWARD);
			}
		} else {
			if (buf->dot == clp[buf->curLCEnd].kanap) {
				jcErrno = JE_CANTDELETE;
				return -1;
			}
		}
	}

	if (buf->clauseInfo[buf->curLCStart].conv) {
		/* $B%+%l%s%HJ8@a$,JQ49:Q$_$G$"$l$PL5JQ49$KLa$9(B */
		if (jcUnconvert(buf) < 0)
			return -1;
		/* prev $B$G$"$l$PJ8@a$N:G8e$NJ8;z!"$=$&$G$J$1$l$PJ8@a$N(B
		 * $B@hF,$NJ8;z$r:o=|$9$k(B
		 */
		if (prev) {
			buf->dot = buf->clauseInfo[buf->curLCEnd].kanap - 1;
		} else {
			buf->dot = buf->clauseInfo[buf->curLCStart].kanap;
		}
	} else {
		/* prev $B$J$i%I%C%H$r#1J8;zLa$7$F$*$/(B
		 * $B$3$&$9$l$P%I%C%H$N8e$m$NJ8;z$r:o=|$9$k$3$H$K$J$k(B
		 * $B:o=|$7=*$o$C$?$H$-$K%I%C%H$rF0$+$9I,MW$b$J$$(B
		 */
		if (prev)
			buf->dot--;
	}

	clp = buf->clauseInfo + buf->curLCStart;

	/* $B$+$J%P%C%U%!$r%"%C%W%G!<%H(B */
	dot = buf->dot;
	/* $B%+%l%s%HJ8@aFb$N%I%C%H0J9_$r0lJ8;z$:$i$9(B */
	(void)bcopy((char *)(dot + 1), (char *)dot,
		    ((clp + 1)->kanap - (dot + 1)) * sizeof(wchar));
	/* $B%+%l%s%HJ8@a$N8e$m$r0lJ8;z$:$i$9(B */
	moveKBuf(buf, buf->curLCEnd, -1);

	/* $BI=<(%P%C%U%!$r%"%C%W%G!<%H(B */
	dispdot = clp->dispp + (dot - clp->kanap);
	/* $B%+%l%s%HJ8@aFb$N%I%C%H0J9_$r0lJ8;z$:$i$9(B */
	(void)bcopy((char *)(dispdot + 1), (char *)dispdot,
		   ((clp + 1)->dispp - (dispdot + 1)) * sizeof(wchar));
	/* $B%+%l%s%HJ8@a$N8e$m$r0lJ8;z$:$i$9(B */
	moveDBuf(buf, buf->curLCEnd, -1);

	/* $B%+%l%s%HJ8@a$ND9$5$,#1$@$C$?>l9g$K$OJ8@a$,#18:$k$3$H$K$J$k(B */
	if (clp->kanap == (clp + 1)->kanap) {
		/* $BJ8@a$,$J$/$J$C$F$7$^$C$?(B */
		moveCInfo(buf, buf->curLCEnd, -1);
		setCurClause(buf, buf->curLCStart);
		DotSet(buf);
	}

	return 0;
}

/* jcKillLine -- $B%I%C%H0J9_$r:o=|$9$k(B */
int
jcKillLine(jcConvBuf *buf)
{
	int cc = buf->curClause;

	TRACE("jcKillLine", "Enter")

	CHECKFIXED(buf);

	/* $BF~NOCf$NJ8@a$,$J$$$+!"%I%C%H$,:G8e$NJ8@a$N<!$K$"$l$P!"%(%i!<(B */
	if (buf->nClause <= 0 || cc >= buf->nClause) {
		jcErrno = JE_CANTDELETE;
		return -1;
	}

#ifdef DEBUG_WNNLIB
        showBuffers(buf, "before jcKillLine");
#endif

	/* $B%I%C%H$,F~NO$N@hF,$G$"$l$P!"(BjcClear $B$r8F=P$7$F=*$j(B */
	if (buf->dot == buf->kanaBuf)
		return jcClear(buf);

	/*
	 * $B%I%C%H0J9_$r:o=|$9$k(B
	 * $B$H$$$C$F$b!"C1$KJ8@a>pJs$H%]%$%s%?$rJQ99$9$l$PNI$$(B
	 */
	checkCandidates(buf, cc, buf->nClause);
	if (buf->clauseInfo[cc].conv) {
		/* $BJQ49$5$l$F$$$l$P!"%+%l%s%HJ8@a$r4^$a$F:o=|(B */
		buf->kanaEnd = buf->dot = buf->clauseInfo[cc].kanap;
		buf->displayEnd = buf->clauseInfo[cc].dispp;

		/* $B%+%l%s%HJ8@a$rKvHxJ8@a$K0\$9(B */
		buf->nClause = buf->curClause = buf->curLCStart = cc;
		buf->curLCEnd = cc + 1;
	} else {
		/* $BL$JQ49$J$i$P!"%I%C%H0J9_$r:o=|(B */
		buf->kanaEnd = buf->dot;
		buf->displayEnd = buf->clauseInfo[cc].dispp
				+ (buf->dot - buf->clauseInfo[cc].kanap);

		/* $B%+%l%s%HJ8@a$O$=$N$^$^$G!"KvHx$@$1$r5$$K$9$l$P$h$$(B */
		cc++;
		buf->nClause = buf->curLCEnd = cc;
	}

	/* $B6u$NKvHxJ8@a$N@_Dj$r$9$k(B */
	buf->clauseInfo[cc].kanap = buf->kanaEnd;
	buf->clauseInfo[cc].dispp = buf->displayEnd;
	buf->clauseInfo[cc].conv = 0;
	buf->clauseInfo[cc].ltop = 1;

	/* $B%+%l%s%HJ8@a$H$=$l0J9_$N(B jllib $B$NJ8@a>pJs$bL58z$K$9$k(B */
	if (jl_bun_suu(buf->wnn) > cc)
		jl_kill(buf->wnn, cc, -1);

#ifdef DEBUG_WNNLIB
        showBuffers(buf, "after jcKillLine");
#endif
	return 0;
}

/* jcChangeClause -- $B%+%l%s%HBgJ8@a$r;XDj$5$l$?J8;zNs$GCV$-49$($k(B */
int
jcChangeClause(jcConvBuf *buf, wchar *str)
{
	jcClause	*clps, *clpe;
	wchar	*p;
	int	newlen;
	int	oklen, odlen;
	int	ksize, dsize;

	TRACE("jcChangeClause", "Enter")

	CHECKFIXED(buf);

	clps = buf->clauseInfo + buf->curLCStart;
	clpe = buf->clauseInfo + buf->curLCEnd;

	newlen = 0;
	p = str;
	while (*p++)
		newlen++;

	/* $B$+$J%P%C%U%!$HI=<(%P%C%U%!$N%5%$%:$rD4$Y$F!"(B
	 * $BF~$i$J$+$C$?$iBg$-$/$9$k(B
	 */
	if (buf->curLCStart < buf->nClause) {
		oklen = clpe->kanap - clps->kanap;
		odlen = clpe->dispp - clps->dispp;
	} else {
		oklen = odlen = 0;
	}
	ksize = (buf->kanaEnd - buf->kanaBuf) + newlen - oklen;
	dsize = (buf->displayEnd - buf->displayBuf) + newlen - odlen;
	if (ksize > buf->bufferSize || dsize > buf->bufferSize) {
		if (resizeBuffer(buf, ksize > dsize ? ksize : dsize) < 0)
			return -1;
	}

	/* curLCStart $B$,(B nClause $B$KEy$7$$;~$@$1!"?7$?$KJ8@a$,:n$i$l$k(B */
	if (buf->curLCStart == buf->nClause) {
		/* clauseInfo $B$NBg$-$5$rD4$Y$k(B*/
		if (buf->nClause + 1 > buf->clauseSize) {
			if (resizeCInfo(buf, buf->nClause + 1) < 0)
				return -1;
		}
		/* $B?7$?$K$G$-$?(B clauseInfo $B$K$O!"(BnClause $BHVL\(B
		 * ($B$D$^$j:G8e$N(B clauseInfo) $B$NFbMF$r%3%T!<$7$F$*$/(B
		 */
		clpe = buf->clauseInfo + buf->nClause + 1;
		*clpe = *(clpe - 1);

		buf->nClause++;
	}

	clps = buf->clauseInfo + buf->curLCStart;
	clpe = buf->clauseInfo + buf->curLCEnd;

	/* $B$+$J%P%C%U%!$NJQ99(B */
	/* $B$^$:$O8e$m$r0\F0$5$;$k(B */
	moveKBuf(buf, buf->curLCEnd, newlen - oklen);
	/* str $B$r%3%T!<(B */
	(void)bcopy((char *)str, (char *)clps->kanap,
		    newlen * sizeof(wchar));
	/* $BI=<(%P%C%U%!$NJQ99(B */
	/* $B$^$:$O8e$m$r0\F0$5$;$k(B */
	moveDBuf(buf, buf->curLCEnd, newlen - odlen);
	/* str $B$r%3%T!<(B */
	(void)bcopy((char *)str, (char *)clps->dispp,
		    newlen * sizeof(wchar));

	/* clauseInfo $B$NJQ99(B */
	/* $B$^$:$O8e$m$r0\F0$5$;$k(B */
	if (clpe > clps + 1) {
		(void)bcopy((char *)clpe, (char *)(clps + 1),
			    (buf->nClause + 1 - buf->curLCEnd) *
			    sizeof(jcClause));
	}
	clps->conv = 0;
	clps->ltop = 1;
	(clps + 1)->ltop = 1;

	return 0;
}

/* jcSaveDic -- $B<-=q!&IQEY%U%!%$%k$r%;!<%V$9$k(B */
int
jcSaveDic(jcConvBuf *buf)
{
	TRACE("jcSaveDic", "Enter")

	return jl_dic_save_all(buf->wnn);
}

/* $B%5!<%P$H$N@\B3$N$?$a$N4X?t72(B */

struct wnn_buf *
jcOpen(char *server, char *envname, int override, char *rcfile, void (*errmsg)(), int (*confirm)(), int timeout)
{
    return jcOpen2(server, envname, override, rcfile, rcfile, errmsg, confirm, timeout);
}

struct wnn_buf *
jcOpen2(char *server, char *envname, int override, char *rcfile4, char *rcfile6, void (*errmsg)(), int (*confirm)(), int timeout)
{
    struct wnn_buf *wnnbuf;
    struct wnn_env *wnnenv;
    char *rcfile;
    int env_exists;
    int wnn_version;

    TRACE("jcOpen2", "Enter")

    /* $B%5!<%PL>$,(B NULL $B$^$?$O6uJ8;zNs$@$C$?>l9g$O4D6-JQ?t(B JSERVER $B$r;HMQ$9$k(B */
    if (server == NULL || server[0] == '\0') {
	server = getenv("JSERVER");
    }

    /* $B4D6-L>$,6uJ8;zNs$@$C$?>l9g$O!"%f!<%6L>$r;HMQ$9$k(B */
    if (envname != NULL && *envname == 0) {
        struct passwd *p = getpwuid(getuid());

	if (p != NULL) envname = p->pw_name;
    }

    /*
     * jserver $B$N%P!<%8%g%s$K$h$C$F(B wnnrc $B$rJQ$($?$$$N$@$,!"(B
     * $B%P!<%8%g%s$rD4$Y$k$?$a$K$O$^$:@\B3$7$J$/$F$O$J$i$J$$!#(B
     * $B$=$3$G(B wnnrc $B0z?t$r(B NULL $B$K$7$F@\B3$9$k!#(B
     */
#if JSERVER_VERSION > 0x4030
    wnnbuf = jl_open_lang(envname, server, "ja_JP",
                          NULL, confirm, (int (*)())errmsg, timeout);
#else
    wnnbuf = jl_open(envname, server, NULL, confirm, errmsg, timeout);
#endif

    /*
     * $B!&%P%C%U%!$,:n$l$J$+$C$?(B
     * $B!&(Bjserver $B$K@\B3$G$-$J$+$C$?(B
     * $B!&(Bwnnrc $B%U%!%$%k$N;XDj$,$J$$(B ($B$D$^$j=i4|2=$7$J$$(B)
     * $B>l9g$K$O$3$l$G=*$j!#(B
     */
    if (wnnbuf == NULL ||
	!jl_isconnect(wnnbuf) ||
	(rcfile4 == NULL && rcfile6 == NULL)) {
	return wnnbuf;
    }

    wnnenv = jl_env_get(wnnbuf);

    /*
     * $B0JA0$+$i4D6-$,B8:_$7$F$$$?$+$I$&$+$H!"%5!<%P$N%P!<%8%g%s$rD4$Y$k!#(B
     * $B4D6-$,B8:_$7$F$$$?$+$I$&$+$O(B jl_fuzokugo_get $B$G(B ($B$D$^$jIUB08l(B
     * $B<-=q$,@_Dj$5$l$F$$$k$+$I$&$+$G(B) $BH=CG$9$k!#(Bjl_open_lang $B$O4D6-$,(B
     * $B$J$1$l$P:n$C$F$7$^$&$?$a!"(Bjs_env_exist $B$O;H$($J$$!#(B
     */
    {
	char fzk[1024];
	int serv_ver, lib_ver;

	if (ki2_jl_fuzokugo_get(wnnbuf, fzk, 1024) != -1) {
	    env_exists = 1;
	    TRACE("jcOpen2", "env exists");
	} else {
	    env_exists = 0;
	    TRACE("jcOpen2", "no env");
	}
	if (js_version(wnnenv->js_id, &serv_ver, &lib_ver) != -1 &&
	    serv_ver >= 0x4f00) {
	    wnn_version = 6;
	    TRACE("jcOpen2", "Wnn6");
	} else {
	    wnn_version = 4;
	    TRACE("jcOpen2", "Wnn4");
	}
    }

    /* wnnrc $B$NA*Br(B */
    rcfile = (wnn_version == 4) ? rcfile4 : rcfile6;

    /*
     * $B4D6-$,$9$G$KB8:_$7$+$D4D6-$N>e=q$-$,;XDj$5$l$F$$$J$$!"$"$k$$$O(B
     * rcfile $B$,(B NULL $B$N>l9g$K$O$3$l$G=*$j!#(B
     */
    if ((env_exists && !override) || rcfile == NULL) return wnnbuf;

    /*
     * wnnrc $B$,6uJ8;zNs$@$C$?>l9g$O!"%G%U%)%k%H$r;HMQ$9$k!#(B
     * 1. $B4D6-JQ?t(B WNNENVRC4 $B$^$?$O(B WNNENVRC6
     * 2. $B4D6-JQ?t(B WNNENVRC
     * 3. $B%7%9%F%`$N%G%U%)%k%H(B
     * $B$N=g$G8!:w$9$k!#:G8e$N$O$A$g$C$H$$$$$+$2$s!#(B
     */
    if (*rcfile == '\0') {
	rcfile = getenv((wnn_version == 4) ? "WNNENVRC4" : "WNNENVRC6");
	if (rcfile == NULL || access(rcfile, R_OK) != 0) {
	    rcfile = getenv("WNNENVRC");
	}
	if (rcfile == NULL || access(rcfile, R_OK) != 0) {
	    if (wnn_version == 6) {
#ifdef WNN6
		rcfile = "@DEFAULT";
#else
		rcfile = "wnnenvrc";
#endif
	    } else {
#if defined(WNNENVDIR) && JSERVER_VERSION > 0x4030
		static char envrc[256];
		rcfile = envrc;
		(void)snprintf(rcfile, sizeof(envrc), "%s/ja_JP/wnnenvrc", WNNENVDIR);
		if (access(rcfile, R_OK) != 0)
		    (void) snprintf(rcfile, sizeof(envrc), "%s/wnnenvrc", WNNENVDIR);
		fprintf( stderr , "%s\n" , rcfile) ;
#else
		rcfile = "wnnenvrc";
#endif
	    }
        }
    }

    /* $B4D6-@_Dj$9$k(B */
    (void)jl_set_env_wnnrc(wnnenv, rcfile, confirm, (int (*)())errmsg);

    return wnnbuf;
}

void
jcClose(struct wnn_buf *wnnbuf)
{
    TRACE("jcClose", "Enter")

    if (wnnbuf != NULL)
	jl_close(wnnbuf);
}

int
jcIsConnect(struct wnn_buf *wnnbuf)
{
    TRACE("jcIsConnect", "Enter")

    if (wnnbuf == NULL)
        return 0;
    return jl_isconnect(wnnbuf);
}


#ifdef DEBUG_WNNLIB
static void
printBuffer(wchar *start, wchar *end)
{
	wchar wc;

	while (start < end) {
		wc = *start++;
		if (wc >= 0200) {
			putc((wc >> 8) & 0xff, stderr);
			wc &= 0xff;
		} else if (wc < 040 || wc == 0177) {
			putc('^', stderr);
			wc ^= 0100;
		} else if (wc == '^' || wc == '\\') {
			putc('\\', stderr);
		}
		putc(wc, stderr);
	}
}

static void
showBuffers(jcConvBuf *buf, char *tag)
{
	int i;
	jcClause *clp = buf->clauseInfo;
	wchar ws[512];

	fprintf(stderr, "Buffer Info [%s]\n", tag);
	fprintf(stderr, "nClause = %d, curClause = %d [%d, %d], ",
		 buf->nClause, buf->curClause, buf->curLCStart, buf->curLCEnd);

	if (buf->dot < buf->kanaBuf) {
		fprintf(stderr, "dot < 0\n");
	} else if (buf->dot > buf->kanaEnd) {
		fprintf(stderr, "dot > 0\n");
	} else if (buf->nClause == 0) {
		fprintf(stderr, "dot == 0\n");
	} else {
		for (i = 0; i < buf->nClause; i++) {
			if (buf->dot <= clp[i].kanap)
				break;
		}
		if (buf->dot < clp[i].kanap)
			i--;
		fprintf(stderr, "dot = %d.%d\n", i, buf->dot - clp[i].kanap);
	}

	for (i = 0; i < buf->nClause; i++) {
		fprintf(stderr,
			"clause[%d]: conv = %d, ltop = %d",
			i, clp->conv, clp->ltop);
		if (clp->conv == 1) {
			fprintf(stderr, " [%d]", jl_dai_top(buf->wnn, i));
		}
		fprintf(stderr, "\n");
		fprintf(stderr, "clause[%d]: Kana = '", i);
		printBuffer(clp->kanap, (clp + 1)->kanap);
		fprintf(stderr, "'\n");
		if (clp->conv == 1) {
			fprintf(stderr, "clause[%d]: Yomi = '", i);
			(void)ki2_jl_get_yomi(buf->wnn, i, i + 1, ws, sizeof(ws));
			printBuffer(ws, ws + jl_yomi_len(buf->wnn, i, i + 1));
			fprintf(stderr, "'\n");
		}
		fprintf(stderr, "clause[%d]: Disp = '", i);
		printBuffer(clp->dispp, (clp + 1)->dispp);
		fprintf(stderr, "'\n");
		if (clp->conv == 1) {
			fprintf(stderr, "clause[%d]: Conv = '", i);
			(void)ki2_jl_get_kanji(buf->wnn, i, i + 1, ws, sizeof(ws));
			printBuffer(ws, ws + jl_kanji_len(buf->wnn, i, i + 1));
			fprintf(stderr, "'\n");
		}
		clp++;
	}
}
#endif /* DEBUG_WNNLIB */
