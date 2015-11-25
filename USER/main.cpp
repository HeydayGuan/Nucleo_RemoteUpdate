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
