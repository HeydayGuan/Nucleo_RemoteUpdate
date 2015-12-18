#include "remoteupdate.h"
#include "HuaweiUSBCDMAModem.h"
#include "flash25spi.h"
#include "userfirmwarepro.h"

#define MY_DEBUG 0

#define MODEM_APN "CMNET"
#define MODEM_USERNAME NULL
#define MODEM_PASSWORD NULL

Serial userCom(PA_9, PA_10); //UART1; tx, rx
Serial pc(USBTX, USBRX); 	 //UART2; tx, rx
I2C    i2c(PB_3, PB_10);	 //I2C1; SDA, SCL for i2c eeprom

Queue<char, 1> queue;
char userFirmwareSendCompleteFlag;
struct stm32f411xx_baseboard_id header;

void pppSurfing(void const*) {
	startRemoteUpdate(&queue);
}

void userFirmwareSend(void const*) {
	char readBuf[BAG_SIZE];//»º´æ
	SPI spi(PC_3, PC_2, PB_13);   // mosi, miso, sclk
	spi.format(8, 0);
	spi.frequency(16 * 1000 * 1000);
	flash25spi w25q64(&spi, PB_12);

	Serial userCom(PA_9, PA_10); //UART1; tx, rx
	DigitalOut userLed(PA_0, 1);

	printf("Start userFirmwareSend thread.........\n");
	while (true) {
		userfwpro userFWPro(&userCom);
		osEvent evt = queue.get();
		if (evt.status == osEventMessage) {
			char *ch = (char *)evt.value.p;
			printf("Get osEventMessage value id is %c\n", *ch);
		}

		while (userFWPro.getUserVersion()) {
			userLed = !userLed;
			Thread::wait(500);
		}

		UINT32 blockAddr = USER_IMG_MAP_BUF_CLEAR_FLAG;
		for (int i = 0; i < 2; i++) {
			int bufFlagValue;
			if (w25q64.read(USER_IMG_MAP_BUF_START + i * USER_IMG_MAP_BUF_SIZE, 
				USER_IMG_MAP_BUF_FLAG_LEN, (char *)&bufFlagValue) == false) {
				INFO("Read user image bufFlagValue from flash failure");
				continue;
			}
			printf("block%d, blockAddr[0x%08X], bufFlagValue[0x%08X].........\n", i,
					USER_IMG_MAP_BUF_START + i * USER_IMG_MAP_BUF_SIZE, bufFlagValue);
			if (bufFlagValue == USER_IMG_MAP_BUF_VAILE_FLAG) {
				if (i == 0) {
					blockAddr = USER_IMG_MAP_BUF_START + USER_IMG_MAP_BUF_FLAG_LEN;
				} else if (i == 1) {
					blockAddr = USER_IMG_MAP_BUF_START + USER_IMG_MAP_BUF_SIZE + USER_IMG_MAP_BUF_FLAG_LEN;
				}
				break;
			}
		}
		if (blockAddr == USER_IMG_MAP_BUF_CLEAR_FLAG) {
			INFO("No valid flash is available .");
			return;
		}
		for (int j = 0; j < updateinfo.fileLength/BAG_SIZE + 2; j++) {
			if (w25q64.read(blockAddr + j * BAG_SIZE, BAG_SIZE, readBuf) == false) {
				INFO("Read user image bytestreams from flash failure");
				continue;			
			}
			if (userFWPro.sendUserFirmware(readBuf, BAG_SIZE, updateinfo.fileLength)) {
				INFO("Send user firmware failure");
				continue;
			}
			printf("...");
		}

		if (userFWPro.runUserFirmware()) {
			INFO("Run user firmware failure\n");
			continue;
		} else {
			userFirmwareSendCompleteFlag = 1;
		}
	}
}

/*
 * Read header information from EEPROM into global structure.
 */
