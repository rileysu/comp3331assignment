#ifndef PACKET_H
#define PACKET_H

#include <sys/types.h>

typedef struct /*__attribute__((packed))*/ Header_t{
        u_int32_t seq;
        u_int32_t ack;
        int16_t checksum;
        u_int16_t len;
        int8_t flags;
} *Header;


//Packet structure
//
//---------------
//| Head | Data | in a single alloc
//---------------
//
//Size of head is sizeof(struct header_t)
//Size of data is dsize
typedef struct Packet_t{
	Header head;
	void *data;
	size_t size;
	size_t dsize;
} *Packet;

//Init
Packet initPacketRTP(u_int32_t,u_int32_t,int8_t,void*,size_t);

//Free
void freePacketRTP(Packet);

//Misc
int16_t checksumRTP(Packet);
size_t readDataSizeRTP(void*);
Packet readPacketRTP(void*);

//Diag
void printDiagsPacketRTP(Packet);
void printDiagsHeaderRTP(Header);

#endif
