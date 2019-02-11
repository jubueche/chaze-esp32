#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdlib.h"
#include "stdio.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"

#include "esp_vfs_fat.h"
#include "diskio.h"

#include <dirent.h>
#include <errno.h>

const char * base_path =  "/spiflash";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
const char * TAG = "FATFS";

void print_dir(void);
void zerr(int ret);
void remove_folder(void);

#define WINDOW_SIZE 9
#define MEM_LEVEL 1
#define GZIP_ENCODING 16

#define CHUNK 16384
FILE * foo;
FILE * comp;
FILE * recon;


int def(FILE *source, FILE *dest, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
	//Allocating on the stack only works for very small chunk sizes.
    /*unsigned char in[CHUNK];
    unsigned char out[CHUNK];*/

    unsigned char * in = (unsigned char *) malloc(CHUNK * sizeof(char));
    unsigned char * out = (unsigned char *) malloc(CHUNK * sizeof(char));

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit2(&strm, level,Z_DEFLATED, WINDOW_SIZE ,MEM_LEVEL,Z_DEFAULT_STRATEGY);
    if (ret != Z_OK){
    	return ret;
    }

    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            zerr(deflateEnd(&strm));
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);
            assert(ret != Z_STREAM_ERROR);
            zerr(ret);
            have = CHUNK - strm.avail_out;
            for(int i=0;i<have;i++)
            	printf("%c", out[i]);

            if (fwrite(out, sizeof(char), have, dest) != have || ferror(dest)) {
                zerr(deflateEnd(&strm));
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);

    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);

    zerr(deflateEnd(&strm));
    free(in);
    free(out);
    return Z_OK;
}


int inf(FILE *source, FILE *dest)
{
	int ret;
	unsigned have;
	z_stream strm;
	//Allocating on the stack only works for very small chunk sizes.
    /*unsigned char in[CHUNK];
    unsigned char out[CHUNK];*/

    unsigned char * in = (unsigned char *) malloc(CHUNK * sizeof(char));
    unsigned char * out = (unsigned char *) malloc(CHUNK * sizeof(char));

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	ret = inflateInit2(&strm,0); //Use window size in compressed stream
	if (ret != Z_OK)
		return ret;

	ESP_LOGI(TAG, "Inited Inflation");

	do {
		strm.avail_in = fread(in, 1, CHUNK, source);
		ESP_LOGI(TAG, "Could read");

		if (ferror(source)) {
			(void)inflateEnd(&strm);
			return Z_ERRNO;
		}
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;

			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}

			have = CHUNK - strm.avail_out;
			if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
				(void)inflateEnd(&strm);
				return Z_ERRNO;
			}

		} while (strm.avail_out == 0);
	} while (ret != Z_STREAM_END);

		(void)inflateEnd(&strm);
	    free(in);
	    free(out);
		return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
	}




extern "C" void app_main()
{

	esp_vfs_fat_mount_config_t mount_config = {};
	mount_config.max_files = 4;
	mount_config.format_if_mount_failed = true;
	mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
		return;
	}

	print_dir();

	remove_folder();

	print_dir();


	ESP_LOGI(TAG, "Opening file");
	foo = fopen("/spiflash/foo.bin", "wb");
	if (foo == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	//int ret = fprintf(foo, "written using ESP-IDF\n");
	const char * tmp = esp_get_idf_version();
	int ret = fwrite(tmp,1,strlen(tmp),foo);
	if(ret < strlen(tmp)){
		ESP_LOGE(TAG, "Error writing file");
	}
	if(fclose(foo) == EOF){
		ESP_LOGE(TAG, "Error closing the file.");
	}
	else ESP_LOGI(TAG, "File written");

	comp = fopen("/spiflash/c.bin", "wb");
	if (comp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}

	foo = fopen("/spiflash/foo.bin", "rb");
	if(foo == NULL){
		ESP_LOGE(TAG, "Error opening the file.");
	}

	print_dir();

	uint8_t level = 6;
	zerr(def(foo, comp, level));

	ESP_LOGI(TAG, "Compressed.");

	if(fclose(foo) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	else ESP_LOGI(TAG, "Closed foo.");

	if(fclose(comp) == EOF){
		ESP_LOGE(TAG, "Error closing file: %s", strerror(errno));
	}
	else ESP_LOGI(TAG, "Closed comp.");


	//Read comp
	char line[128];
	char *pos;
	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	comp = fopen("/spiflash/c.bin", "rb");
	if (comp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
	}
	fgets(line, sizeof(line), foo);
	fclose(comp);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);


	//Inflate
	recon = fopen("/spiflash/rec.bin", "wb");
	comp = fopen("/spiflash/c.bin", "rb");
	if(recon == NULL || comp == NULL){
		ESP_LOGE(TAG, "Error opening file before inflating.");
	}

	ESP_LOGI(TAG, "Start inflating");

	zerr(inf(comp, recon));

	ESP_LOGI(TAG, "Stopped inflating");

	if(fclose(comp) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	ESP_LOGI(TAG, "Closed comp");

	if(fclose(recon) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	ESP_LOGI(TAG, "Closed recon");


	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	recon = fopen("/spiflash/rec.bin", "rb");
	if (recon == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
	}
	fgets(line, sizeof(line), recon);
	fclose(recon);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);


	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	foo = fopen("/spiflash/foo.bin", "rb");
	if (foo == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return;
	}
	fgets(line, sizeof(line), foo);
	fclose(foo);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);

	// Unmount FATFS
	ESP_LOGI(TAG, "Unmounting FAT filesystem");
	ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle));

	ESP_LOGI(TAG, "Done");

	while(1){
		vTaskDelay(1000);
	}
}

