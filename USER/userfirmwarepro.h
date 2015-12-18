#ifndef __USER_FIRMWARE_PRO_H__
#define __USER_FIRMWARE_PRO_H__
#include "mbed.h"
#include "rtos.h"

#define BYTE unsigned char
#define UINT32 unsigned int
#define UINT unsigned int
#define DWORD unsigned long

#define BAG_SIZE 512//数据包尺寸
#define UC_GET_VER 0xC0//获取版本号
#define UC_WRITE_FLASH 0xC1//写FLASH
#define UC_RUN_CODE 0xC2//切换执行代码(IAP切换到用户代码,用户代码切换到IAP)

class userfwpro {
public:
	userfwpro(Serial *serial);

	~userfwpro();

int getUserVersion();

int sendUserFirmware(char *pbuf, int len, int fileLength);

int runUserFirmware();

private:
	void rxCallback();
	int CheckCode(BYTE dat[], int len);

	Serial *userCom;
	BYTE recvMsg[1024];
	BYTE buf[1024];//缓存
	BYTE tbuf[4];//暂时保存
	UINT pos;
	UINT32 address;//地址
	DWORD a;//计数
	DWORD retry;//重试
};

#endif
