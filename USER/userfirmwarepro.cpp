#include <string.h>
#include "userfirmwarepro.h"

UINT32 mask = 0x20504149;

// This function is called when a character goes into the RX buffer.
void userfwpro::rxCallback() {
	recvMsg[pos++] = userCom->getc();
}

userfwpro::userfwpro(Serial *serial) {
	memset(recvMsg, 0, sizeof(recvMsg));
	memset(buf, 0, sizeof(buf));
	memset(tbuf, 0, sizeof(tbuf));
	pos = 0;
	address = 0;
	a = 0;
	retry = 0;

	userCom = serial;
	userCom->baud(57600);
	userCom->attach(this, &userfwpro::rxCallback, Serial::RxIrq);
}

userfwpro::~userfwpro() {
	userCom = NULL;
}

int userfwpro::getUserVersion()
{
	buf[0]=UC_GET_VER;//获取版本
	if(CheckCode(buf, 1)<0) {//通讯失败
		printf("Communication failure\n");
		return -1;
	} else {//正常
		buf[buf[1]+2]=0;
		if (strstr((char *)buf, "TheCut_IAP_V")) {
			printf("Get user version is %s\n", buf);
			return 0;
		} else {
			printf("Get user version failure[%s]\n", buf);
			return -2;
		}
	}
}

int userfwpro::sendUserFirmware(char *pbuf, int len, int fileLength)
{
	memcpy(&buf[7], pbuf, BAG_SIZE);
	{
		UINT32 *pb=(UINT32*)&buf[7];
		UINT32 temp;
		for(int a=0;a<(BAG_SIZE/sizeof(UINT32));a++) {
			temp=*(pb+a);
			*(pb+a)^=mask;
			mask=temp;
		}
	}
	if (a == 0) {
		memcpy(tbuf,&buf[11],4);//暂时保存
		memset(&buf[11],0xFF,4);//暂时保存
		address=0;			
	}
	{
		UINT32 *pb=(UINT32*)&buf[7];
		for(int a=(BAG_SIZE/sizeof(UINT32))-1;a>0;a--)
		{
			*(pb+a)^=*(pb+a-1);
		}
	}

	if(a + (fileLength%BAG_SIZE) < fileLength+BAG_SIZE) {
		buf[0]=UC_WRITE_FLASH;//写CHIP
		buf[1]=address/0x1000000;
		buf[2]=address/0x10000;
		buf[3]=address/0x100;
		buf[4]=address;
		buf[5]=BAG_SIZE/0x100;
		buf[6]=BAG_SIZE%0x100;
		if(CheckCode(buf,BAG_SIZE+7)<0) {//通讯失败
			printf("Communication failure\n");
		}
		else if(buf[7]==0) {//写FLASH错误
			printf("Write flash error\n");
		}
		else {//正常
			a+=BAG_SIZE;
			address+=BAG_SIZE;
			return 0;
		}
	} else {
		buf[0]=UC_WRITE_FLASH;//写CHIP
		buf[1]=0;
		buf[2]=0;
		buf[3]=0;
		buf[4]=4;
		buf[5]=0;
		buf[6]=4;
		memcpy(&buf[7],tbuf,4);//暂时保存
		if(CheckCode(buf,4+7)<0) {//通讯失败
			printf("Communication failure\n");
		}
		else if(buf[7]==0) {//写FLASH错误
			printf("Write flash error\n");
		}
		else
		{//正常
			return 0;
		}
	}

	return -1;
}

int userfwpro::runUserFirmware()
{
	buf[0]=UC_RUN_CODE;
	if(CheckCode(buf, 1)<0) {//通讯失败
		printf("Communication failure\n");
		return -1;
	}
	else if(buf[1]==0) {//正常
		printf("Run user firmware success\n");
		return 0;
	} else {
		printf("Finished the firmware burning, failed to start properly\n");
		return -2;
	}
}

int userfwpro::CheckCode(BYTE dat[], int len)
{
	int retry=3;
	do {
		BYTE buf[1024] = {0};
		buf[0]=0xF0;
		buf[1]=len/0x100;
		buf[2]=len%0x100;
		memcpy(&buf[3],dat,len);
		buf[len+3]=0;
		for(int a=0;a<(len+3);a++) {
			buf[len+3]+=buf[a];
		}
		memset(recvMsg, 0, sizeof(recvMsg));
		pos = 0;
		for (int i = 0; i < (len+4); i++)
			userCom->putc(buf[i]);
		Thread::wait(500);
		BYTE *pread=buf;
		int l;
		if(pos>0) {
			memcpy(pread, recvMsg, pos);
			pread += pos;
		}
		BYTE *ptemp=buf;
		while(ptemp<pread-3)
		{
			if(*ptemp==0xA0)
			{
				l=*(ptemp+1)*0x100+*(ptemp+2);
				if(pread-ptemp<(l+4))
					break;
				BYTE JY=0;
				BYTE *pt=ptemp+(l+3);
				while(--pt>=ptemp)
					JY+=*pt;
				if(JY==*(ptemp+l+3))
				{
					int l=*(ptemp+1)*0x100+*(ptemp+2);
					memcpy(dat,ptemp+3,l);
					return l;
				}
			}
			ptemp++;
		}
		memcpy(buf,ptemp,pread-ptemp);
		pread=buf+(pread-ptemp);
	} while (--retry);
	return -1;
}
