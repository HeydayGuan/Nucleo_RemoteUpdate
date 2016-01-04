/* ATCommandsInterface.cpp */
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

#define __DEBUG__  4 //ERR+WARN
#ifndef __MODULE__
#define __MODULE__ "ATCommandsInterface.cpp"
#endif

#include "mbed.h"
#include "core/fwk.h"

#include <cstdio>
#include <cstring> //For memset, strstr...

using std::memmove;

#include "ATCommandsInterface.h"

ATCommandsInterface::ATCommandsInterface(IOStream* pStream) :
   m_pStream(pStream), m_open(false), m_transactionState(IDLE)
{
	 m_inputPos = 0;
}

//Open connection to AT Interface in order to execute command & register/unregister events
int ATCommandsInterface::open()
{
  if( m_open )
  {
    WARN("AT interface is already open");
    return OK;
  }
  DBG("Opening AT interface");

  m_open = true;

  DBG("AT interface opened");
  
  return OK;
}

//Initialize AT link & start events processing
int ATCommandsInterface::init(bool reset /* = true*/)
{  
  if (reset)
  {
    DBG("Sending ATZ E1 V1");
    //Should we flush m_pStream at this point ???
    int err;
    int tries = 6;
    do
    {
      err = executeInternal("ATZ E1 V1\r", this, NULL, 3000); //Enable echo and verbosity
     if(err && tries)
      {
        WARN("No response, trying again");
        Thread::wait(2000); //Give dongle time to recover
      }
    } while(err && tries--);
    if( err )
    {
      ERR("Sending ATZ E1 V1 returned with err code %d", err);
      return err;
    }
  }

  //Enable events handling and execute events enabling commands
//  enableEvents();

  DBG("AT interface initialized");

  return OK;
}

//Close connection
int ATCommandsInterface::close()
{
  if( !m_open )
  {
    WARN("AT interface is already closed");
    return OK;
  }

  DBG("Closing AT interface");

  //Disable events handling and advertize this to the events handlers
//  disableEvents();

  m_open = false;
  
  DBG("AT interface closed");
  return OK;
}

bool ATCommandsInterface::isOpen()
{
  return m_open;
}

int ATCommandsInterface::executeSimple(const char* command, ATResult* pResult, uint32_t timeout/*=1000*/)
{
  return execute(command, this, pResult, timeout);
}

int ATCommandsInterface::execute(const char* command, IATCommandsProcessor* pProcessor, ATResult* pResult, uint32_t timeout/*=1000*/)
{
  if(!m_open)
  {
    WARN("Interface is not open!");
    return NET_INVALID;
  }

//  disableEvents(); //Disable unsollicited result codes
  int ret = executeInternal(command, pProcessor, pResult, timeout);
//  enableEvents(); //Re-enable unsollicited result codes whatever the result of the command is
  
  return ret;
}

//Private methods

int ATCommandsInterface::executeInternal(const char* command, IATCommandsProcessor* pProcessor, ATResult* pResult, uint32_t timeout/*=1000*/)
{
  DBG("Executing command %s", command);

  //Send params to the process routine
  m_transactionCommand = command;
  if(pProcessor != NULL)
  {
    m_pTransactionProcessor = pProcessor;
  }
  else
  {
    m_pTransactionProcessor = this; //Use default behaviour
  }

  int res = m_pStream->atInstWrite(m_transactionCommand);
  if (res) {
 	ERR("AT instruction[%s] write failure!", m_transactionCommand);
	return -1;
  }
  m_transactionState = COMMAND_SENT;

  int tries = 4;
  do{
	memset(m_inputBuf, 0, sizeof(m_inputBuf));
	res = m_pStream->atInstRead(m_inputBuf);
	if (res) {
		ERR("AT instruction[%s] read failure!", m_transactionCommand);
		return -3;
	}
	#if 1
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
	#endif
	preProcessReadLine();
  } while (m_transactionState != IDLE && tries--);

  if(pResult != NULL)
  {
    *pResult = m_transactionResult;
  }

  int ret = ATResultToReturnCode(m_transactionResult);
  if(ret != OK)
  {
    WARN("Command returned AT result %d with code %d", m_transactionResult.result, m_transactionResult.code);
//    WARN("Command \"%s\" returned AT result %d with code %d", command, m_transactionResult.result, m_transactionResult.code);
  }

  DBG("Command returned successfully");

  return ret;
}

