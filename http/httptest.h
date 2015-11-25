#ifndef HTTPTEST_H_
#define HTTPTEST_H_

#include "HuaweiUSBCDMAModem.h"

int httptest(HuaweiUSBCDMAModem& modem, const char* apn = NULL, const char* username = NULL, const char* password= NULL);

#endif

