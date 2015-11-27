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
