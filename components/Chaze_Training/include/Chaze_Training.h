#ifndef __CHAZE_TRAINING_H__
#define __CHAZE_TRAINING_H__

#include "stdint.h"

class Training {
    public:
        Training(const char *, uint32_t, bool, unsigned long, unsigned long, uint32_t);
    
    private:
        const char * _date_recorded;
        uint32_t _size;
        bool _synched;
        unsigned long _write_offset;
        unsigned long _read_offset;
        uint32_t _page_offset;

};

#endif