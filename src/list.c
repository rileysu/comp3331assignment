#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtp.h"
#include "list.h"

ListRTP initListRTP(size_t nsize, int64_t initlength){
	ListRTP l = malloc(sizeof(struct ListRTP_t));
	if (l == NULL){
		errorRTP("Could not allocate memory for list", ERROR_INIT);
	}

	l->head = malloc(nsize * initlength);
	if (l->head == NULL){
		errorRTP("Could not allocate memory for list", ERROR_INIT);
	}
	l->nsize = nsize;
	l->clength = 0;
	l->length = initlength;

	return l;
}

void freeListRTP(ListRTP l){
	free(l->head);
	free(l);
}

void addListRTP(void *src, ListRTP l){
	if (l->clength == l->length)
		expandListRTP(l->length*2, l);

	memcpy((char*)l->head + l->nsize * l->clength, src, l->nsize);

    l->clength++;
}

void insertListRTP(int64_t ind, void *src, ListRTP l){
	if (l->clength == l->length)
		expandListRTP(l->length*2, l);

	memmove((char*)l->head + (ind+1)*l->nsize, (char*)l->head + ind*l->nsize, (l->clength - ind)*l->nsize);
	memcpy((char*)l->head + ind*l->nsize, src, l->nsize);

    l->clength++;
}

void removeListRTP(int64_t ind, ListRTP l){
	//Dw i know what im doing
	memmove((char*)l->head + ind*l->nsize, (char*)l->head + (ind+1)*l->nsize, (l->clength - ind)*l->nsize);
	l->clength--;
}

void *getListRTP(int64_t ind, ListRTP l){
	return (char*)l->head + ind*l->nsize;
}

void expandListRTP(int64_t length, ListRTP l){
	l->head = realloc(l->head, l->nsize * length);
	if (l->head == NULL){
		errorRTP("Could not reallocate memory for list", ERROR_MISC);
	} else {
		l->length = length;
	}
}

void printDiagsListRTP(ListRTP l){
	for (int i = 0; i < 30; i++) printf("-");
	printf("\n");
	
	printf("List Length:%ld\n", l->length);
	printf("List CLength:%ld\n", l->clength);

	printf("Contains:\n");
	for (int64_t i = 0; i < l->clength; i++){
		for (int64_t j = 0; j < l->nsize; j++){
			//Endianness is fun :D
			printf("%02x", *((unsigned char*)l->head + i*l->nsize + j));
		}
		printf("\n");
	}
	
	for (int i = 0; i < 30; i++) printf("-");
	printf("\n");
}
