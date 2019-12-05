/* This part of the library was added at 18.05.18 by Constantin Koch
   to enable the use of Advanced Sector Protection (ASP).
*/

#ifdef ARDUINO
  #include "include/SPIFlash.h"
#else
  #include "SPIFlash.h"
#endif

//Command defines
#define PPBP	0xE3	//Persistant Protection Bits Program
#define	PPBRD	0xE2	//Persistant Protection Bits Read
#define	PPBE	0xE4	//Persistant Protection Bits Erase

bool SPIFlash::ASP_PPB_protect_sector(uint32_t _byteaddr) {
  if (_isChipPoweredDown() || !_notBusy() || !_writeEnable()) {
    return false;
  }
  delayMicroseconds(10);
  _beginSPI(PPBP);
  //Transfer Address
  _nextByte(WRITE, Highest(_byteaddr));
  _nextByte(WRITE, Higher(_byteaddr));
  _nextByte(WRITE, Hi(_byteaddr));
  _nextByte(WRITE, Lo(_byteaddr));
  _endSPI();
  if (!_notBusy(100 * 1000L)) {
    return false;
  }
  return true;
}

uint8_t SPIFlash::ASP_PPB_read_Access_Register(uint32_t _byteaddr) {
  if (_isChipPoweredDown() || !_notBusy()) {
    return 23;
  }
  delayMicroseconds(10);
  _beginSPI(PPBRD);
  //Transfer Address
  _nextByte(WRITE, Highest(_byteaddr));
  _nextByte(WRITE, Higher(_byteaddr));
  _nextByte(WRITE, Hi(_byteaddr));
  _nextByte(WRITE, Lo(_byteaddr));
  uint8_t ppbar = _nextByte(READ);
  _endSPI();
  if (!_notBusy(100 * 1000L)) {
    return 23;
  }
  return ppbar;
}

bool SPIFlash::ASP_release_all_sectors(void) {
  if (_isChipPoweredDown() || !_notBusy() || !_writeEnable()) {
    return false;
  }
  delayMicroseconds(10);
  _beginSPI(PPBE);
  _endSPI();

  if (!_notBusy(1000 * 1000L)) {
    return false;
  }
  return true;
}
