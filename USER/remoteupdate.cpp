#include "mbed.h"
#include "rtos.h"
#include "remoteupdate.h"

static short messageID;
static char deviceUniqId[25];
static char updateCommandFlag;
static GetUpdateinfo updateinfo;

int deviceRegister(int sockfd) {
    // construct CoAP packet
    CoapPDU *pdu = new CoapPDU();
    pdu->setVersion(1);
    pdu->setType(CoapPDU::COAP_CONFIRMABLE);
    pdu->setCode(CoapPDU::COAP_POST);
    pdu->setToken((uint8_t*)"\3\2\1\1",4);
	pdu->setURI((char*)"register", 8);

	char buffer[BUFLEN];
	MbedJSONValue json;
	json["devType"] = "T1";
	json["firmwareNo"] = "F1";
	json["softVerNo"] = DEVICE_SOFTWARE_VERSION;
	json["userDevType"] = "UT1";
	json["userSoftVerNo"] = "US2";
	json["mobile"] = "15811112222";
	json["userHardVerNo"] = "UH1";
	json["hardVerNo"] = DEVICE_HARDWARE_VERSION;
	json["devUniqId"] = (string)deviceUniqId;

	string jsonStr = json.serialize();
	printf("%s : serialized content = %s\r\n" , __func__, jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();
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

		printf("sockfd = %d, getPDUPointer() = %p, getPDULength = %d\n", sockfd, pdu->getPDUPointer(), pdu->getPDULength());
        // send packet 
        ret = send(sockfd,pdu->getPDUPointer(),pdu->getPDULength(),0);
        if(ret!=pdu->getPDULength()) {
            INFO("Error sending packet.");
			pdu->reset();
            perror(NULL);
            return -2;
        }
        INFO("Packet sent");

        // receive response
        ret = recvfrom(sockfd,&buffer,BUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
        if(ret==-1) {
            INFO("Error receiving data");
			pdu->reset();
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
			pdu->reset();
            return -4;
        }
        INFO("Valid CoAP PDU received");
        recvPDU->printHuman();

		MbedJSONValue recvJson;
		parse(recvJson, (char *)recvPDU->getPayloadPointer());
		if (recvJson[SUCCESS].get<bool>() ||
			strcmp(recvJson[MSG].get<string>().c_str(), ALREADY_REGISTERED) == 0) {
			pdu->reset();
			return 0;
		} else
			tryNum--;

        Thread::wait(1000);
    } while (tryNum);

	pdu->reset();
	return -1;
}

int deviceStatusReport(int sockfd, char status) {
    // construct CoAP packet
    CoapPDU *pdu = new CoapPDU();
    pdu->setVersion(1);
    pdu->setType(CoapPDU::COAP_CONFIRMABLE);
    pdu->setCode(CoapPDU::COAP_POST);
    pdu->setToken((uint8_t*)"\3\2\1\1",4);
	pdu->setURI((char*)"deviceStatus", 12);

	char buffer[BUFLEN];
	MbedJSONValue json;
	json["devUniqId"] = (string)deviceUniqId;
	json["status"] =  status;

	time_t tt = time(0);
	printf("\rtime is %u......\n", tt);
	char str[16];
	sprintf(str, "%u", tt);
	json["timestamp"] = str;

	string jsonStr = json.serialize();
	printf("\r%s : serialized content = %s\r\n" , __func__, jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();
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

		printf("\rsockfd = %d, getPDUPointer() = %p, getPDULength = %d\n", sockfd, pdu->getPDUPointer(), pdu->getPDULength());
        // send packet 
        ret = send(sockfd,pdu->getPDUPointer(),pdu->getPDULength(),0);
        if(ret!=pdu->getPDULength()) {
            INFO("Error sending packet.");
            perror(NULL);
			pdu->reset();
            return -2;
        }
        INFO("Packet sent");

        // receive response
        ret = recvfrom(sockfd,&buffer,BUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
        if(ret==-1) {
            INFO("Error receiving data");
			pdu->reset();
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
			pdu->reset();
            return -4;
        }
        INFO("Valid CoAP PDU received");
        recvPDU->printHuman();

		MbedJSONValue recvJson;
		parse(recvJson, (char *)recvPDU->getPayloadPointer());
		if (recvJson[SUCCESS].get<bool>()) {
			if (recvJson.hasMember(COMMAND) &&
				strcmp(recvJson[COMMAND].get<string>().c_str(), UPDATE) == 0)
				updateCommandFlag = 1;
			pdu->reset();
			return 0;
		} else
			tryNum--;
		
        Thread::wait(1000);
    } while (tryNum);
	
	pdu->reset();
	return -1;
}

int deviceGetUpdateinfo(int sockfd) {
    // construct CoAP packet
    CoapPDU *pdu = new CoapPDU();
    pdu->setVersion(1);
    pdu->setType(CoapPDU::COAP_CONFIRMABLE);
    pdu->setCode(CoapPDU::COAP_POST);
    pdu->setToken((uint8_t*)"\3\2\1\1",4);
	pdu->setURI((char*)"newVersion", 10);

	char buffer[BUFLEN];
	MbedJSONValue json;
	json["devUniqId"] = (string)deviceUniqId;

	string jsonStr = json.serialize();
	printf("%s : serialized content = %s\r\n" , __func__, jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();
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
            perror(NULL);
			pdu->reset();
            return -2;
        }
        INFO("\rPacket sent");

        // receive response
        ret = recvfrom(sockfd,&buffer,BUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
        if(ret==-1) {
            INFO("Error receiving data");
			pdu->reset();
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
			pdu->reset();
            return -4;
        }
        INFO("Valid CoAP PDU received");
        recvPDU->printHuman();

		MbedJSONValue recvJson;
		parse(recvJson, (char *)recvPDU->getPayloadPointer());
		if (recvJson[SUCCESS].get<bool>()) {
			//Get the filename and newVersionNo
			updateinfo.type = recvJson[TYPE].get<int>();
			strcpy(updateinfo.newVersionNo, recvJson[NEWVERSIONNO].get<string>().c_str());
			printf("type = %s, newVersionNo = %s.........\n", updateinfo.type?"User":"Myself", updateinfo.newVersionNo);
			for (int i = 0; i < recvJson[FILENAMES].size(); i++) {
				strcpy(updateinfo.fileNames[i], recvJson[FILENAMES][i].get<string>().c_str());
				printf("\rThe %d filename is %s.........\n", i, updateinfo.fileNames[i]);
			}
			pdu->reset();
			return 0;
		} else
			tryNum--;

        Thread::wait(1000);
    } while (tryNum);

	pdu->reset();
	return -1;
}

int deviceGetUpdatefile(int sockfd) {
	char buffer[BYTEBUFLEN];
	MbedJSONValue json;
	json["devUniqId"] = (string)deviceUniqId;
	json["type"] = updateinfo.type;
	json["fileName"] = updateinfo.fileNames[0];
	json["newVersionNo"] = updateinfo.newVersionNo;

	string jsonStr = json.serialize();
	printf("%s : serialized content = %s\r\n" , __func__, jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();

	int blockNum = 0;
	int ret = 0;
	int nextBlock = 0;

	// structure for getting address of incoming packets
	sockaddr_in fromAddr;
	socklen_t fromAddrLen = sizeof(struct sockaddr_in);
	memset(&fromAddr,0x00,fromAddrLen);

	do {
		if (blockNum%5 == 0) {
			deviceStatusReport(sockfd, '0');
		}
		// construct CoAP packet
		CoapPDU *pdu = new CoapPDU();
		pdu->setVersion(1);
		pdu->setType(CoapPDU::COAP_CONFIRMABLE);
		pdu->setCode(CoapPDU::COAP_POST);
		pdu->setToken((uint8_t*)"\3\2\1\1",4);
		pdu->setURI((char*)"downfile", 8);
		unsigned short blockValue = (blockNum++<<4) | 0x6;
		blockValue = ((blockValue&0xFF)<<8) | ((blockValue>>8)&0xFF);
		pdu->addOption(CoapPDU::COAP_OPTION_BLOCK2, 2, (uint8_t *)&blockValue);

		pdu->setPayload((uint8_t*)payload,strlen(payload));

		// set PDU parameters
		pdu->setMessageID(messageID++);

		// send packet 
		ret = send(sockfd,pdu->getPDUPointer(),pdu->getPDULength(),0);
		if(ret!=pdu->getPDULength()) {
			INFO("Error sending packet.");
			perror(NULL);
			pdu->reset();
			return  -2;
		}
		INFO("Packet sent");

		// receive response
		ret = recvfrom(sockfd,&buffer,BYTEBUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
		if(ret==-1) {
			INFO("Error receiving data");
			pdu->reset();
			return -3;
		}
		INFO("Received %d bytes from %s:%d",ret,inet_ntoa(fromAddr.sin_addr),ntohs(fromAddr.sin_port));

		if(ret>BYTEBUFLEN) {
			INFO("PDU too large to fit in pre-allocated buffer");
		}

		// reuse the below coap object
		CoapPDU *recvPDU = new CoapPDU((uint8_t*)buffer,BUFLEN,BUFLEN);
		// validate packet
		recvPDU->setPDULength(ret);
		if(recvPDU->validate()!=1) {
			INFO("Malformed CoAP packet");
			pdu->reset();
			return -4;
		}
		INFO("Valid CoAP PDU received");
		recvPDU->printHuman();

		nextBlock = recvPDU->getOptionsBlock2Next();
		recvPDU->reset();
		pdu->reset();
	} while (nextBlock);

	return 0;
}

sockaddr_in bindAddr,serverAddress;
bool connectToSocketUDP(char *ipAddress, int port, int *sockfd) {
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
  return true;
}

void startRemoteUpdate(void) {
	unsigned int mcuID[3] = {0};

	mcuID[0] = *(__IO u32*)(0x1FFF7A10);
	mcuID[1] = *(__IO u32*)(0x1FFF7A14);
	mcuID[2] = *(__IO u32*)(0x1FFF7A18);
	sprintf(deviceUniqId, "%08X%08X%08X", mcuID[2], mcuID[1], mcuID[0]+19);
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

	// register the device until register success
	int ret = 0;
	int tryNum = 5;
	do {	
		ret = deviceRegister(sockfd);
		Thread::wait(1000);
	} while(ret);

	printf("\rdeviceRegister is %s..........\n", ret == 0?"OK":"FAIL");
	// report the device status until
	while (true) {
		ret = deviceStatusReport(sockfd, '1');
		if (ret) {
			Thread::wait(1000);
			continue;
		}

		printf("\rdeviceStatusReport is %s, updateCmd is %d.............\n",
				ret == 0?"OK":"FAIL", updateCommandFlag);
		// check the update identify
		if (updateCommandFlag) {
			updateCommandFlag = 0;
			// check the device update information
			tryNum = 5;
			do {
				ret = deviceGetUpdateinfo(sockfd);
				Thread::wait(500);
			} while( ret && tryNum-- );
			if (ret || tryNum == 0)
				break;

			tryNum = 5;
			do {
				ret = deviceGetLoadUpdatefile(sockfd);
				Thread::wait(1000);				
			} while(ret && tryNum--);
			if (ret || tryNum == 0)
				break;
		}
			
		Thread::wait(3000);
	}

	return ;
}
