#include "Chaze_Realtime.h"
#include "Chaze_RealtimeWrapper.h"

struct RV3028Wrapper {
    void * obj;
};

RV3028Wrapper_t * RV3028Wrapper_create(void) {
    RV3028Wrapper *rv;
    RV3028 *obj;

    rv = (RV3028Wrapper_t *) malloc (sizeof(*rv));

    obj = new RV3028();
    rv->obj = obj;

    return rv;
}

void RV3028Wrapper_destroy(RV3028Wrapper *rv)
{
    if(rv==NULL)
        return;

    delete static_cast<RV3028 *>(rv->obj);
    free(rv);
}

bool RV3028Wrapper_begin(RV3028Wrapper *rv, i2c_port_t port)
{
    RV3028 *obj;

    if(rv==NULL)
        return false;

    obj = static_cast<RV3028 *>(rv->obj);
    return obj->begin(port);
}