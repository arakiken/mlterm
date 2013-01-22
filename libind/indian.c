#include "indian.h"
#include <ctype.h>	/* isprint */

char *binsearch(struct tabl *table, int sz, char *word) {
	int result, index, lindex, hindex;

	if (isprint(word[0])) return word;

	lindex = 0 ;
	hindex = sz ;

	while( 1)
	{
		index = (lindex + hindex) / 2;	

		result = strcmp(table[index].iscii,word);

		if (result == 0) return table[index].font;
		if (result > 0) hindex = index;
		if (result < 0) lindex = index + 1;

		if (lindex >= hindex) return NULL;
	}
}

char *illdefault(struct tabl *table, char *wrd, int sz) {
	
	int i;
	char *ostr;
	char iwrd[2];
	char *p;
	
	ostr = (char *) calloc(1000 , sizeof(char));

	for(i = 0; i < strlen(wrd); i++) {
		iwrd[0]=wrd[i];
		iwrd[1]='\0';
		if((p=binsearch(table, sz, iwrd)))
			strcat(ostr,p);
	}

	return ostr;
}

int iscii2font(struct tabl *table, char *input, char *output, int sz) {
	bzero(output,strlen(output));
	strcat(output , (char *) split(table, input, sz));
	return strlen(output); 
}
