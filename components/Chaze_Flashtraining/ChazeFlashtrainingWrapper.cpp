//  TODO: Remove boiler plate code by creating function. Downside: Have to move declaration inside header.

#include <stdlib.h>
#include <stdint.h>
#include "ChazeFlashtrainingWrapper.h"
#include "ChazeFlashtraining.h"

struct FlashtrainingWrapper
{
    void * obj;
};

FlashtrainingWrapper_t * FlashtrainingWrapper_create(void)
{
    FlashtrainingWrapper *ft;

    ft = (FlashtrainingWrapper_t *) malloc (sizeof(*ft));
    ft->obj = global_ft;

    return ft;
}

void FlashtrainingWrapper_destroy(FlashtrainingWrapper *ft)
{
    if(ft==NULL)
        return;

    delete static_cast<Flashtraining *>(ft->obj);
    free(ft);
}

bool FlashtrainingWrapper_write_compressed_chunk(FlashtrainingWrapper *ft, uint8_t * data, uint32_t n)
{
    Flashtraining *obj;

    if(ft==NULL)
        return false;

    obj = static_cast<Flashtraining *>(ft->obj);
    return obj->write_compressed_chunk(data,n);
    
}

bool FlashtrainingWrapper_start_new_training(FlashtrainingWrapper_t *ft)
{
    Flashtraining *obj;

    if(ft==NULL)
        return false;

    obj = static_cast<Flashtraining *>(ft->obj);
    return obj->start_new_training();
}			

bool FlashtrainingWrapper_stop_training(FlashtrainingWrapper_t *ft)
{
    Flashtraining *obj;

    if(ft==NULL)
        return false;

    obj = static_cast<Flashtraining *>(ft->obj);
    return obj->stop_training();
}

int FlashtrainingWrapper_get_STATE(FlashtrainingWrapper_t * ft)
{
    Flashtraining *obj;

    if(ft==NULL)
        return -1;

    obj = static_cast<Flashtraining *>(ft->obj);
    return obj->get_STATE();
}

float FlashtrainingWrapper_readCalibration(FlashtrainingWrapper_t * ft, uint8_t storage_address)
{
    Flashtraining *obj;

    if(ft==NULL)
        return -1;

    obj = static_cast<Flashtraining *>(ft->obj);
    return obj->readCalibration(storage_address);
}

uint32_t FlashtrainingWrapper_get_number_of_unsynched_trainings(FlashtrainingWrapper_t * ft)
{
    Flashtraining *obj;

    if(ft==NULL)
        return -1;

    obj = static_cast<Flashtraining *>(ft->obj);
    return obj->meta_number_of_unsynced_trainings();
}