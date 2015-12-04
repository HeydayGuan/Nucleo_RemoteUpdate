#ifndef __REMOTE_UPDATE_H__
#define __REMOTE_UPDATE_H__
#include "cantcoap.h"
#include "bsd_socket.h"
#include "MbedJSONValue.h"

#define REMOTE_COAP_SERVER_IP "123.57.235.186"
#define REMOTE_COAP_SERVER_PORT 5683

#define DEVICE_SOFTWARE_VERSION	"V1.1"
#define DEVICE_HARDWARE_VERSION "V1.0"

#define SUCCESS	"success"
#define MSG	"msg"
#define ALREADY_REGISTERED	"already registered!"
#define COMMAND	"command"
#define UPDATE	"update"
#define TYPE	"type"
#define NEWVERSIONNO	"newVersionNo"
#define FILENAMES	"fileNames"

#define u32	unsigned int

#define BUFLEN 1024
#define BYTEBUFLEN 1096

typedef struct{
	int type;
	char newVersionNo[24];
	char fileNames[8][64];
	char *byteStream;
} GetUpdateinfo;

int deviceRegister(int sockfd);

int deviceStatusReport(int sockfd, char status);

int deviceGetUpdateinfo(int sockfd);

int deviceGetLoadUpdatefile(int sockfd);

void startRemoteUpdate(void);

bool connectToSocketUDP(char *, int, int *);

#endif
