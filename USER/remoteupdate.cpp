#include "mbed.h"
#include "rtos.h"
#include "remoteupdate.h"
#include "flash25spi.h"
#include "userfirmwarepro.h"

static short messageID;
static char deviceUniqId[25];
static char updateCommandFlag;
static char recvBuffer[RECVBUFLEN];
GetUpdateinfo updateinfo;
char lostConnectionFlag = 0;

int deviceRegister(int sockfd) {
	char buffer[BUFLEN];
	MbedJSONValue json;
	json["devType"] = header.devType;
	json["softVerNo"] = header.softVerNo;
	json["hardVerNo"] = header.hardVerNo;
	json["firmwareNo"] = header.firmwareNo;

	json["userDevType"] = header.userDevType;
	json["userSoftVerNo"] = header.userSoftVerNo;
	json["userHardVerNo"] = header.userHardVerNo;

	json["mobile"] = "15811112222";
	json["devUniqId"] = (string)deviceUniqId;

	string jsonStr = json.serialize();
	printf("%s : serialized content = %s\r\n" , __func__, jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();
    // construct CoAP packet
    CoapPDU *pdu = new CoapPDU();
    pdu->setVersion(1);
    pdu->setType(CoapPDU::COAP_CONFIRMABLE);
    pdu->setCode(CoapPDU::COAP_POST);
    pdu->setToken((uint8_t*)"\3\2\1\1",4);
	pdu->setURI((char*)"register", 8);
	pdu->setPayload((uint8_t*)payload,strlen(payload));

	// structure for getting address of incoming packets
    sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(struct sockaddr_in);
    memset(&fromAddr,0x00,fromAddrLen);
	char tryNum = 5;
	int ret = 0;
	// send packets to server
    do {
        // set PDU parameters
        pdu->setMessageID(messageID++);

        // send packet 
        ret = send(sockfd,pdu->getPDUPointer(),pdu->getPDULength(),0);
        if(ret!=pdu->getPDULength()) {
            INFO("Error sending packet.");
			pdu->reset();
			delete pdu;
            perror(NULL);
            return -2;
        }
        INFO("Packet sent");

        // receive response
        ret = recvfrom(sockfd,&buffer,BUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
        if(ret==-1) {
            INFO("Error receiving data");
			pdu->reset();
			delete pdu;
            perror(NULL);
            return -3;
        }
        INFO("Received %d bytes from %s:%d",ret,inet_ntoa(fromAddr.sin_addr),ntohs(fromAddr.sin_port));

        if(ret>BUFLEN) {
            INFO("PDU too large to fit in pre-allocated buffer");
			pdu->reset();
			delete pdu;
            continue;
        }

		// reuse the below coap object
		CoapPDU *recvPDU = new CoapPDU((uint8_t*)buffer,BUFLEN,BUFLEN);
        // validate packet
        recvPDU->setPDULength(ret);
        if(recvPDU->validate()!=1) {
            INFO("Malformed CoAP packet");
			recvPDU->reset();
			delete recvPDU;
			pdu->reset();
			delete pdu;
            return -4;
        }
        INFO("Valid CoAP PDU received");
        recvPDU->printHuman();

		MbedJSONValue recvJson;
		parse(recvJson, (char *)recvPDU->getPayloadPointer());
		if (recvJson.hasMember(SUCCESS) && (recvJson[SUCCESS].get<bool>() ||
			strcmp(recvJson[MSG].get<string>().c_str(), ALREADY_REGISTERED) == 0)) {
			recvPDU->reset();
			delete recvPDU;
			pdu->reset();
			delete pdu;
			return 0;
		} else
			tryNum--;

		recvPDU->reset();
		delete recvPDU;
		Thread::wait(3000);
	} while (tryNum);

	pdu->reset();
	delete pdu;
	return -1;
}

int deviceStatusReport(int sockfd, char status) {
	char buffer[BUFLEN];
	MbedJSONValue json;
	json["devUniqId"] = (string)deviceUniqId;
	json["status"] =  status;

	time_t tt = time(0);
	char str[16];
	sprintf(str, "%u", tt);
	json["timestamp"] = str;

	string jsonStr = json.serialize();
	printf("\r%s : serialized content = %s\r\n" , __func__, jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();
    // construct CoAP packet
    CoapPDU *pdu = new CoapPDU();
    pdu->setVersion(1);
    pdu->setType(CoapPDU::COAP_CONFIRMABLE);
    pdu->setCode(CoapPDU::COAP_POST);
    pdu->setToken((uint8_t*)"\3\2\1\1",4);
	pdu->setURI((char*)"deviceStatus", 12);
	pdu->setPayload((uint8_t*)payload,strlen(payload));

	// structure for getting address of incoming packets
    sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(struct sockaddr_in);
    memset(&fromAddr,0x00,fromAddrLen);
	char tryNum = 5;
	int ret = 0;
	// send packets to server
    do {
        // set PDU parameters
        pdu->setMessageID(messageID++);

        // send packet 
        ret = send(sockfd,pdu->getPDUPointer(),pdu->getPDULength(),0);
        if(ret!=pdu->getPDULength()) {
            INFO("Error sending packet.");
			pdu->reset();
			delete pdu;
            perror(NULL);
			lostConnectionFlag = 1;
			NetLED_ticker.detach();
			NetLED_ticker.attach(&toggle_NetLed, 3);	//net is connect
            return -2;
        }
        INFO("Packet sent");

        // receive response
        ret = recvfrom(sockfd,&buffer,BUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
        if(ret==-1) {
            INFO("Error receiving data");
			pdu->reset();
			delete pdu;
            perror(NULL);
			lostConnectionFlag = 1;
			NetLED_ticker.detach();
			NetLED_ticker.attach(&toggle_NetLed, 3);	//net is connect
            return -3;
        }
        INFO("Received %d bytes from %s:%d",ret,inet_ntoa(fromAddr.sin_addr),ntohs(fromAddr.sin_port));

        if(ret>BUFLEN) {
            INFO("PDU too large to fit in pre-allocated buffer");
			pdu->reset();
			delete pdu;
            continue;
        }

		// reuse the below coap object
		CoapPDU *recvPDU = new CoapPDU((uint8_t*)buffer,BUFLEN,BUFLEN);
        // validate packet
        recvPDU->setPDULength(ret);
        if(recvPDU->validate()!=1) {
            INFO("Malformed CoAP packet");
			recvPDU->reset();
			delete recvPDU;
			pdu->reset();
			delete pdu;
            return -4;
        }
        INFO("Valid CoAP PDU received");
        recvPDU->printHuman();

		MbedJSONValue recvJson;
		parse(recvJson, (char *)recvPDU->getPayloadPointer());
		if (recvJson.hasMember(SUCCESS) && recvJson[SUCCESS].get<bool>()) {
			if (recvJson.hasMember(COMMAND) &&
				strcmp(recvJson[COMMAND].get<string>().c_str(), UPDATE) == 0)
				updateCommandFlag = 1;
			recvPDU->reset();
			delete recvPDU;
			pdu->reset();
			delete pdu;
			if (lostConnectionFlag == 1) {
				lostConnectionFlag = 0;
				NetLED_ticker.detach();
				NetLED_ticker.attach(&toggle_NetLed, 0.5);	//net is again connect
			}
			return 0;
		} else
			tryNum--;
			
		recvPDU->reset();
		delete recvPDU;
        Thread::wait(3000);
    } while (tryNum);
	
	pdu->reset();
	delete pdu;
	return -1;
}

int deviceGetUpdateinfo(int sockfd) {
	char buffer[BUFLEN];
	MbedJSONValue json;
	json["devUniqId"] = (string)deviceUniqId;

	string jsonStr = json.serialize();
	printf("%s : serialized content = %s\r\n" , __func__, jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();
    // construct CoAP packet
    CoapPDU *pdu = new CoapPDU();
    pdu->setVersion(1);
    pdu->setType(CoapPDU::COAP_CONFIRMABLE);
    pdu->setCode(CoapPDU::COAP_POST);
    pdu->setToken((uint8_t*)"\3\2\1\1",4);
	pdu->setURI((char*)"newVersion", 10);
	pdu->setPayload((uint8_t*)payload,strlen(payload));

	// structure for getting address of incoming packets
    sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(struct sockaddr_in);
    memset(&fromAddr,0x00,fromAddrLen);
	int tryNum = 5;
	int ret = 0;
	// send packets to server
    do {
        // set PDU parameters
        pdu->setMessageID(messageID++);

        // send packet 
        ret = send(sockfd,pdu->getPDUPointer(),pdu->getPDULength(),0);
        if(ret!=pdu->getPDULength()) {
            INFO("Error sending packet.");
 			pdu->reset();
			delete pdu;
			perror(NULL);
            return -2;
        }
        INFO("Packet sent");

        // receive response
        ret = recvfrom(sockfd,&buffer,BUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
        if(ret==-1) {
            INFO("Error receiving data");
 			pdu->reset();
			delete pdu;
			perror(NULL);
            return -3;
        }
        INFO("Received %d bytes from %s:%d",ret,inet_ntoa(fromAddr.sin_addr),ntohs(fromAddr.sin_port));

        if(ret>BUFLEN) {
            INFO("PDU too large to fit in pre-allocated buffer");
            continue;
        }

		// reuse the below coap object
		CoapPDU *recvPDU = new CoapPDU((uint8_t*)buffer,BUFLEN,BUFLEN);
        // validate packet
        recvPDU->setPDULength(ret);
        if(recvPDU->validate()!=1) {
            INFO("Malformed CoAP packet");
			recvPDU->reset();
			delete recvPDU;
			pdu->reset();
			delete pdu;
            return -4;
        }
        INFO("Valid CoAP PDU received");
        recvPDU->printHuman();

		MbedJSONValue recvJson;
		parse(recvJson, (char *)recvPDU->getPayloadPointer());
		if (recvJson.hasMember(SUCCESS) && recvJson[SUCCESS].get<bool>()) {
			memset(&updateinfo, 0, sizeof(GetUpdateinfo));
			//Get the filename and newVersionNo
			if (recvJson.hasMember(TYPE) && recvJson.hasMember(NEWVERSIONNO)) {				
				updateinfo.type = recvJson[TYPE].get<int>();
				if (strlen(recvJson[NEWVERSIONNO].get<string>().c_str()) <= HDR_NAME_8CHAR_LEN)
					strcpy(updateinfo.newVersionNo, recvJson[NEWVERSIONNO].get<string>().c_str());
//				printf("type = %s, newVersionNo = %s.........\n", updateinfo.type?"User":"Myself", updateinfo.newVersionNo);
				for (int i = 0; i < recvJson[FILENAMES].size(); i++) {
					if (strlen(recvJson[FILENAMES][i].get<string>().c_str()) <= FILENAME_MAX_LEN)
						strcpy(updateinfo.fileNames[i], recvJson[FILENAMES][i].get<string>().c_str());
//					printf("\rThe %d filename is %s.........\n", i, updateinfo.fileNames[i]);
				}
				recvPDU->reset();
				delete recvPDU;
				pdu->reset();
				delete pdu;
				return 0;
			} else {			
				recvPDU->reset();
				delete recvPDU;
				pdu->reset();
				delete pdu;
				return -5;
			}
		} else
			tryNum--;

		recvPDU->reset();
		delete recvPDU;
        Thread::wait(3000);
    } while (tryNum);

	pdu->reset();
	delete pdu;
	return -1;
}

int deviceGetSaveUpdatefile(int sockfd, int type) {
	DataLED_ticker.detach();
	DataLED_ticker.attach(&toggle_DataLed, 0.1);
	MbedJSONValue json;
	json["devUniqId"] = (string)deviceUniqId;
	json["type"] = updateinfo.type;
	json["fileName"] = updateinfo.fileNames[0];
	json["newVersionNo"] = updateinfo.newVersionNo;

	string jsonStr = json.serialize();
	printf("%s : serialized content = %s\r\n" , __func__, jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();

	int ret = 0;
	int coapBlockNum = 0;
	int nextCoapBlock = 0;

	// structure for getting address of incoming packets
	sockaddr_in fromAddr;
	socklen_t fromAddrLen = sizeof(struct sockaddr_in);
	memset(&fromAddr,0x00,fromAddrLen);

	SPI spi(PC_3, PC_2, PB_13);   // mosi, miso, sclk
	spi.format(8, 0);
	spi.frequency(16 * 1000 * 1000);
	flash25spi w25q64(&spi, PB_12);

	UINT32 blockAddr = USER_IMG_MAP_BUF_CLEAR_FLAG;
	UINT32 brotherBlockAddr = USER_IMG_MAP_BUF_CLEAR_FLAG;

	for (int i = 0; i < 2; i++) {
		int bufFlagValue;
		if (w25q64.read(USER_IMG_MAP_BUF_START + i * USER_IMG_MAP_BUF_SIZE, 
				USER_IMG_MAP_BUF_FLAG_LEN, (char *)&bufFlagValue) == false) {
			INFO("Read user image bufFlagValue from flash failure");
			continue;
		}
//		printf("block%d, blockAddr[0x%08X], bufFlagValue[0x%08X].........\n", i,
//				USER_IMG_MAP_BUF_START + i * USER_IMG_MAP_BUF_SIZE, bufFlagValue);
		if (bufFlagValue == USER_IMG_MAP_BUF_CLEAR_FLAG) {
			int bufFlag = USER_IMG_MAP_BUF_VAILE_FLAG;
			w25q64.write(USER_IMG_MAP_BUF_START + i * USER_IMG_MAP_BUF_SIZE, USER_IMG_MAP_BUF_FLAG_LEN, (char *)&bufFlag);
			if (i == 0) {
				blockAddr = USER_IMG_MAP_BUF_START + USER_IMG_MAP_BUF_FLAG_LEN;
				brotherBlockAddr = USER_IMG_MAP_BUF_START + USER_IMG_MAP_BUF_SIZE;
			} else if (i == 1) {
				blockAddr = USER_IMG_MAP_BUF_START + USER_IMG_MAP_BUF_SIZE + USER_IMG_MAP_BUF_FLAG_LEN;
				brotherBlockAddr = USER_IMG_MAP_BUF_START;
			}
			for (int i = 0; i < USER_IMG_MAP_BUF_SIZE/USER_IMG_MAP_BLOCK_SIZE; i++)
				w25q64.clearBlock(brotherBlockAddr + i*USER_IMG_MAP_BLOCK_SIZE);
			break;
		}
	}
	if (blockAddr == USER_IMG_MAP_BUF_CLEAR_FLAG || brotherBlockAddr == USER_IMG_MAP_BUF_CLEAR_FLAG) {
		INFO("No valid flash is available");
		return  -1;
	}

	int tryNum1 = 5, tryNum2 = 5;
	do {
		if (coapBlockNum%8 == 0)
			deviceStatusReport(sockfd, type);

		// construct CoAP packetsss
		CoapPDU *pdu = new CoapPDU();
		pdu->setVersion(1);
		pdu->setType(CoapPDU::COAP_CONFIRMABLE);
		pdu->setCode(CoapPDU::COAP_POST);
		pdu->setToken((uint8_t*)"\3\2\1\1",4);
		pdu->setURI((char*)"downfile", 8);
		unsigned short coapBlockValue = (coapBlockNum<<4) | 0x6;
		coapBlockValue = ((coapBlockValue&0xFF)<<8) | ((coapBlockValue>>8)&0xFF);
		pdu->addOption(CoapPDU::COAP_OPTION_BLOCK2, 2, (uint8_t *)&coapBlockValue);
		pdu->setPayload((uint8_t*)payload,strlen(payload));
		// set PDU parameters
		pdu->setMessageID(messageID++);

		// send packet 
		ret = send(sockfd,pdu->getPDUPointer(),pdu->getPDULength(),0);
		if(ret!=pdu->getPDULength()) {
			INFO("Error sending packet.");
			pdu->reset();
			delete pdu;
			perror(NULL);
			return  -2;
		}
		INFO("Packet sent");

		// receive response
		ret = recvfrom(sockfd,&recvBuffer,RECVBUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
		if(ret==-1) {
			INFO("Error receiving data");
			pdu->reset();
			delete pdu;
			perror(NULL);
			if (tryNum1--)
				continue;
			else
				return -3;
		}
		tryNum1 = 5;
		INFO("Received %d bytes from %s:%d",ret,inet_ntoa(fromAddr.sin_addr),ntohs(fromAddr.sin_port));

		if(ret>RECVBUFLEN) {
			pdu->reset();
			delete pdu;
			INFO("PDU too large to fit in pre-allocated buffer");
		}

		// reuse the below coap object
		CoapPDU *recvPDU = new CoapPDU((uint8_t*)recvBuffer,RECVBUFLEN,RECVBUFLEN);
		// validate packet
		recvPDU->setPDULength(ret);
		if(recvPDU->validate()!=1) {
			INFO("Malformed CoAP packet");
			recvPDU->reset();
			delete recvPDU;
			pdu->reset();
			delete pdu;
			return -4;
		}
#if 0
		INFO("Valid CoAP PDU received");
		recvPDU->printHuman();
#endif

		char *payload = (char *)recvPDU->getPayloadPointer();
		int payloadLen = recvPDU->getPayloadLength();
		if (payloadLen != BYTEPAYLOADLEN) {
			MbedJSONValue recvJson;
			parse(recvJson, (char *)recvPDU->getPayloadPointer());
			if (recvJson.hasMember(SUCCESS)) {
				printf("Recvfrom the payloadLen = %d,,,,,,,,,,,,\n", payloadLen);
				recvPDU->reset();
				delete recvPDU;
				pdu->reset();
				delete pdu;
				Thread::wait(1000);
				nextCoapBlock = 1;
				if (tryNum2--)
					continue;
				else
					return -5;
			}
		}
		tryNum2 = 5;
		updateinfo.fileLength += payloadLen;

		if (w25q64.write(blockAddr + coapBlockNum * 1024, payloadLen, payload) == false) {
			INFO("Error write the flash");
			recvPDU->reset();
			delete recvPDU;
			pdu->reset();
			delete pdu;
			return -5;
		}

		nextCoapBlock = recvPDU->getOptionsBlock2Next();
		recvPDU->reset();
		delete recvPDU;
		pdu->reset();
		delete pdu;
		coapBlockNum++;
	} while (nextCoapBlock);

	return 0;
}

bool connectToSocketUDP(char *ipAddress, int port, int *sockfd) {
  sockaddr_in bindAddr,serverAddress;

  *sockfd = -1;
  // create the socket
  if((*sockfd=socket(AF_INET,SOCK_DGRAM,0))<0) {
     DBG("Error opening socket");
     return false;
  }
  socklen_t sockAddrInLen = sizeof(struct sockaddr_in);
 
  // bind socket to 11111
  memset(&bindAddr,  0x00, sockAddrInLen);
  bindAddr.sin_family = AF_INET; // IP family
  bindAddr.sin_port = htons(port);
  bindAddr.sin_addr.s_addr = IPADDR_ANY; // 32 bit IP representation
  // call bind
  if(bind(*sockfd,(const struct sockaddr *)&bindAddr,sockAddrInLen)!=0) {
     DBG("Error binding socket");
     perror(NULL);
  }

  INFO("UDP socket created and bound to: %s:%d",inet_ntoa(bindAddr.sin_addr),ntohs(bindAddr.sin_port));
         
  // create the socket address
  memset(&serverAddress, 0x00, sizeof(struct sockaddr_in));
  serverAddress.sin_addr.s_addr = inet_addr(ipAddress);
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);

  // do socket connect
  //LOG("Connecting socket to %s:%d", inet_ntoa(serverAddress.sin_addr), ntohs(serverAddress.sin_port));
  if(connect(*sockfd, (const struct sockaddr *)&serverAddress, sizeof(serverAddress))<0) {
     shutdown(*sockfd,SHUT_RDWR);
     close(*sockfd);
     DBG("Could not connect");
     return false;
  }

  int timeout = 5000;		//timeout 5s
  setsockopt(*sockfd,SOL_SOCKET,SO_SNDTIMEO,(char *)&timeout, sizeof(timeout));
  setsockopt(*sockfd,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout, sizeof(timeout));

  return true;
}

int updateUserVersionNo(char *version) {
	const int EEPROM_MEM_ADDR = 0xA0;
	const int ntests = 3;
    int i2c_stat = 0;
	const int i2c_delay_us = 5000;	//delay 5ms


	char *data = (char *)malloc(HDR_NAME_8CHAR_LEN + 2);
	memset(data, 0, HDR_NAME_8CHAR_LEN + 2);
	int address = (UINT32)&header.userSoftVerNo - (UINT32)&header;
	data[0] = ((address&0xFF00) >> 8) & 0x00FF;
	data[1] = address&0x00FF;
	if (strlen(version) <= HDR_NAME_8CHAR_LEN)
		strcpy(&data[2], version);

	// Write stm32f411 header data to eeprom using i2c
	for (int i = 0; i < ntests; i++) {
		if ((i2c_stat = i2c.write(EEPROM_MEM_ADDR, data, HDR_NAME_8CHAR_LEN + 2)) != 0) {
			printf("Unable to write userVersionNo to EEPROM (i2c_stat = 0x%02X), aborting\r\n", i2c_stat);
			continue;
		}

		// us delay if specified, Notice : This is necessary
		if (i2c_delay_us != 0)
			wait_us(i2c_delay_us);

		// ACK polling (assumes write will be successful eventually)
		while (i2c.write(EEPROM_MEM_ADDR, NULL, 0) != 0) ;

		free(data);
		data = NULL;
		if (strlen(version) <= HDR_NAME_8CHAR_LEN)
			strcpy(header.userSoftVerNo, version);
		return 0;
	}

	free(data);
	data = NULL;
	return -1;
}

void startRemoteUpdate() {
	unsigned int mcuID[3] = {0};

	mcuID[0] = *(__IO u32*)(0x1FFF7A10);
	mcuID[1] = *(__IO u32*)(0x1FFF7A14);
	mcuID[2] = *(__IO u32*)(0x1FFF7A18);
	sprintf(deviceUniqId, "%08X%08X%08X", mcuID[2], mcuID[1]+1, mcuID[0]);
	INFO("The device uniqe ID is %s.", deviceUniqId);

    // structure for getting address of incoming packets
    sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(struct sockaddr_in);
    memset(&fromAddr,0x00,fromAddrLen);

    // setup socket to remote server
    int sockfd = NULL;
    if(!connectToSocketUDP((char *)REMOTE_COAP_SERVER_IP, REMOTE_COAP_SERVER_PORT, &sockfd)) {
       DBG("Error connecting to socket");
       return;
    }
    DBG("\"Connected\" to UDP socket");

	// register the device until register success or timeout 120s
	int ret = 0, retryNum = 120;
	do {
		ret = deviceRegister(sockfd);
		Thread::wait(1000);
	} while(ret && retryNum--);

	printf("\rdeviceRegister is %s..........\n", ret == 0?"OK":"FAIL");
	enum DeviceUpdateStep step = DEVICE_UPDATE_IDELING;
	char tryNum = 5;

	// report the device status until
	while (true) {
		switch (step) {
			case DEVICE_UPDATE_IDELING:
				// start the device status report
				tryNum = 5;
				do {
					ret = deviceStatusReport(sockfd, 1);
					if (ret)
						Thread::wait(3000);
				} while(ret && tryNum--);
				if (ret == 0) {
					printf("\rdeviceStatusReport is %s, updateCmd is %d.............\n", ret == 0?"OK":"FAIL", updateCommandFlag);
					// check the update identify
					if (updateCommandFlag) {
						updateCommandFlag = 0;
						step = DEVICE_UPDATE_COMMAND;
						continue;
					}
				}
				break;
			case DEVICE_UPDATE_COMMAND:
				// check the device update information
				tryNum = 5;
				do {
					ret = deviceGetUpdateinfo(sockfd);
					if (ret)
						Thread::wait(1000);
				} while(ret && tryNum--);
				if (ret == 0) {
					if (updateinfo.type == 1)
						step = USER_DEVICE_UPDATE_ONGOING;
					else if (updateinfo.type == 0)
						step = MYSELF_DEVICE_UPDATE_ONGOING;
					continue;
				}
				break;			
			case USER_DEVICE_UPDATE_ONGOING:
				tryNum = 5;
				do {
					ret = deviceGetSaveUpdatefile(sockfd, 2);
					if (ret)
						Thread::wait(1000);
				} while (ret && tryNum--);
				if (ret == 0) {
					char ch = 1;
					queue.put(&ch);
					step = USER_DEVICE_UPDATE_COMPLETE;
				}
				break;
			case USER_DEVICE_UPDATE_COMPLETE:
				// start the device status report
				tryNum = 5;
				do {
					ret = deviceStatusReport(sockfd, 2);
					if (ret)
						Thread::wait(1000);
				} while (ret && tryNum--);
				if (userFirmwareSendCompleteFlag == 1) {
					userFirmwareSendCompleteFlag = 0;
					if (updateUserVersionNo(updateinfo.newVersionNo) == 0)
						deviceRegister(sockfd);
					step = DEVICE_UPDATE_IDELING;
				}
				break;
			case MYSELF_DEVICE_UPDATE_ONGOING:
				break;
			case MYSELF_DEVICE_UPDATE_COMPLETE:
				break;
			default:
				step = DEVICE_UPDATE_IDELING;
				break;
		}
		Thread::wait(5000);
	}

	return ;
}
