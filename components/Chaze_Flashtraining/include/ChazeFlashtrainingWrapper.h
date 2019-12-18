#ifndef __CHAZEFLASHTRAININGWRAPPER_H__
#define __CHAZEFLASHTRAININGWRAPPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>


struct FlashtrainingWrapper;

typedef struct FlashtrainingWrapper FlashtrainingWrapper_t;

FlashtrainingWrapper_t *FlashtrainingWrapper_create();

void FlashtrainingWrapper_destroy(FlashtrainingWrapper_t *);

bool FlashtrainingWrapper_write_compressed_chunk(FlashtrainingWrapper_t *, uint8_t *, uint32_t);
bool FlashtrainingWrapper_start_new_training(FlashtrainingWrapper_t *);																							
bool FlashtrainingWrapper_stop_training(FlashtrainingWrapper_t *);
int FlashtrainingWrapper_get_STATE(FlashtrainingWrapper_t *);
float FlashtrainingWrapper_readCalibration(FlashtrainingWrapper_t *, uint8_t);
uint32_t FlashtrainingWrapper_get_number_of_unsynched_trainings(FlashtrainingWrapper_t *);

#ifdef __cplusplus
}
#endif

#endif /* __FLASHTRAININGWRAPPER_H__ */