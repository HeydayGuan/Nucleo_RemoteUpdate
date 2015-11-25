#include "mbed.h"
#include "HuaweiUSBCDMAModem.h"
#include "HTTPClient.h"
#include "httptest.h"

int httptest(HuaweiUSBCDMAModem& modem, const char* apn, const char* username, const char* password)
{
    printf("Connecting...\r\n");

    HTTPClient http;
    char str[512];

    Thread::wait(1000);
    int ret = modem.connect(apn, username, password);
    if(ret)
    {
      printf("Could not connect\r\n");
      return false;
    }

    //GET data
    printf("Trying to fetch page...\r\n");
    ret = http.get("https://developer.mbed.org/media/uploads/donatien/hello.txt", str, 128);
    if (!ret)
    {
      printf("Page fetched successfully - read %d characters\r\n", strlen(str));
      printf("Result: %s\n", str);
    }
    else
    {
      printf("Page fetched Error - ret = %d - HTTP return code = %d\r\n", ret, http.getHTTPResponseCode());
      modem.disconnect();
      return false;
    }

    //POST data
    HTTPMap map;
    HTTPText text(str, 512);
    map.put("Hello", "World");
    map.put("test", "1234");
    printf("Trying to post data...\r\n");
    ret = http.post("http://httpbin.org/post", map, &text);
    if (!ret)
    {
      printf("Executed POST successfully - read %d characters\r\n", strlen(str));
      printf("Result: %s\n", str);
    }
    else
    {
      printf("Executed POST Error - ret = %d - HTTP return code = %d\r\n", ret, http.getHTTPResponseCode());
      modem.disconnect();
      return false;
    }

    modem.disconnect();
    return true;
}
