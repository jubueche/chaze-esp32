#include "compression.h"
#include "assert.h"
#include "zlib.h"
#include <string.h>
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdlib.h"
#include "esp_log.h"

const char * TAG = "Chaze-Compression";

/**
 * @brief When recording sensor data, multiple tasks need to acquire a mutex in order to read from the bus and
 * write to the buffer. The buffer is a struct that holds a counter (number of bytes written) and a malloced
 * char array. If the number of bytes written equals to the BUFFER SIZE, this function is called and the
 * data is compressed and saved to the flash.
 * @param Buffer number to read from and compress. Either 0 or 1.
 */
void compress_and_save(uint8_t buff_num)
{
	ESP_LOGI(TAG, "Using buffer number %d", buff_num);
	//Get the pointer to the current buffer
	buffer_t * current_buffer = buffers[buff_num];
	assert(current_buffer->counter == BUFFER_SIZE); //The buffer must be filled completely.

	//Data written to flash.
	char * out = (char *) malloc(BUFFER_SIZE * sizeof(char));

	if(out == NULL){
		ESP_LOGE(TAG, "Could not allocate enough space");
		//Need to write the raw, uncompressed data to the flash instead
		return;
	} else{

		ESP_LOGI(TAG, "Start compressing.");
		//Writes 'written' many chars to the out buffer previously allocated
		uint32_t written = def(current_buffer->data, out, DEFAULT_COMPRESSION_LEVEL);

		//Write data to flash
		write_data_to_flash(out, written);
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
uint32_t def(char * source, char * dest, uint8_t level)
{
	int ret, flush;
	unsigned have;
	z_stream strm;

	unsigned char * in = (unsigned char *) malloc(CHUNK * sizeof(char));
	unsigned char * out = (unsigned char *) malloc(CHUNK * sizeof(char));

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
            ESP_LOGI(TAG, "Have: %d", have);
            for(int i=0;i<have;i++)
            	printf("%c", out[i]);

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

//For now just inflates data to check if was deflated correctly
void write_data_to_flash(char * data, uint32_t n)
{
	int ret;
	unsigned have;
	z_stream strm;

	unsigned char * in = (unsigned char *) malloc(CHUNK * sizeof(char));
	unsigned char * out = (unsigned char *) malloc(CHUNK * sizeof(char));


	char * to_print = (char *) malloc(CHUNK*sizeof(char));

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
		printf("%c", to_print[i]);
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