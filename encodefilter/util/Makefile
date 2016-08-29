SRCDIR=src

ucs_map: jisx0208 jisx0212 jisx0213 iso8859-2 iso8859-3 iso8859-4 iso8859-10 \
	iso8859-13 iso8859-14 iso8859-16 johab tcvn viscii koi8-t georgian-ps \
	cp1250 cp1251 cp1252 cp1253 cp1254 cp1255 cp1256 \
	cp1257 cp1258 big5 big5hkscs cns11643 gbk uhc property

# Manually maintained tables.
ucs_map2: jisx0208-nec jisx0208-ibm sjis-ibm

jisx0208:
	cat ${SRCDIR}/JIS0208.TXT.fixed | ./jisx0208_1983_table.pl

jisx0208-nec:
	cat ${SRCDIR}/nec98.txt | ./jisx0208_nec_ext_table.pl

jisx0208-ibm:
	cat ${SRCDIR}/necibm.txt | ./jisx0208_necibm_ext_table.pl

sjis-ibm:
	cat ${SRCDIR}/ibmibm.txt | ./sjis_ibm_ext_table.pl

jisx0212:
	cat ${SRCDIR}/JIS0212.TXT | ./jisx0212_1990_table.pl

jisx0213:
	cat ${SRCDIR}/jisx0213code-csv.txt | ./jisx0213_2000_table.pl

iso8859-2:
	cat ${SRCDIR}/8859-2.TXT | ./iso8859_2_table.pl

iso8859-3:
	cat ${SRCDIR}/8859-3.TXT | ./iso8859_3_table.pl

iso8859-4:
	cat ${SRCDIR}/8859-4.TXT | ./iso8859_4_table.pl

iso8859-10:
	cat ${SRCDIR}/8859-10.TXT | ./iso8859_10_table.pl

iso8859-13:
	cat ${SRCDIR}/8859-13.TXT | ./iso8859_13_table.pl

iso8859-14:
	cat ${SRCDIR}/8859-14.TXT | ./iso8859_14_table.pl

iso8859-16:
	cat ${SRCDIR}/8859-16.TXT | ./iso8859_16_table.pl

johab:
	cat ${SRCDIR}/hangul-codes.txt | ./johab_table.pl

uhc:
	cat ${SRCDIR}/KSC5601.TXT | ./uhc_table.pl

tcvn:
	cat ${SRCDIR}/vscii.txt | ./tcvn_table.pl

viscii:
	cat ${SRCDIR}/unicode.html | ./viscii_table.pl

koi8-t:
	cat ${SRCDIR}/KOI8-T | ./koi8_t_table.pl

georgian-ps:
	cat ${SRCDIR}/GEORGIAN-PS | ./georgian_ps_table.pl

cp1250:
	cat ${SRCDIR}/CP1250.TXT | ./cp1250_table.pl

cp1251:
	cat ${SRCDIR}/CP1251.TXT | ./cp1251_table.pl

cp1252:
	cat ${SRCDIR}/CP1252.TXT | ./cp1252_table.pl

cp1253:
	cat ${SRCDIR}/CP1253.TXT | ./cp1253_table.pl

cp1254:
	cat ${SRCDIR}/CP1254.TXT | ./cp1254_table.pl

cp1255:
	cat ${SRCDIR}/CP1255.TXT | ./cp1255_table.pl

cp1256:
	cat ${SRCDIR}/CP1256.TXT | ./cp1256_table.pl

cp1257:
	cat ${SRCDIR}/CP1257.TXT | ./cp1257_table.pl

cp1258:
	cat ${SRCDIR}/CP1258.TXT | ./cp1258_table.pl

big5:
	cat ${SRCDIR}/BIG5 | ./big5_table.pl

big5hkscs:
	cat ${SRCDIR}/BIG5HKSCS | ./big5hkscs_table.pl

cns11643:
	cat ${SRCDIR}/CNS11643.TXT | ./cns11643_1992_table.pl

gbk:
	cat ${SRCDIR}/CP936.TXT | ./gbk_table.pl

property:
	cat ${SRCDIR}/UnicodeData-20130512.txt ${SRCDIR}/EastAsianWidth-6.3.0.txt | ./ucs_property.pl

clean:
	rm -f mef_*.table
