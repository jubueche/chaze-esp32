#ifndef __CHAZE_REALTIME_H__
#define __CHAZE_REALTIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"

struct RV3028Wrapper;

typedef struct RV3028Wrapper RV3028Wrapper_t;

RV3028Wrapper_t *RV3028Wrapper_create();

void RV3028Wrapper_destroy(RV3028Wrapper_t *);
bool RV3028Wrapper_begin(RV3028Wrapper_t *, i2c_port_t);

#ifdef __cplusplus
}
#endif

#endif /* __CHAZE_REALTIME_H__ */