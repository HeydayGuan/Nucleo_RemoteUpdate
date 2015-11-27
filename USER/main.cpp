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