static int read_eeprom(struct stm32f411xx_baseboard_id *header) {
	const int EEPROM_MEM_ADDR = 0xA0;
	const int ntests = 10;
    int i2c_stat = 0;

	// Read the header from eeprotm using i2c
	for (int i = 0; i < ntests; i++) {
		//write the address you want to read
		char data[] = { 0, 0 };
		if ((i2c_stat = i2c.write(EEPROM_MEM_ADDR, data, 2, true)) != 0) {
			printf("Test %d failed at write, i2c_stat is 0x%02X\r\n", i, i2c_stat);
			continue;
		}

		/* read the eeprom using i2c */
		if ((i2c_stat = i2c.read(EEPROM_MEM_ADDR, (char *)header, sizeof(struct stm32f411xx_baseboard_id))) != 0) {
			printf("Test %d failed at read, i2c_stat is 0x%02X\r\n", i, i2c_stat);
			continue;
		}
		break;
	}

#if MY_DEBUG
	if (header->magic != 0x04030201) {
#else
	if (header->magic != DEVICE_MAGIC_ID) {
#endif
		header->magic = DEVICE_MAGIC_ID;
		strcpy(header->devType, DEVICE_TYPE);
		strcpy(header->softVerNo, DEVICE_SOFTWARE_VERSION);
		strcpy(header->hardVerNo, DEVICE_HARDWARE_VERSION);
		strcpy(header->firmwareNo, DEVICE_FIRMWARE_VERSION);

		strcpy(header->userDevType, USER_DEVICE_TYPE);
		strcpy(header->userSoftVerNo, USER_DEVICE_SOFTWARE_VERSION);
		strcpy(header->userHardVerNo, USER_DEVICE_HARDWARE_VERSION);

		return -1;
	}

	return 0;
}
/*
 * Write header information from EEPROM into global structure.
 */
static int write_eeprom(struct stm32f411xx_baseboard_id *header)
{
	const int EEPROM_MEM_ADDR = 0xA0;
	const int ntests = 1;
    int i2c_stat = 0;
	const int i2c_delay_us = 5000;	//delay 5ms

	char *data = (char *)malloc(sizeof(struct stm32f411xx_baseboard_id) + 2);
	memset(data, 0, sizeof(struct stm32f411xx_baseboard_id) + 2);
	memcpy(&data[2], header, sizeof(struct stm32f411xx_baseboard_id));

	// Write stm32f411 header data to eeprom using i2c
	for (int i = 0; i < ntests; i++) {
		if ((i2c_stat = i2c.write(EEPROM_MEM_ADDR, data, sizeof(struct stm32f411xx_baseboard_id) + 2)) != 0) {
			printf("Unable to write data to EEPROM (i2c_stat = 0x%02X), aborting\r\n", i2c_stat);
			continue;
		}

		// us delay if specified, Notice : This is necessary
		if (i2c_delay_us != 0)
			wait_us(i2c_delay_us);

		// ACK polling (assumes write will be successful eventually)
		while (i2c.write(EEPROM_MEM_ADDR, NULL, 0) != 0) ;

		free(data);
		return 0;
	}

	free(data);
	return -1;
}

int main() {
    DBG_INIT();
    DBG_SET_SPEED(115200);
    DBG_SET_NEWLINE("\r\n");

	INFO("Start the Nucleo Remote Update main process......");

	// Read and identify the stm32f4 board header id from eeprom
	{
		const int i2c_freq_hz = 400000;
		i2c.frequency(i2c_freq_hz);
		printf("I2C: I2C Frequency: %d Hz\r\n", i2c_freq_hz);

		if (read_eeprom(&header) < 0) {
			INFO("Could not get board ID, the is the first identify the board?");
			if (write_eeprom(&header) < 0) {
				INFO("Initialization eeprom failure.");
				return -1;
			}
		}
	}

	// connect to cellular network
	HuaweiUSBCDMAModem modem(NC, true);
    if (modem.connect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD) != OK) {
		INFO("Could connect the mobile gprs server, please check the Huawei modem or sim card.");
		return -2;
	}

	Thread pppSurfingTask(pppSurfing, NULL, osPriorityNormal, 1024 * 8);

	Thread userFirmwareSendTask(userFirmwareSend, NULL, osPriorityNormal, 1024 * 10);

	DigitalOut mcuLed(LED1);
	while(1)
	{
		mcuLed = !mcuLed;
		Thread::wait(1000);
	}

    return 0;
}
