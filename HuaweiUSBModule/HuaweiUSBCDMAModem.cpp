/* HuaweiUSBCDMAModem.cpp */
/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define __DEBUG__ 1
#ifndef __MODULE__
#define __MODULE__ "HuaweiUSBCDMAModem.cpp"
#endif

#include "HuaweiUSBCDMAModem.h"

#include "core/fwk.h"

#include "HuaweiCDMAModemInitializer.h"
#include "USBHost.h"

HuaweiUSBCDMAModem::HuaweiUSBCDMAModem(PinName powerGatingPin /*= NC*/, bool powerGatingOnWhenPinHigh /* = true*/) : 
	m_dongle(),
	m_atStream(m_dongle.getSerial(2)),
	m_pppStream(m_dongle.getSerial(0)),
	m_at(&m_atStream),
	m_ppp(&m_pppStream),
	m_dongleConnected(false),
	m_ipInit(false),
	m_atOpen(false),
	m_powerGatingPin(powerGatingPin),
	m_powerGatingOnWhenPinHigh(powerGatingOnWhenPinHigh)
{
  USBHost* host = USBHost::getHostInst();
  m_dongle.addInitializer(new HuaweiCDMAModemInitializer(host));
  if( m_powerGatingPin != NC )
  {
    power(false); //Dongle will have to be powered on manually
  }
}

#if 0
class CSSProcessor : public IATCommandsProcessor
{
public:
  CSSProcessor() : status(STATUS_REGISTERING)
  {

  }
  enum REGISTERING_STATUS { STATUS_REGISTERING, STATUS_OK };
  REGISTERING_STATUS getStatus()
  {
    return status;
  }
private:
  virtual int onNewATResponseLine(ATCommandsInterface* pInst, const char* line)
  {
    char b;
		char bc[3] = "";
		int sid = 99999; 
		
    //if( sscanf(line, "%*d, %c", &r) == 1 )
    if(sscanf(line, "%*s %c,%2s,%d", &b,bc,&sid)==3)
		{
				if(strcmp("Z", bc) == 0)
					status = STATUS_REGISTERING;
				else
					status = STATUS_OK;
    }
    return OK;
  }
  virtual int onNewEntryPrompt(ATCommandsInterface* pInst)
  {
    return OK;
  }
  volatile REGISTERING_STATUS status;
};
#else
class CREGProcessor : public IATCommandsProcessor
{
public:
  CREGProcessor() : status(STATUS_REGISTERING)
  {

  }
  enum REGISTERING_STATUS { STATUS_REGISTERING, STATUS_OK, STATUS_FAILED };
  REGISTERING_STATUS getStatus()
  {
    return status;
  }
private:
  virtual int onNewATResponseLine(ATCommandsInterface* pInst, const char* line)
  {
	printf("......line = %s..............\n", line);
    int r;
    if( sscanf(line, "+CREG: %*d,%d", &r) == 1 )
    {
      switch(r)
      {
      case 1:
      case 5:
        status = STATUS_OK;
        break;
      case 0:
      case 2:
        status = STATUS_REGISTERING;
        break;
      case 3:
      default:
        status = STATUS_FAILED;
        break;
      }
    }
    return OK;
  }
  virtual int onNewEntryPrompt(ATCommandsInterface* pInst)
  {
    return OK;
  }
  volatile REGISTERING_STATUS status;
};
#endif

int HuaweiUSBCDMAModem::connect(const char* apn, const char* user, const char* password)
{
  if( !m_ipInit )
  {
    m_ipInit = true;
    m_ppp.init();
  }
  m_ppp.setup(user, password, DEFAULT_MSISDN_SCDMA);

  int ret = init();
  if(ret)
  {
    return ret;
  }

#if 0
  at_test();
#endif
  
  ATCommandsInterface::ATResult result;

  if(apn != NULL)
  {
    char cmd[48];
    sprintf(cmd, "AT+CGDCONT=1,\"IP\",\"%s\"\r", apn);
    ret = m_at.executeSimple(cmd, &result);
    DBG("Result of command: Err code=%d", ret);
    DBG("ATResult: AT return=%d (code %d)", result.result, result.code);
    DBG("APN set to %s", apn);
  }

  //Connect
  DBG("Connecting");

  DBG("Connecting PPP");

  ret = m_ppp.connect();
  DBG("Result of connect: Err code=%d", ret);

  return ret;
}