int ATCommandsInterface::preProcessReadLine()
{
  static bool lineDetected = false;
  int ret = 0;
  size_t readLen = strlen(m_inputBuf);
	
  if (readLen == 0) {
    DBG("Nothing read");
    return OK; //0 chars were read
  }

  m_inputPos+=readLen;
  m_inputBuf[m_inputPos] = '\0'; //Add null terminating character to ease the use of str* functions
//  DBG("In buffer: [%s]", m_inputBuf);

  DBG("Trying to process incoming line");
  bool lineProcessed = false;

  do
  {
    lineProcessed = false; //Reset flag

    DBG("New iteration");

    //Look for a new line
    if(!lineDetected)
    {
      DBG("No line detected yet");
      //Try to look for a starting CRLF
      char* crPtr = strchr(m_inputBuf, CR);
      /*
      Different cases at this point:
      - CRLF%c sequence: this is the start of a line
      - CRLFCR(LF) sequence: this is the end of a line (followed by the beginning of the next one)
      - LF: this is the trailing LF char of the previous line, discard
      - CR / CRLF incomplete sequence: more data is needed to determine which action to take
      - %c ... CR sequence: this should be the echo of the previous sequence
      - %c sequence: This might be the echo of the previous command; more data is needed to determine which action to take

      In every case, move mem at the beginning
      */
      if(crPtr != NULL)
      {
        DBG("CR char found");

#if 0
        //Discard all preceding characters (can do nothing if m_inputBuf == crPtr)
		if ((m_inputPos + 1) - (crPtr-m_inputBuf) <= sizeof(m_inputBuf)) {
			memmove(m_inputBuf, crPtr, (m_inputPos + 1) - (crPtr-m_inputBuf)); //Move null-terminating char as well
			m_inputPos = m_inputPos - (crPtr-m_inputBuf); //Adjust m_inputPos
		}
#endif

        //If the line starts with CR, this should be a result code
        if( crPtr == m_inputBuf )
        {
          //To determine the sequence we need at least 3 chars
          if(m_inputPos >= 3)
          {
            //Look for a LF char next to the CR char
            if(m_inputBuf[1] == LF)
            {
              //At this point we can check whether this is the end of a preceding line or the beginning of a new one
              if(m_inputBuf[2] != CR)
              {
                DBG("Beginning of new line found");
                //Beginning of a line
                lineDetected = true; //Move to next state-machine step
              }
              else
              {
                //End of an unprocessed line
                WARN("End of unprocessed line");
              }
              //In both cases discard CRLF
              DBG("Discarding CRLF");
			  if ((m_inputPos + 1) - 2 <= sizeof(m_inputBuf)) {
				memmove(m_inputBuf, m_inputBuf + 2, (m_inputPos + 1) - 2); //Move null-terminating char as well
				m_inputPos = m_inputPos - 2; //Adjust m_inputPos
			  }
            }
            else
            {
              //This is completely unexpected, discard the CR char to try to recover good state
              WARN("Unexpected %c char (%02d code) found after CR char", m_inputBuf[1]);
			  if ((m_inputPos + 1) - 1 <= sizeof(m_inputBuf)) {
				memmove(m_inputBuf, m_inputBuf + 1, (m_inputPos + 1) - 1); //Move null-terminating char as well
				m_inputPos = m_inputPos - 1; //Adjust m_inputPos
			  }
            }
          }
        }
        //if the line does NOT begin with CR, this can be an echo of the previous command, process it
        else
        {
          int crPos = crPtr - m_inputBuf;
          int lfOff = 0; //Offset for LF if present
          DBG("New line found (possible echo of command)");
          //This is the end of line
          //Replace m_inputBuf[crPos] with null-terminating char
          m_inputBuf[crPos] = '\0';
          //Check if there is a LF char afterwards
          if(m_inputPos - crPos >= 1)
          {
            if(m_inputBuf[crPos+1] == LF)
            {
              lfOff++; //We will discard LF char as well
            }
          }
          //Process line
          int ret = processReadLine();
          if(ret)
          {
            m_inputPos = 0;
            m_inputBuf[0] = '\0'; //Always have a null-terminating char at start of buffer
            lineDetected = false;
            return ret;
          }

          //If sendData has been called, all incoming data has been discarded
          if(m_inputPos > 0)
          {
			if ((m_inputPos + 1) - (crPos + lfOff + 1) <= sizeof(m_inputBuf)) {
				memmove(m_inputBuf, m_inputBuf + crPos + lfOff + 1, (m_inputPos + 1) - (crPos + lfOff + 1)); //Move null-terminating char as well
				m_inputPos = m_inputPos - (crPos + lfOff + 1); //Adjust m_inputPos
			}
          }
          DBG("One line was successfully processed");
          lineProcessed = true; //Line was processed with success
          lineDetected = false; //Search now for a new line
        }
      }
      else if(m_inputBuf[0] == LF) //If there is a remaining LF char from the previous line, discard it
      {
        DBG("Discarding single LF char");
		if ((m_inputPos + 1) - 1 <= sizeof(m_inputBuf)) {
			memmove(m_inputBuf, m_inputBuf + 1, (m_inputPos + 1) - 1); //Move null-terminating char as well
			m_inputPos = m_inputPos - 1; //Adjust m_inputPos
		}
      }
    }

    //Look for the end of line
    if(lineDetected)
    {
      DBG("Looking for end of line");
      //Try to look for a terminating CRLF
      char* crPtr = strchr(m_inputBuf, CR);
      /*
      Different cases at this point:
      - CRLF sequence: this is the end of the line
      - CR%c sequence : unexpected
      - CR incomplete sequence: more data is needed to determine which action to take
      */

      //Try to look for a '>' (greater than character) that marks an entry prompt
      char* greaterThanPtr = strchr(m_inputBuf, GD);
      /*
      This character must be detected as there is no CRLF sequence at the end of an entry prompt
       */

      if(crPtr != NULL)
      {
        DBG("CR char found");
        int crPos = crPtr - m_inputBuf;
        //To determine the sequence we need at least 2 chars
        if(m_inputPos - crPos >= 2)
        {
          //Look for a LF char next to the CR char
          if(m_inputBuf[crPos + 1] == LF)
          {
            DBG("End of new line found");
            //This is the end of line
            //Replace m_inputBuf[crPos] with null-terminating char
            m_inputBuf[crPos] = '\0';
            //Process line
            int ret = processReadLine();
            if(ret)
            {
              m_inputPos = 0;
              m_inputBuf[0] = '\0'; //Always have a null-terminating char at start of buffer
              lineDetected = false;
              return ret;
            }

            //If sendData has been called, all incoming data has been discarded
            if(m_inputPos > 0)
            {
              //Shift remaining data to beginning of buffer
			  if ((m_inputPos + 1) - (crPos + 2) > sizeof(m_inputBuf)) {
				m_inputPos = 0;
				m_inputBuf[0] = '\0'; //Always have a null-terminating char at start of buffer
				lineDetected = false;
				return -1;
			  }
			  memmove(m_inputBuf, m_inputBuf + crPos + 2, (m_inputPos + 1) - (crPos + 2)); //Move null-terminating char as well
			  m_inputPos = m_inputPos - (crPos + 2); //Adjust m_inputPos
            }

            DBG("One line was successfully processed");
            lineProcessed = true; //Line was processed with success
          }
          else
          {
            //This is completely unexpected, discard all chars till the CR char to try to recover good state
            WARN("Unexpected %c char (%02d code) found in incoming line", m_inputBuf[crPos + 1]);
			if ((m_inputPos + 1) - (crPos + 1) <= sizeof(m_inputBuf)) {
				memmove(m_inputBuf, m_inputBuf + crPos + 1, (m_inputPos + 1) - (crPos + 1)); //Move null-terminating char as well
				m_inputPos = m_inputPos - (crPos + 1); //Adjust m_inputPos
			}
          }
          lineDetected = false; //In both case search now for a new line
        }
      }
      else if(greaterThanPtr != NULL)
      {
        DBG("> char found");
        int gdPos = greaterThanPtr - m_inputBuf;
        //To determine the sequence we need at least 2 chars
        if(m_inputPos - gdPos >= 2)
        {
          //Look for a space char next to the GD char
          if(m_inputBuf[gdPos + 1] == ' ')
          {
            //This is an entry prompt
            //Replace m_inputBuf[gdPos] with null-terminating char
            m_inputBuf[gdPos] = '\0';

            //Shift remaining data to beginning of buffer
			if ((m_inputPos + 1) - (gdPos + 1) <= sizeof(m_inputBuf)) {
				memmove(m_inputBuf, m_inputBuf + gdPos + 1, (m_inputPos + 1) - (gdPos + 1)); //Move null-terminating char as well
				m_inputPos = m_inputPos - (gdPos + 1); //Adjust m_inputPos
			}

            //Process prompt
            ret = processEntryPrompt();
            if(ret)
            {
              m_inputPos = 0;
              m_inputBuf[0] = '\0'; //Always have a null-terminating char at start of buffer
              lineDetected = false;
              return ret;
            }

            DBG("One line was successfully processed");
            lineProcessed = true; //Line was processed with success
          }
          else
          {
            //This is completely unexpected, discard all chars till the GD char to try to recover good state
            WARN("Unexpected %c char (%02d code) found in incoming line", m_inputBuf[gdPos + 1]);
			if ((m_inputPos + 1) - (gdPos + 1) <= sizeof(m_inputBuf)) {
				memmove(m_inputBuf, m_inputBuf + gdPos + 1, (m_inputPos + 1) - (gdPos + 1)); //Move null-terminating char as well
				m_inputPos = m_inputPos - (gdPos + 1); //Adjust m_inputPos
			}
          }
          lineDetected = false; //In both case search now for a new line
        }
      }
    }
  } while(lineProcessed); //If one complete line was processed there might be other incoming lines that can also be processed without reading the buffer again

  //If the line could not be processed AND buffer is full, it means that we won't ever be able to process it (buffer too short)
  if(m_inputPos == AT_INPUT_BUF_SIZE - 1)
  {
    //Discard everything
    m_inputPos = 0;
    m_inputBuf[0] = '\0'; //Always have a null-terminating char at start of buffer
    WARN("Incoming buffer is too short to process incoming line");
    //Look for a new line
    lineDetected = false;
  }

  DBG("Processed every full incoming lines");

  return OK;
}

