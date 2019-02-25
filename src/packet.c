#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "rtp.h"
#include "packet.h"

Packet initPacketRTP(u_int32_t seq, u_int32_t ack, int8_t flags, void *data, size_t size){
	Packet p = malloc(sizeof(struct Packet_t));
	if (p == NULL)
		errorRTP("Could not allocate memory for packet rep", ERROR_INIT);

	struct Header_t header;
	memset(&header, 0, sizeof(header)); //Because padding causes uninitialized bytes to be sent into a syscall which is probably a bad idea

	header.seq = seq;
	header.ack = ack;
	header.checksum = 0;
	header.len = (int16_t)size;
	header.flags = flags;

	p->head = malloc(sizeof(struct Header_t) + size);
	if (p->head == NULL)
		errorRTP("Could not allocate memory for packet data", ERROR_INIT);
	memcpy(p->head, &header, sizeof(struct Header_t));

	if (data != NULL) {
		p->data = (char*)p->head + sizeof(struct Header_t);
		memcpy(p->data, data, size);
	} else {
		p->data = NULL;
	}

	p->size = sizeof(struct Header_t) + size;
	p->dsize = size;
	p->head->checksum = checksumRTP(p);

	return p;
}

void freePacketRTP(Packet p){
	if (p->head != NULL)
		free(p->head);
	free(p);
}

int16_t checksumRTP(Packet p){

	int16_t ret = 0;

	int16_t tcsm = p->head->checksum;
	p->head->checksum = 0;

	int64_t divlen = p->size / sizeof(int16_t);
	for (int64_t i = 0; i < divlen; i++){
		ret += ((int16_t*) p->head)[i];
	}

	if (p->size & 1){
		ret += ((int8_t*) p->head)[divlen * 2];
	}

	ret ^= 0x11111111;

	p->head->checksum = tcsm;

	return ret;
}

size_t readDataSizeRTP(void *buf){
	Header h = (Header)buf;
	return (size_t)h->len;
}

Packet readPacketRTP(void *buf) {
	Packet p = malloc(sizeof(struct Packet_t));
	if (p == NULL)
		errorRTP("Could not allocate memory for packet rep", ERROR_INIT);

	p->head = (Header) buf;
	p->data = (char*) buf + sizeof(struct Header_t);
	p->dsize = p->head->len;
	p->size = p->dsize + sizeof(struct Header_t);
	if (p->head->checksum == checksumRTP(p)) {
		void *data = malloc(p->size);
		memcpy(data, p->head, p->size);
		p->head = data;
		p->data = (char *) p->head + sizeof(struct Header_t);
		return p;
	} else {
		free(p);
		return NULL;
	}
}

void printDiagsPacketRTP(Packet p){
	for (int i = 0; i < 30; i++) printf("-");
	printf("\n");

	printDiagsHeaderRTP(p->head);

	printf("Packet size:%lu\n", p->size);
	printf("Packet dsize:%lu\n", p->dsize);

	printf("Contains:\n");
	int nc = 0;
	int bc = 0;
	for (int i = 0; i < p->dsize; i++) {
		nc++;
		printf("%02x", *((unsigned char *)p->data + i));
		if (nc == sizeof(int16_t)) {
			printf(" ");
			nc = 0;
			bc++;
		}
		if (bc == 16){
			bc = 0;
			printf("\n");
		}
	}
	if (bc != 0){
		printf("\n");
	}

	for (int i = 0; i < 30; i++) printf("-");
	printf("\n");
}

void printDiagsHeaderRTP(Header h){
	for (int i = 0; i < 30; i++) printf("-");
	printf("\n");

	printf("Header seq:%d\n", h->seq);
	printf("Header ack:%d\n", h->ack);
	printf("Header checksum:%d\n", h->checksum);
	printf("Header len:%d\n", h->len);
	printf("Header flags:");
	if (h->flags & SYN){
		printf("SYN ");
	}
	if (h->flags & FIN){
		printf("FIN ");
	}
	if (h->flags & ACK){
		printf("ACK ");
	}
	if (h->flags & CMP){
		printf("CMP ");
	}
	printf("\n");

	for (int i = 0; i < 30; i++) printf("-");
	printf("\n");
}