void HuaweiUSBCDMAModem::AT_DBGX(char *m_inputBuf)
{
	DBGX("In buffer: [");
	for(int i=0; i<strlen((char *)m_inputBuf); i++) {
		if(m_inputBuf[i]=='\r') {
			DBGX("<CR>");
		} else if(m_inputBuf[i]=='\n') {
			DBGX("<LF>");
		} else
			DBGX("%c",m_inputBuf[i]);
	}
	DBGX("]\r\n");
	Thread::wait(500);
}

int HuaweiUSBCDMAModem::disconnect()
{
  DBG("Disconnecting from PPP");
  int ret = m_ppp.disconnect();
  if(ret)
  {
    ERR("Disconnect returned %d, still trying to disconnect", ret);
  }
  
  //Ugly but leave dongle time to recover
  Thread::wait(500);

  DBG("Trying to hangup");

  return OK;
}

int HuaweiUSBCDMAModem::power(bool enable)
{
  if( m_powerGatingPin == NC )
  {
    return NET_INVALID; //A pin name has not been provided in the constructor
  }

  if(!enable) //Will force components to re-init
  {
    cleanup();
  }
  
  DigitalOut powerGatingOut(m_powerGatingPin);
  powerGatingOut = m_powerGatingOnWhenPinHigh?enable:!enable;

  return OK;
}

bool HuaweiUSBCDMAModem::power()
{
  if( m_powerGatingPin == NC )
  {
    return true; //Assume power is always on 
  }
  
  DigitalOut powerGatingOut(m_powerGatingPin);
  return m_powerGatingOnWhenPinHigh?powerGatingOut:!powerGatingOut;
}

int HuaweiUSBCDMAModem::init()
{
  printf("HuaweiUSBCDMAModem::init..................\r\n");
  if( !m_dongleConnected )
  {
    if(!power())
    {
      //Obviously cannot initialize the dongle if it is disconnected...
      ERR("Power is off");
      return NET_INVALID;
    }
    m_dongleConnected = true;
    while( !m_dongle.connected() )
    {
      m_dongle.tryConnect();
      Thread::wait(100);
    }
  }

  if(m_atOpen)
  {
    return OK;
  }

  DBG("Starting AT thread if needed\r");
  int ret = m_at.open();
  if(ret)
  {
    return ret;
  }

  DBG("Sending initialisation commands\r");
  ret = m_at.init();
  if(ret)
  {
    return ret;
  }

  if(m_dongle.getDongleType() == WAN_DONGLE_TYPE_HUAWEI_ME909S821)
  {
    INFO("Using a HUAWEI ME909S-821 Dongle");
  }
  else
  {
    WARN("Using an Unknown Dongle");
  }

  ATCommandsInterface::ATResult result;

#if 0
  //Wait for network registration
  CSSProcessor cssProcessor;
  do
  {
    DBG("Waiting for network registration");
    ret = m_at.execute("AT+CSS?\r", &cssProcessor, &result);
    DBG("Result of command: Err code=%d\n", ret);
    DBG("ATResult: AT return=%d (code %d)\n", result.result, result.code);
    if(cssProcessor.getStatus() == CSSProcessor::STATUS_REGISTERING)
    {
      Thread::wait(3000);
    }
  } while(cssProcessor.getStatus() == CSSProcessor::STATUS_REGISTERING);
#else
	//Wait for network registration
	CREGProcessor cregProcessor;
	do
	{
	  DBG("Waiting for network registration");
	  ret = m_at.execute("AT+CREG?\r", &cregProcessor, &result);
	  DBG("Result of command: Err code=%d\n", ret);
	  DBG("ATResult: AT return=%d (code %d)\n", result.result, result.code);
	  if(cregProcessor.getStatus() == CREGProcessor::STATUS_REGISTERING)
	  {
		Thread::wait(3000);
	  }
	} while(cregProcessor.getStatus() == CREGProcessor::STATUS_REGISTERING);
	if(cregProcessor.getStatus() == CREGProcessor::STATUS_FAILED)
	{
	  ERR("Registration denied");
	  return NET_AUTH;
	}
#endif
  
  m_atOpen = true;

  return OK;
}

