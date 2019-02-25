#ifndef RTP_H
#define RTP_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <netinet/ip.h>

#include "list.h"
#include "logger.h"
#include "pld.h"

#define ERROR_INIT 1
#define ERROR_MISC 2
#define ERROR_FREE 3

typedef enum StatusCode_t{
	SYN = 0x01, //Sync
	FIN = 0x02, //Finish
	ACK = 0x04, //Acknowledge
	CMP = 0x08 //Complete
} StatusCode;

typedef struct Socket_t{
	int fd;
	int connected;
	clock_t timeout;
	clock_t estRTT;
	clock_t devRTT;
	Logger logger;
	PLD pld;
	struct sockaddr_in localaddr;
	struct sockaddr_in peeraddr;
} *Socket;

extern size_t mssRTP;
extern unsigned int windowsizeRTP;

//Init
Socket initSocketRTP(sa_family_t,size_t,size_t,int);
void bindSocketRTP(sa_family_t,in_port_t,in_addr_t,Socket);
void connectSocketRTP(sa_family_t,in_port_t,in_addr_t,Socket);
void acceptSocketRTP(Socket);

//Free
void freeSocketRTP(Socket s);

//Send
void sendSocketRTP(void*,size_t,Socket);

//Recv
ssize_t recvSocketRTP(void*,size_t,Socket);

//Finish
void closeSocketRTP(Socket);
void awaitCloseSocketRTP(Socket);

//Misc
void errorRTP(char*,int);
void setSocketOptRTP(int,int,void*,socklen_t,Socket);

//Diag

#endif