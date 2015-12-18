/* This library is based on the Ser25lcxxx library by Hendrik Lipka
* It was adapted to flash memory chips on 19.2.2011 by Klaus Steinhammer
* the BSD license also applies to this code - have fun. 
*/
 
/*
* Ser25lcxxx library
* Copyright (c) 2010 Hendrik Lipka
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/
 
#ifndef __FLASH25SPI_H__
#define __FLASH25SPI_H__
 
#include "mbed.h"
 
/**
A class to read and write all 25* serial SPI flash devices from various manufacturers.
It tries to autodetect the connected device. If your flash is not in the device list, but
it compatible, feel free to modify the library and add it to the list.
*/
 
class flash25spi
{
    public:
        /**
            create the handler class. it tries to autodetect your device. If your device is not recognized, add the devices parameters
            to the device data structure at the library sources.
            @param spi the SPI port where the flash is connected. Must be set to format(8,3), and with a speed matching the one of your device
            @param enable the pin name for the port where /CS is connected
        */
        flash25spi(SPI *spi, PinName enable);
        
        /**
            destroys the handler, and frees the /CS pin        
        */
        ~flash25spi();
        
        /**
            read a part of the flashs memory. The buffer will be allocated here, and must be freed by the user
            @param startAdr the adress where to start reading. Doesn't need to match a page boundary
            @param len the number of bytes to read (must not exceed the end of memory)
            @return NULL in case of an error or if the adresses are out of range, the pointer to the data otherwise
        */
        bool read(unsigned int startAdr, unsigned int len, char *data);
        
        /**
            writes the give buffer into the memory. This function handles dividing the write into 
            pages, and waites until the phyiscal write has finished
            @param startAdr the adress where to start writing. Doesn't need to match a page boundary
            @param len the number of bytes to read (must not exceed the end of memory)
            @return false if the adresses are out of range
        */
        bool write(unsigned int startAdr, unsigned int len, const char* data);
        
        /**
            fills the sector containing the given address with 0xFF - this erases several pages. 
            Refer to the flashes datasheet about the memory organisation.
            @param addr an address within the sector to be cleared
        */
        void clearSector(unsigned int addr);
 
        /**
            fills the block containing the given address with 0xFF - this erases several pages and several sectors. 
            Refer to the flashes datasheet about the memory organisation.
            @param addr an address within the sector to be cleared
        */
        void clearBlock(unsigned int addr);
 
        
        /**
            fills the whole flash with 0xFF
        */
        void clearMem();
        
    private:
        bool writePage(unsigned int startAdr, unsigned int len, const char* data);
        int readStatus();
        void waitForWrite();
        void enableWrite();
        
 
        SPI* _spi;
        DigitalOut* _enable;
        unsigned int _size,_pageSize,_sectorSize,_blockSize;
        
};
 
 
 
#endif