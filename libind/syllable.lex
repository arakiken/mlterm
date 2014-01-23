%{
#include "indian.h"
char word[1000], outstr[1000];
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

CNS [³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÈÉÊËÌÍÏĞÑÒÔÕÖ×Ø]
VOWELS [¤¥¦§¨©ª«¬­¯®°±²]
MATRAS [ÚÛÜİŞßàáâäãåæç]
VOWMOD [¡¢£]
HALMWA [è]
NUKTA [é]
NON_DEV [^¡-é\n\t ]
DIGIT [ñ-ú]

%%

{VOWELS}?{VOWMOD}?{NUKTA}? |
{CNS}{VOWMOD}?{HALMWA}?{NON_DEV} |
({CNS}{NUKTA}?{HALMWA})*{CNS}{NUKTA}?{HALMWA}?{HALMWA}?{MATRAS}?{VOWMOD}? |
{MATRAS} |
{DIGIT} |
{CNS}?{HALMWA}{NUKTA}?{CNS}? process_it(table, sz, yytext);

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
	char *p;
	size_t len;
	char tmp;
	int i;

	len = strlen(inpword);
	tmp = '\0';

	while(1) {
		for(i = len; i > 0; i--) {
			tmp = inpword[i];
			inpword[i] = '\0';
			p = binsearch(table, sz, inpword);
			inpword[i] = tmp;
			if(p) {
				strcat(outstr,p);
				break;
			}
		}

		if(i == 0) i = 1;

		if((len -= i) > 0) {
			inpword += i;
		}
		else {
			break;
		}
	}

	return 1;
}

int splitwrap(){
	return 1;
}
