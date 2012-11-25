%{
#include "indian.h"
char word[1000], string1[1000],outstr[1000];
int process_it(struct tabl *, int, char *);
int my_yyinput(char *, int);
#undef YY_INPUT 
#undef YY_DECL

#define YY_INPUT(inp,result,maxlen) { \
	 (result = (word[0] == '\0') ? YY_NULL : my_yyinput(inp,maxlen)); \
	word[0] = '\0'; \
	}

#define YY_DECL int yylex (struct tabl *table, int sz)

%}

CNS [≥¥µ∂∑∏π∫ªºΩæø¿¡¬√ƒ≈∆»… ÀÃÕœ–—“‘’÷◊ÿ]
VOWELS [§•¶ß®©™´¨≠ØÆ∞±≤]
MATRAS [⁄€‹›ﬁﬂ‡·‚‰„ÂÊÁ]
VOWMOD [°¢£]
HALMWA [Ë]
NUKTA [È]
NON_DEV [^°-È\n\t ]

%%

{VOWELS}{VOWMOD}? | 
{CNS}{VOWMOD}?{HALMWA}?{NON_DEV} |
({CNS}{NUKTA}?{HALMWA})*{CNS}{NUKTA}?{HALMWA}?{HALMWA}?{MATRAS}?{VOWMOD}? |
{MATRAS} |
{VOWMOD}{NUKTA}? |
{NUKTA}  |
{CNS}?{HALMWA}{NUKTA}?{CNS}? process_it(table, sz, yytext);

. illdefault(table, yytext, sz);

%%

int my_yyinput(char *buf, int max_size) {
	strcpy(buf,word);
	return strlen(word);

}

char *split(struct tabl *table, char *input1, int sz) {
	
	memset(outstr,0,1000);
	strcpy(word,input1);
	splitlex(table, sz);
	return outstr;
}

int process_it(struct tabl *table, int sz, char *inpword) {
	char* p;
	sprintf(string1+strlen(string1),"%s",inpword);
	if(!(p=binsearch(table, sz, string1))) {
		strcat(outstr, (p=illdefault(table,string1,sz)));
		free(p);
	}
	else {
		strcat(outstr, p);
	}

	memset(string1,0,1000);
	return 1;
}

int splitwrap(){
	return 1;
}
