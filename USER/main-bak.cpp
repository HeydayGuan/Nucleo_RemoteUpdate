////////////////////////////
/// Simple example making use of cantcoap library, just keeps sending packets to a server and prints the response.

#include "mbed.h"
#include "rtos.h"
#include "HuaweiUSBCDMAModem.h"
#include "cantcoap.h"
#include "bsd_socket.h"
#include "MbedJSONValue.h"
#include "remoteupdate.h"

#define MODEM_APN "CMNET"
#define MODEM_USERNAME NULL
#define MODEM_PASSWORD NULL

bool pppDialing(void) {
	INFO("Start the pppDialing thread,Thread ID is %d.\n", (int)Thread::gettid());
    HuaweiUSBCDMAModem modem(NC, true);

	Thread::wait(1000);
	// connect to cellular network
    int ret = modem.connect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD);
	if(ret)
    {
		ERR("PPP dialing failure, Could not connect!");
		return false;
    }

	return true;
}

void pppSurfing(void const*) {
	if (pppDialing() == false)
		return ;

	startRemoteUpdate();
}

int main() {
    DBG_INIT();
    DBG_SET_SPEED(115200);
    DBG_SET_NEWLINE("\r\n");

	INFO("Start the Nucleo Remote Update main process......");
	Thread pppSurfingTask(pppSurfing, NULL, osPriorityNormal, 1024 * 4);

	DigitalOut myled(LED1);
	while(1)
	{
		myled=!myled;
		Thread::wait(1000);  
	}

    return true;
}




////////////////////////////


/// Simple example making use of cantcoap library, just keeps sending packets to a server and prints the response.

#include "rtos.h"
#include "mbed.h"
#include "HuaweiUSBCDMAModem.h"
#include "cantcoap.h"
#include "bsd_socket.h"
#include "MbedJSONValue.h"

DigitalOut myled(LED1);

void fail(int code) {
   while(1) {
      myled = !myled;
      Thread::wait(100);
   }
}

//#define APN_GDSP
//#define APN_CONTRACT
#define APN_SCDMA

#ifdef APN_GDSP
   #define APN "ppinternetd.gdsp" 
   #define APN_USERNAME ""
   #define APN_PASSWORD ""
#endif

#ifdef APN_CONTRACT
   #define APN "internet" 
   #define APN_USERNAME "web"
   #define APN_PASSWORD "web"
#endif

#ifdef APN_SCDMA
   #define APN "cmnet" 
   #define APN_USERNAME ""
   #define APN_PASSWORD ""
#endif

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

