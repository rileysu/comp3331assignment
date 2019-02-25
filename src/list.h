#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <stdint.h>

typedef struct ListRTP_t{
	void *head;
	size_t nsize;
	int64_t clength;
	int64_t length;
} *ListRTP;

//Init
ListRTP initListRTP(size_t,int64_t);

//Free
void freeListRTP(ListRTP);

//Add
void addListRTP(void*,ListRTP);
void insertListRTP(int64_t,void*,ListRTP);

//Remove
void removeListRTP(int64_t,ListRTP);

//Get
void *getListRTP(int64_t,ListRTP);

//Misc
void expandListRTP(int64_t,ListRTP);

//Diag
void printDiagsListRTP(ListRTP);

#endif
