#include "indian.h"
#include <ctype.h>	/* isprint */

char *binsearch(struct tabl *table, int sz, char *word) {
	int result, index, lindex, hindex;

	if (isprint(word[0])) return word;

	if (word[1] == '\0' && 0xf1 <= ((u_char*)word)[0] && ((u_char*)word)[0] <= 0xfa) {
		/* is digit */
		word[0] -= 0xc1;
		return word;
	}

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

int iscii2font(struct tabl *table, char *input, char *output, int sz) {
	bzero(output,strlen(output));
	strcat(output , (char *) split(table, input, sz));
	return strlen(output); 
}
