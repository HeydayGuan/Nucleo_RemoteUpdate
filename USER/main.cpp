/// Simple example making use of cantcoap library, just keeps sending packets to a server and prints the response.

#include "rtos.h"
#include "mbed.h"
#include "HuaweiUSBCDMAModem.h"
#include "cantcoap.h"
#include "bsd_socket.h"
#include "MbedJSONValue.h"
#include "remoteupdate.h"

#define MODEM_APN "CMNET"
#define MODEM_USERNAME NULL
#define MODEM_PASSWORD NULL

void pppSurfing(void const*) {
	startRemoteUpdate();
}

int main() {
    DBG_INIT();
    DBG_SET_SPEED(115200);
    DBG_SET_NEWLINE("\r\n");

	INFO("Start the Nucleo Remote Update main process......");

	// connect to cellular network
	HuaweiUSBCDMAModem modem(NC, true);
    modem.connect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD);

	Thread pppSurfingTask(pppSurfing, NULL, osPriorityNormal, 1024 * 4);

	DigitalOut myled(LED1);
	while(1)
	{
		myled=!myled;
		Thread::wait(1000);  
	}

    return true;
}
