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
 
 
#include "flash25spi.h"
#include "wait_api.h"
 
#define DEBUG
 
struct dataBase {
unsigned char vendor;
unsigned char device;
unsigned char capacity;
unsigned int memsize;
unsigned int blocksize;
unsigned int sectorsize;
unsigned int pagesize;
};

const struct dataBase devices[] = {
// vendor, device, capacity,   memsize, blocksize, sectorsize, pagesize
{    0xEF,   0x40,     0x17,  0x800000,    0x8000,     0x1000,    0x100}, // Manufacturer: Winbond, Device: W25Q64

{    0x1c,   0x31,     0x10,   0x10000,    0x8000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25F05 (untested)
{    0x1c,   0x31,     0x11,   0x20000,    0x8000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25F10 (untested)
{    0x1c,   0x31,     0x12,   0x40000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25F20 (untested)
{    0x1c,   0x31,     0x13,   0x80000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25F40 (untested)
{    0x1c,   0x31,     0x14,  0x100000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25F80 (untested)
{    0x1c,   0x31,     0x15,  0x200000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25F16 (untested)
{    0x1c,   0x30,     0x15,  0x200000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25Q16 (untested)
{    0x1c,   0x31,     0x16,  0x400000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25F32 (untested)
{    0x1c,   0x30,     0x16,  0x400000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25Q32A
{    0x1c,   0x30,     0x17,  0x800000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25Q64 (untested)
{    0x1c,   0x30,     0x18, 0x1000000,   0x10000,     0x1000,    0x100}, // Manufacturer: EON, Device: EN25Q128 (untested)
 
{    0x20,   0x20,     0x10,  0x10000,    0x8000,          0,    0x100}, // Manufacturer: Numonyx, Device: M25P05-A (untested)
{    0x20,   0x20,     0x16, 0x400000,   0x10000,          0,    0x100}, // Manufacturer: Numonyx, Device: M25P32 (untested)
{    0x00,   0x00,     0x00,     0x00,      0x00,       0x00,     0x00}, // end of table
};
 
 
#define HIGH(x) ((x&0xff0000)>>16)
#define MID(x) ((x&0xff00)>>8)
#define LOW(x) (x&0xff)
 
flash25spi::flash25spi(SPI *spi, PinName enable) {
    unsigned char chipid[3] = {0};
    unsigned int i = 0;
    _spi=spi;
    _enable=new DigitalOut(enable);
    _enable->write(1);

//  wait_us(1000);
     
    _enable->write(0);
    wait_us(1);
    // send address

    _spi->write(0x9f);
    chipid[0] = _spi->write(0); // get vendor ID
    chipid[1] = _spi->write(0); // get device ID
    chipid[2] = _spi->write(0); // get capacity
    wait_us(1);
    _enable->write(1);
    
    _size = 0;
 
#ifdef DEBUG
    printf ("got flash ids: %x, %x, %x\n", chipid[0], chipid[1], chipid[2]);
#endif
 
    while (_size == 0) {
#ifdef DEBUG
        printf ("checking: %x, %x, %x\n", devices[i].vendor, devices[i].device, devices[i].capacity);
#endif
        if (devices[i].vendor == 0) {
            printf("flash device not found\n");
            return;
        }
        if ((chipid[0] == devices[i].vendor) &&
            (chipid[1] == devices[i].device) &&
            (chipid[2] == devices[i].capacity)) {
                _size=devices[i].memsize;
                _blockSize=devices[i].blocksize;
                _sectorSize=devices[i].sectorsize;
                _pageSize=devices[i].pagesize;
#ifdef DEBUG
                printf("device found: %x - %x, %x, %x, %x\n",i, _size, _blockSize, _sectorSize, _pageSize);
#endif
        }
        else
            i++;
    }
    return;
}
 
flash25spi::~flash25spi() {
    delete _enable;
}
 
bool flash25spi::read(unsigned int startAdr, unsigned int len, char *data) {
    // assertion
    if (startAdr+len>_size)
        return false;
//  char* ret=(char*)malloc(len);
    if (!len) return false;

    _enable->write(0);
    wait_us(1);
    // send address
    _spi->write(0x03);
    _spi->write(HIGH(startAdr));
    _spi->write(MID(startAdr));
    _spi->write(LOW(startAdr));
 
    // read data into buffer
    for (unsigned int i=0;i<len;i++) {
        data[i]=_spi->write(0);
    }
    wait_us(1);
    _enable->write(1);

    return true;
}
 
bool flash25spi::write(unsigned int startAdr, unsigned int len, const char* data) {
    if (startAdr+len>_size)
        return false;
 
    unsigned int ofs=0;
    while (ofs<len) {
        // calculate amount of data to write into current page
        int pageLen=_pageSize-((startAdr+ofs)%_pageSize);
        if (ofs+pageLen>len)
            pageLen=len-ofs;
        // write single page
        bool b=writePage(startAdr+ofs,pageLen,data+ofs);
        if (!b)
            return false;
        // and switch to next page
        ofs+=pageLen;
    }
    return true;
}
 
bool flash25spi::writePage(unsigned int startAdr, unsigned int len, const char* data) {
    enableWrite();
 
    _enable->write(0);
    wait_us(1);
 
    _spi->write(0x02);
    _spi->write(HIGH(startAdr));
    _spi->write(MID(startAdr));
    _spi->write(LOW(startAdr));
 
    // do real write
    for (unsigned int i=0;i<len;i++) {
        _spi->write(data[i]);
    }
    wait_us(1);
    // disable to start physical write
    _enable->write(1);
    
    waitForWrite();
 
    return true;
}
 
void flash25spi::clearSector(unsigned int addr) {
    if (_sectorSize == 0) {
        clearBlock(addr);
        return;
    }
        
    addr &= ~(_sectorSize-1);
 
    enableWrite();
    _enable->write(0);
    wait_us(1);
    _spi->write(0x20);
    _spi->write(HIGH(addr));
    _spi->write(MID(addr));
    _spi->write(LOW(addr));
    wait_us(1);
    _enable->write(1);
    waitForWrite();
}
 
void flash25spi::clearBlock(unsigned int addr) {
    addr &= ~(_blockSize-1);
 
    enableWrite();
    _enable->write(0);
    wait_us(1);
    _spi->write(0xd8);
    _spi->write(HIGH(addr));
    _spi->write(MID(addr));
    _spi->write(LOW(addr));
    wait_us(1);
    _enable->write(1);
    waitForWrite();
}
 
void flash25spi::clearMem() {
    enableWrite();
    _enable->write(0);
    wait_us(1);
    _spi->write(0xc7);
    wait_us(1);
    _enable->write(1);
    waitForWrite();
}
 
int flash25spi::readStatus() {
    _enable->write(0);
    wait_us(1);
    _spi->write(0x5);
    int status=_spi->write(0x00);
    wait_us(1);
    _enable->write(1);
    return status;
}
 
void flash25spi::waitForWrite() {
    while (true) {
        if (0==readStatus()&1)
            break;
        wait_us(10);
    }
}
 
void flash25spi::enableWrite()
{
    _enable->write(0);
    wait_us(1);
    _spi->write(0x06);
    wait_us(1);
    _enable->write(1);
}