int ATCommandsInterface::processReadLine()
{
  DBG("Processing read line [%s]", m_inputBuf);
  //The line is stored in m_inputBuf
  if(m_transactionState == COMMAND_SENT)
  {
    //If the command has been sent, checks echo to see if it has been received properly
    if( strncmp(m_transactionCommand, m_inputBuf, strlen(m_transactionCommand)-1) == 0 )
    {
      DBG("Command echo received");
      //If so, it means that the following lines will only be solicited results
      m_transactionState = READING_RESULT;
      return OK;
    }
  }
  if(m_transactionState == IDLE || m_transactionState == COMMAND_SENT)
  {
    bool found = false;
    char* pSemicol = strchr(m_inputBuf, ':');
    char* pData = NULL;
    if( pSemicol != NULL ) //Split the identifier & the result code (if it exists)
    {
      *pSemicol = '\0';
      pData = pSemicol + 1;
      if(pData[0]==' ')
      {
        pData++; //Suppress whitespace
      }
    }
#if 0
    //Looks for a unsolicited result code; we can have m_transactionState == COMMAND_SENT as the code may have arrived just before we sent the command
    m_eventsProcessingMtx.lock();
    //Go through the list
    for(int i = 0; i < MAX_AT_EVENTS_HANDLERS; i++) //Find a free slot
    {
      if( m_eventsHandlers[i] != NULL )
      {
        if( m_eventsHandlers[i]->isATCodeHandled(m_inputBuf) )
        {
          m_eventsHandlers[i]->onEvent(m_inputBuf, pData);
          found = true; //Do not break here as there might be multiple handlers for one event type
        }
      }
    }
    m_eventsProcessingMtx.unlock();
#endif
    if(found)
    {
      return OK;
    }
  }
  if(m_transactionState == READING_RESULT)
  {
    //The following lines can either be a command response or a result code (OK / ERROR / CONNECT / +CME ERROR: %s / +CMS ERROR: %s)
    if(strcmp("OK", m_inputBuf) == 0)
    {
      DBG("OK result received");
      m_transactionResult.code = 0;
      m_transactionResult.result = ATResult::AT_OK;
      m_transactionState = IDLE;
      return OK;
    }
    else if(strcmp("ERROR", m_inputBuf) == 0)
    {
      DBG("ERROR result received");
      m_transactionResult.code = 0;
      m_transactionResult.result = ATResult::AT_ERROR;
      m_transactionState = IDLE;
      return OK;
    }
    else if(strncmp("CONNECT", m_inputBuf, 7 /*=strlen("CONNECT")*/) == 0) //Result can be "CONNECT" or "CONNECT %d", indicating baudrate
    {
      DBG("CONNECT result received");
      m_transactionResult.code = 0;
      m_transactionResult.result = ATResult::AT_CONNECT;
      m_transactionState = IDLE;
      return OK;
    }
    else if(strcmp("COMMAND NOT SUPPORT", m_inputBuf) == 0) //Huawei-specific, not normalized
    {
      DBG("COMMAND NOT SUPPORT result received");
      m_transactionResult.code = 0;
      m_transactionResult.result = ATResult::AT_ERROR;
      m_transactionState = IDLE;
      return OK;
    }
    else if(strstr(m_inputBuf, "+CME ERROR:") == m_inputBuf) //Mobile Equipment Error
    {
      std::sscanf(m_inputBuf + 12 /* =strlen("+CME ERROR: ") */, "%d", &m_transactionResult.code);
      DBG("+CME ERROR: %d result received", m_transactionResult.code);
      m_transactionResult.result = ATResult::AT_CME_ERROR;
      m_transactionState = IDLE;
      return OK;
    }
    else if(strstr(m_inputBuf, "+CMS ERROR:") == m_inputBuf) //SIM Error
    {
      std::sscanf(m_inputBuf + 13 /* =strlen("+CME ERROR: ") */, "%d", &m_transactionResult.code);
      DBG("+CMS ERROR: %d result received", m_transactionResult.code);
      m_transactionResult.result = ATResult::AT_CMS_ERROR;
      m_transactionState = IDLE;
      return OK;
    }
    else
    {
      DBG("Unprocessed result received: '%s'", m_inputBuf);
      //Must call transaction processor to complete line processing
      int ret = m_pTransactionProcessor->onNewATResponseLine(this, m_inputBuf); //Here sendData can be called
	  //Need by modify??????????????????????????????
      m_transactionResult.result = ATResult::AT_OK;
      m_transactionState = IDLE;
	  //Need by modify??????????????????????????????
      return ret;
    }
  }

  return OK;
}

int ATCommandsInterface::processEntryPrompt()
{
  DBG("Calling prompt handler");
  int ret = m_pTransactionProcessor->onNewEntryPrompt(this); //Here sendData can be called

  if( ret != NET_MOREINFO ) //A new prompt is expected
  {
    DBG("Sending break character");
    //Send CTRL+Z (break sequence) to exit prompt
    char seq[2] = {BRK, 0x00};
//  sendData(seq);
  }

  return OK;
}

int ATCommandsInterface::ATResultToReturnCode(ATResult result) //Helper
{
  if(result.result == ATResult::AT_OK)
  {
    return OK;
  }
  else
  {
    return NET_MOREINFO;
  }
}

/*virtual*/ int ATCommandsInterface::onNewATResponseLine(ATCommandsInterface* pInst, const char* line) //Default implementation for simple commands handling
{
  return OK;
}

/*virtual*/ int ATCommandsInterface::onNewEntryPrompt(ATCommandsInterface* pInst) //Default implementation (just sends Ctrl+Z to exit the prompt by returning OK right-away)
{
  return OK;
}
