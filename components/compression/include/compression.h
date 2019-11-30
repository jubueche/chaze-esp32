#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "stdint.h"
#include "ChazeFlashtrainingWrapper.h"

#define BUFFER_SIZE /*16384*/ 8192
#define CHUNK 8192 /*Must be same size as BUFFER_SIZE*/
#define DEFAULT_COMPRESSION_LEVEL 6
#define WINDOW_SIZE 9
#define MEM_LEVEL 1
#define GZIP_ENCODING 16

typedef struct {
	uint32_t counter;
	uint8_t * data;
}buffer_t;

extern buffer_t ** buffers;

void compress_and_save(FlashtrainingWrapper_t *, uint8_t, buffer_t **);
void write_data_to_flash(FlashtrainingWrapper_t *, uint8_t *, uint32_t);
uint32_t def(uint8_t *, uint8_t *, uint8_t);
void zerr(int32_t);

#endif