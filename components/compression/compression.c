#include "compression.h"
#include "assert.h"
#include "zlib.h"
#include <string.h>
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdlib.h"
#include "esp_log.h"


#include "ChazeFlashtrainingWrapper.h"

const char * TAG = "Chaze-Compression";

buffer_t ** buffers = NULL;

#define DEBUG_COMPRESSION 1

/**
 * @brief When recording sensor data, multiple tasks need to acquire a mutex in order to read from the bus and
 * write to the buffer. The buffer is a struct that holds a counter (number of bytes written) and a malloced
 * char array. If the number of bytes written equals to the BUFFER SIZE, this function is called and the
 * data is compressed and saved to the flash.
 * @param Buffer number to read from and compress. Either 0 or 1.
 */
void compress_and_save(FlashtrainingWrapper_t *ft, uint8_t buff_num, buffer_t ** buffers)
{
	
	buffer_t * current_buffer = buffers[buff_num];
	
	//Data written to flash.
	uint8_t * out = (uint8_t *) malloc(BUFFER_SIZE * sizeof(uint8_t));

	if(out == NULL){
		ESP_LOGE(TAG, "Could not allocate enough space");
		//Need to write the raw, uncompressed data to the flash instead
		return;
	} else{

		if(DEBUG_COMPRESSION) ESP_LOGI(TAG, "Start compressing.");
		//Writes 'written' many chars to the out buffer previously allocated
		uint32_t written = def(current_buffer->data, out, DEFAULT_COMPRESSION_LEVEL);

		//Write data to flash
		write_data_to_flash(ft, out, written);
		free(out);
	}
}

/**
 * @brief Deflates the char array given by source and writes to the char array dest.
 * @param source
 * @param dest
 * @param level Level of compression.
 * @return Number of written bytes to dest
 */
uint32_t def(uint8_t * source, uint8_t * dest, uint8_t level)
{
	int ret, flush;
	unsigned have;
	z_stream strm;

	uint8_t * in = (uint8_t *) malloc(CHUNK * sizeof(uint8_t));
	uint8_t * out = (uint8_t *) malloc(CHUNK * sizeof(uint8_t));

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = deflateInit2(&strm, level,Z_DEFLATED, WINDOW_SIZE ,MEM_LEVEL,Z_DEFAULT_STRATEGY);
	zerr(ret);

	uint32_t dest_offset = 0;
    do {
        memcpy(in, source, CHUNK);
        strm.avail_in = CHUNK; /*We assert that the buffer is full and since chunk size = buffer size, we can read that many bytes*/
        flush = Z_FINISH; //Immediately flush since we only compress one chunk.
        strm.next_in = in;

        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);
            assert(ret != Z_STREAM_ERROR);
            zerr(ret);
            have = CHUNK - strm.avail_out;
            if(DEBUG_COMPRESSION) ESP_LOGI(TAG, "Have: %d", have);
			if(DEBUG_COMPRESSION){ //Prints the compressed uint8_t array
				for(int i=0;i<have;i++)
            		printf("%d ", out[i]);
			}

            memcpy(dest+dest_offset, out, have);
            dest_offset += have;
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);

    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);

    zerr(deflateEnd(&strm));
    free(in);
    free(out);
    return dest_offset;
}

/*
* @brief Takes pointer to char array of data to be written to the flash and the number of chars to be written.
		 If DEBUG_COMPRESSION is defined, will also decompress the data and print it.
*/
void write_data_to_flash(FlashtrainingWrapper_t * ft, uint8_t * data, uint32_t n)
{

	if(DEBUG_COMPRESSION) ESP_LOGI(TAG, "Writing data to flash.");

	bool err = !FlashtrainingWrapper_write_compressed_chunk(ft, data, n);

	if(err){
		ESP_LOGE(TAG, "Error ocurred while writing to flash.");
	} else {
		if(DEBUG_COMPRESSION) ESP_LOGI(TAG, "Written to flash.");
	}

	if(DEBUG_COMPRESSION)
	{
		int ret;
		unsigned have;
		z_stream strm;

		unsigned char * in = (unsigned char *) malloc(CHUNK * sizeof(char));
		unsigned char * out = (unsigned char *) malloc(CHUNK * sizeof(char));


		uint8_t * to_print = (uint8_t *) malloc(CHUNK*sizeof(uint8_t));

		if(to_print == NULL || in == NULL || out == NULL)
		{
			ESP_LOGE(TAG, "Could not allocate enough space");
			return;
		}

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;

		ret = inflateInit2(&strm,0); //Use window size in compressed stream
		zerr(ret);
		uint32_t to_print_offset = 0;

		do {
				memcpy(in, data, n);
				strm.avail_in = n;

				if (strm.avail_in == 0)
					break;
				strm.next_in = in;

				do {
					strm.avail_out = CHUNK;
					strm.next_out = out;

					ret = inflate(&strm, Z_NO_FLUSH);
					assert(ret != Z_STREAM_ERROR);
					zerr(ret);

					have = CHUNK - strm.avail_out;
					memcpy(to_print+to_print_offset, out, have);
					to_print_offset+=have;

				} while (strm.avail_out == 0);
			} while (ret != Z_STREAM_END);

		(void)inflateEnd(&strm);
		free(in);
		free(out);

		for(int i=0;i<CHUNK;i++)
			printf("%d ", to_print[i]);
		
		ESP_LOGI(TAG, "In character form");

		}
}


void zerr(int ret)
{
    switch (ret) {
    case Z_ERRNO:
        ESP_LOGE(TAG, "Z err no");
        break;
    case Z_STREAM_ERROR:
    	ESP_LOGE(TAG, "Invalid compression level");
        break;
    case Z_DATA_ERROR:
    	ESP_LOGE(TAG, "Data error");
        break;
    case Z_MEM_ERROR:
    	ESP_LOGE(TAG, "Mem error");
        break;
    case Z_VERSION_ERROR:
    	ESP_LOGE(TAG, "Version error");
    }
}