int HuaweiUSBCDMAModem::cleanup()
{
  if(m_ppp.isConnected())
  {
    WARN("Data connection is still open"); //Try to encourage good behaviour from the user
    m_ppp.disconnect(); 
  }

  if(m_atOpen)
  {
    m_at.close();
    m_atOpen = false;
  }
  
  m_dongle.disconnect();
  m_dongleConnected = false;
  
  return OK;
}


int HuaweiUSBCDMAModem::at_test(void)
{
	char buf[64];
	char buf1[64];
	
	/* Send ATZ E1 V1 command */
	DBG("Send ATZ E1 V1 command");
	int res = m_atStream.atInstWrite("ATZ E1 V1\r");
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	res = m_atStream.atInstRead(buf);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf1, 0, sizeof(buf1));
	res = m_atStream.atInstRead(buf1);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	strcat(buf, buf1);	
	AT_DBGX(buf);

	/* Send AT+CGMR command */
	DBG("Send AT+CGMR command");
	res = m_atStream.atInstWrite("AT+CGMR\r");
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	res = m_atStream.atInstRead(buf);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf1, 0, sizeof(buf1));
	res = m_atStream.atInstRead(buf1);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	strcat(buf, buf1);
	AT_DBGX(buf);

	/* Send AT+CGMM command */
	DBG("Send AT+CGMM command");
	res = m_atStream.atInstWrite("AT+CGMM\r");
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	res = m_atStream.atInstRead(buf);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf1, 0, sizeof(buf1));
	res = m_atStream.atInstRead(buf1);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	strcat(buf, buf1);	
	AT_DBGX(buf);

	/* Send AT+CREG? command */
	DBG("Send AT+CREG? command");
	res = m_atStream.atInstWrite("AT+CREG?\r");
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	res = m_atStream.atInstRead(buf);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf1, 0, sizeof(buf1));
	res = m_atStream.atInstRead(buf1);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	strcat(buf, buf1);	
	AT_DBGX(buf);

	/* Send AT+CGATT? command */
	DBG("Send AT+CGATT? command");
	res = m_atStream.atInstWrite("AT+CGATT?\r");
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	res = m_atStream.atInstRead(buf);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf1, 0, sizeof(buf1));
	res = m_atStream.atInstRead(buf1);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	strcat(buf, buf1);	
	AT_DBGX(buf);

	/* Send AT+CGDCONT=1,"IP","cmnet" command */
	DBG("Send AT+CGDCONT=1,\"IP\",\"cmnet\" command");
	res = m_atStream.atInstWrite("AT+CGDCONT=1,\"IP\",\"cmnet\"\r");
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	res = m_atStream.atInstRead(buf);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf1, 0, sizeof(buf1));
	res = m_atStream.atInstRead(buf1);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	strcat(buf, buf1);	
	AT_DBGX(buf);

	/* Send ATH command */
	DBG("Send ATH command");
	res = m_atStream.atInstWrite("ATH\r");
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	res = m_atStream.atInstRead(buf);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf1, 0, sizeof(buf1));
	res = m_atStream.atInstRead(buf1);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	strcat(buf, buf1);	
	AT_DBGX(buf);

	/* Send ATD *99# command */
	DBG("Send ATD *99# command");
	res = m_atStream.atInstWrite("ATD *99#\r");
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	res = m_atStream.atInstRead(buf);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	memset(buf1, 0, sizeof(buf1));
	res = m_atStream.atInstRead(buf1);
	if (res) {
		ERR("at_test failure!");
		return -1;
	}
	strcat(buf, buf1);	
	AT_DBGX(buf);

	return 0;
}
