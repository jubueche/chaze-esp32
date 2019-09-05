#include "Chaze_Training.h"


Training::Training(const char * date_recorded, uint32_t size, bool synched, unsigned long write_offset, unsigned long read_offset, uint32_t page_offset)
{
    _date_recorded = date_recorded;
    _size = size;
    _synched = synched;
    _write_offset = write_offset;
    _read_offset = read_offset;
    _page_offset = page_offset;

}