int main() {
    DBG_INIT();
    DBG_SET_SPEED(115200);
    DBG_SET_NEWLINE("\r\n");

    // structure for getting address of incoming packets
    sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(struct sockaddr_in);
    memset(&fromAddr,0x00,fromAddrLen);
    
    // connect to cellular network
	HuaweiUSBCDMAModem modem(NC, true);
    modem.connect(APN,APN_USERNAME,APN_PASSWORD);

    // setup socket to remote server
    int sockfd = NULL;
//    if(!connectToSocketUDP("109.74.199.96", 5683, &sockfd)) {
    if(!connectToSocketUDP("123.57.235.186", 5683, &sockfd)) {
       DBG("Error connecting to socket");
       fail(1);
    }
    DBG("\"Connected\" to UDP socket");

    // construct CoAP packet
    CoapPDU *pdu = new CoapPDU();
    pdu->setVersion(1);
    pdu->setType(CoapPDU::COAP_CONFIRMABLE);
    pdu->setCode(CoapPDU::COAP_POST);
    pdu->setToken((uint8_t*)"\3\2\1\1",4);
    pdu->setURI((char*)"register",8);

    #define BUFLEN 128
    short messageID = 0;
    char buffer[BUFLEN];

	MbedJSONValue json;
	json["devType"] = "T1";
	json["firmwareNo"] = "F1";
	json["softVerNo"] = "S4";
	json["userDevType"] = "UT1";
	json["userSoftVerNo"] = "US2";
	json["mobile"] = "15811112222";
	json["userHardVerNo"] = "UH1";
	json["hardVerNo"] = "H1";
	json["devUniqId"] = "q1w2e3r4t5";

	string jsonStr = json.serialize();
	printf("serialized content = %s\r\n" ,  jsonStr.c_str());

	char *payload = (char *)jsonStr.c_str();
	pdu->setPayload((uint8_t*)payload,strlen(payload));

	int num = 3;
    // send packets to server
    while(num--) {
        // set PDU parameters
        pdu->setMessageID(messageID++);
    
        // send packet 
        int ret = send(sockfd,pdu->getPDUPointer(),pdu->getPDULength(),0);
        if(ret!=pdu->getPDULength()) {
            INFO("Error sending packet.");
            perror(NULL);
            fail(2);
        }
        INFO("Packet sent");

        // receive response
        ret = recvfrom(sockfd,&buffer,BUFLEN,0,(struct sockaddr *)&fromAddr,&fromAddrLen);
        if(ret==-1) {
            INFO("Error receiving data");
            fail(3);
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
            fail(4);
        }
        INFO("Valid CoAP PDU received");
        recvPDU->printHuman();
        
        Thread::wait(5000);
    }
	
    modem.disconnect();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////
#include "mbed.h"
#include "VodafoneIOModem.h"
#include "HTTPClient.h"

#define DBG_PORT_BAUD 460800
//#define DBG_PORT_BAUD 9600

Serial pc(USBTX, USBRX);
void test(void const*) 
{
	printf("VodafoneUSBModemHTTPClientTest\n");    
	VodafoneIOModem modem;
	HTTPClient http;
	char str[512];

	int ret = modem.connect("live.vodafone.com");
	if(ret)
	{
		printf("Could not connect\n");
		return;
	}else
		printf("Connected!\n");
	//GET data
	printf("Trying to fetch page...\n");
	ret = http.get("http://developer.mbed.org/media/uploads/donatien/hello.txt", str, 128);
	if (!ret)
	{
		printf("Page fetched successfully - read %d characters\n", strlen(str));
		printf("Result: %s\n", str);
	}
	else
	{
		printf("Error - ret = %d - HTTP return code = %d\n", ret, http.getHTTPResponseCode());
	}

//POST data
	HTTPMap map;
	HTTPText text(str, 512);
	map.put("Hello", "World");
	map.put("test", "1234");
	printf("Trying to post data...\n");
	ret = http.post("http://httpbin.org/post", map, &text);
	if (!ret)
	{
		printf("Executed POST successfully - read %d characters\n", strlen(str));
		printf("Result: %s\n", str);
	}
	else
	{
		printf("Error - ret = %d - HTTP return code = %d\n", ret, http.getHTTPResponseCode());
	}

	modem.disconnect();  
	while(1) {

	}
}

int main()
{
	pc.baud (DBG_PORT_BAUD);  
	Thread testTask(test, NULL, osPriorityNormal, 1024 * 4);
	DigitalOut led(LED1);
	while(1)
	{
		printf("xxxxxxxxxxxxxxxxxx\n");
		led=!led;
		Thread::wait(1000);  
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////
#include "mbed.h"
#include "HuaweiUSBCDMAModem.h"
#include "httptest.h"

Serial pc(USBTX, USBRX);

#define MODEM_HUAWEI_CDMA
#define MODEM_APN "CMNET"

#if !defined(MODEM_HUAWEI_GSM) && !defined(MODEM_HUAWEI_CDMA)
#warning No modem defined, using GSM by default
#define MODEM_HUAWEI_GSM
#endif

#ifndef MODEM_APN
#warning APN not specified, using "internet"
#define MODEM_APN "internet"
#endif

#ifndef MODEM_USERNAME
#warning username not specified
#define MODEM_USERNAME NULL
#endif

#ifndef MODEM_PASSWORD
#warning password not specified
#define MODEM_PASSWORD NULL
#endif

void test(void const*)
{
	pc.printf("Start the httptest thread,Thread ID is %d.\n",Thread::gettid());
#ifdef MODEM_HUAWEI_GSM
    HuaweiUSBGSMModem modem;
#else
    HuaweiUSBCDMAModem modem(NC, true);
#endif
    httptest(modem, MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD);
}

int main()
{
	pc.printf("Start the Remote Update main process......\n");
	Thread testTask(test, NULL, osPriorityNormal, 1024 * 4);
	DigitalOut myled(LED1);
	while(1)
	{
		myled=!myled;
		Thread::wait(1000);  
	}
}
