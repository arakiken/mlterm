#include "indian.h"
#include <ctype.h>	/* isprint */

char *binsearch(struct tabl *table, int sz, char *word) {
	int result, index, lindex, hindex;

	if (isprint(word[0])) return word;

	if (word[1] == '\0') {
		if (0xf1 <= ((unsigned char*)word)[0] && ((unsigned char*)word)[0] <= 0xfa) {
			/* is digit */
			word[0] -= 0xc1;
			return word;
		}
		else if ( ((unsigned char*)word)[0] == 0xea) {
			/* full stop */
			word[0] = 0x2a;
			return word;
		}
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
