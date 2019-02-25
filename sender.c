#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include "rtp.h"
#include "list.h"
#include "packet.h"
#include "logger.h"
#include "pld.h"

int main(int argc, char *argv[]) {

	if (argc != 15)
		errorRTP("Incorrect amount of arguments entered!", ERROR_INIT);

	in_addr_t receiverhost = inet_addr(argv[1]);
	in_port_t receiverport = htons(atoi(argv[2]));
	FILE *fp = fopen(argv[3], "r");
	if (fp == NULL){
		errorRTP("Could not open file\n", ERROR_INIT);
	}
	int mws = atoi(argv[4]);
	int mss = atoi(argv[5]);
	int gamma = atoi(argv[6]);
	double pDrop = atof(argv[7]);
	double pDuplicate = atof(argv[8]);
	double pCorrupt = atof(argv[9]);
	double pOrder = atof(argv[10]);
	int maxOrder = atoi(argv[11]);
	double pDelay = atof(argv[12]);
	int maxDelay = atoi(argv[13]);
	unsigned int seed = (unsigned int)atoi(argv[14]);

	Socket s = initSocketRTP(AF_INET, (size_t)mss, (size_t)mws, gamma);
	s->pld = initPLDRTP(pDrop, pDuplicate, pCorrupt, pOrder, maxOrder, pDelay, maxDelay, seed);

	char *buf = calloc(sizeof(char), 20971520); //20MB limit
	int read;
	int i;

	for (i = 0; i < 20971520 && (read = getc(fp)) != EOF; i++){
		buf[i] = read;
	}

	connectSocketRTP(AF_INET, receiverport, receiverhost, s);

	sendSocketRTP(buf, (size_t)i, s);

	closeSocketRTP(s);

	writeLogger("sender.log", LOGGER_SENDER, s->logger);

	fclose(fp);

	freePLDRTP(s->pld);

	freeSocketRTP(s);

	free(buf);

	return 0;
}
