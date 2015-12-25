#ifndef __REMOTE_UPDATE_H__
#define __REMOTE_UPDATE_H__
#include "mbed.h"
#include "rtos.h"
#include "cantcoap.h"
#include "bsd_socket.h"
#include "MbedJSONValue.h"

#define REMOTE_COAP_SERVER_IP "123.57.235.186"
#define REMOTE_COAP_SERVER_PORT 5683

#define DEVICE_MAGIC_ID	0xEE3355AA
#define DEVICE_TYPE	"T1"
#define DEVICE_SOFTWARE_VERSION	"V1.0"
#define DEVICE_HARDWARE_VERSION "H1"
#define DEVICE_FIRMWARE_VERSION	"F1.0"
#define USER_DEVICE_TYPE	"UT1"
#define USER_DEVICE_SOFTWARE_VERSION	"UV1.0"
#define USER_DEVICE_HARDWARE_VERSION	"UH1"

#define SUCCESS	"success"
#define MSG	"msg"
#define ALREADY_REGISTERED	"already registered!"
#define COMMAND	"command"
#define UPDATE	"update"
#define TYPE	"type"
#define NEWVERSIONNO	"newVersionNo"
#define FILENAMES	"fileNames"

#define u32	unsigned int

#define BUFLEN 256
#define BYTEPAYLOADLEN 1024
#define RECVBUFLEN 1096

#define USER_IMG_MAP_BUF_CLEAR_FLAG 0xFFFFFFFF
#define USER_IMG_MAP_BUF_VAILE_FLAG	0x04030201
#define USER_IMG_MAP_BUF_FLAG_LEN	0x04	//4bytes buf flag, 0xFFFFFFFF is alread erased.
#define USER_IMG_MAP_BUF_START	0x00		//The user image start address in flash.
#define USER_IMG_MAP_BUF_SIZE	0x10000		//64Kbytes, The user iamge buffer size
#define USER_IMG_MAP_BLOCK_SIZE	0x8000		//32Kbytes, The flash block size

enum DeviceUpdateStep{DEVICE_UPDATE_IDELING = 0x20, DEVICE_UPDATE_COMMAND,
	USER_DEVICE_UPDATE_ONGOING, USER_DEVICE_UPDATE_COMPLETE,
	MYSELF_DEVICE_UPDATE_ONGOING, MYSELF_DEVICE_UPDATE_COMPLETE};

#define HDR_NAME_4CHAR_LEN 4
#define HDR_NAME_8CHAR_LEN 8
/* 64bytes stm32f411 header id */
struct stm32f411xx_baseboard_id {
    unsigned int  magic;

	char devType[HDR_NAME_4CHAR_LEN];
	char softVerNo[HDR_NAME_8CHAR_LEN];
	char hardVerNo[HDR_NAME_4CHAR_LEN];
	char firmwareNo[HDR_NAME_8CHAR_LEN];

	char userDevType[HDR_NAME_4CHAR_LEN];
	char userSoftVerNo[HDR_NAME_8CHAR_LEN];
	char userHardVerNo[HDR_NAME_4CHAR_LEN];
};

#define FILENAME_MAX_LEN 128
typedef struct{
	int type;
	char newVersionNo[HDR_NAME_8CHAR_LEN];
	char fileNames[4][FILENAME_MAX_LEN];
	int fileLength;
	char *byteStream;
} GetUpdateinfo;

extern Queue<char, 1> queue;
extern Queue<char, 1> redial;
extern char pppDialingSuccessFlag;
extern bool pppRedialingSuccessFlag;
extern char userFirmwareSendCompleteFlag;
extern GetUpdateinfo updateinfo;
extern struct stm32f411xx_baseboard_id header;
extern I2C i2c;		 //I2C1; SDA, SCL for i2c eeprom
extern Ticker NetLED_ticker;
extern Ticker DataLED_ticker;

int deviceRegister(int sockfd);

int deviceStatusReport(int sockfd, char status);

int deviceGetUpdateinfo(int sockfd);

int deviceGetSaveUpdatefile(int sockfd);

bool connectToSocketUDP(char *, int, int *);

void startRemoteUpdate(void);

int updateUserVersionNo(char *version);

void toggle_NetLed(void);

void toggle_DataLed(void);
#endif
