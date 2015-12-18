#ifndef __USER_FIRMWARE_PRO_H__
#define __USER_FIRMWARE_PRO_H__
#include "mbed.h"
#include "rtos.h"

#define BYTE unsigned char
#define UINT32 unsigned int
#define UINT unsigned int
#define DWORD unsigned long

#define BAG_SIZE 512//���ݰ��ߴ�
#define UC_GET_VER 0xC0//��ȡ�汾��
#define UC_WRITE_FLASH 0xC1//дFLASH
#define UC_RUN_CODE 0xC2//�л�ִ�д���(IAP�л����û�����,�û������л���IAP)

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
	BYTE buf[1024];//����
	BYTE tbuf[4];//��ʱ����
	UINT pos;
	UINT32 address;//��ַ
	DWORD a;//����
	DWORD retry;//����
};

#endif