void print_dir(void)
{
	ESP_LOGI(TAG, "Directory content:");
	DIR *d;
	struct dirent *dir;
	d = opendir("/spiflash");
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			ESP_LOGI(TAG, "%s\n", dir->d_name);
		}
	closedir(d);
	}
}

void remove_folder(void)
{
	DIR *theFolder = opendir("/spiflash");
	struct dirent *next_file;
	char filepath[256];

	while ( (next_file = readdir(theFolder)) != NULL )
	{
		// build the path for each file in the folder
		sprintf(filepath, "%s/%s", "/spiflash", next_file->d_name);
		remove(filepath);
	}
	closedir(theFolder);
}

/* report a zlib or i/o error */
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

/*esp_vfs_fat_mount_config_t mount_config = {};
	mount_config.max_files = 4;
	mount_config.format_if_mount_failed = true;
	mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
		return;
	}

	print_dir();

	remove_folder();

	print_dir();


	ESP_LOGI(TAG, "Opening file");
	foo = fopen("/spiflash/foo.bin", "wb");
	if (foo == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	//int ret = fprintf(foo, "written using ESP-IDF\n");
	const char * tmp = esp_get_idf_version();
	int ret = fwrite(tmp,1,strlen(tmp),foo);
	if(ret < strlen(tmp)){
		ESP_LOGE(TAG, "Error writing file");
	}
	if(fclose(foo) == EOF){
		ESP_LOGE(TAG, "Error closing the file.");
	}
	else ESP_LOGI(TAG, "File written");

	comp = fopen("/spiflash/c.bin", "wb");
	if (comp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}

	foo = fopen("/spiflash/foo.bin", "rb");
	if(foo == NULL){
		ESP_LOGE(TAG, "Error opening the file.");
	}

	print_dir();

	uint8_t level = 6;
	zerr(def(foo, comp, level));

	ESP_LOGI(TAG, "Compressed.");

	if(fclose(foo) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	else ESP_LOGI(TAG, "Closed foo.");

	if(fclose(comp) == EOF){
		ESP_LOGE(TAG, "Error closing file: %s", strerror(errno));
	}
	else ESP_LOGI(TAG, "Closed comp.");


	//Read comp
	char line[128];
	char *pos;
	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	comp = fopen("/spiflash/c.bin", "rb");
	if (comp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
	}
	fgets(line, sizeof(line), foo);
	fclose(comp);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);


	//Inflate
	recon = fopen("/spiflash/rec.bin", "wb");
	comp = fopen("/spiflash/c.bin", "rb");
	if(recon == NULL || comp == NULL){
		ESP_LOGE(TAG, "Error opening file before inflating.");
	}

	ESP_LOGI(TAG, "Start inflating");

	zerr(inf(comp, recon));

	ESP_LOGI(TAG, "Stopped inflating");

	if(fclose(comp) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	ESP_LOGI(TAG, "Closed comp");

	if(fclose(recon) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	ESP_LOGI(TAG, "Closed recon");


	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	recon = fopen("/spiflash/rec.bin", "rb");
	if (recon == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
	}
	fgets(line, sizeof(line), recon);
	fclose(recon);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);


	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	foo = fopen("/spiflash/foo.bin", "rb");
	if (foo == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return;
	}
	fgets(line, sizeof(line), foo);
	fclose(foo);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);

	// Unmount FATFS
	ESP_LOGI(TAG, "Unmounting FAT filesystem");
	ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle));

	ESP_LOGI(TAG, "Done");

	while(1){
		vTaskDelay(1000);
	}
	*/
