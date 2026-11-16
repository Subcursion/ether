#ifndef ETHER_RC_H
#define ETHER_RC_H

#define _ERROR(val) val << 8 // move to be higher than errno

#define RC_LIST                       \
    X(ETHER_SUCCESS, 0x0)             \
    X(ETHER_EPARAM, _ERROR(0x1))      \
    X(ETHER_ESDL_INIT, _ERROR(0x100)) \
    X(ETHER_EVK_INIT, _ERROR(0x200))  \
    X(ETHER_ERENDER, _ERROR(0x300))

typedef enum
{
#define X(name, val) name = val,
    RC_LIST
#undef X
} ether_rc;

const char *ether_rc_str(ether_rc rc);

#include "ether/logging.h"

#define PARAM_CHECK(check, do_what, err_action)           \
    do                                                    \
    {                                                     \
        if (!(check))                                     \
        {                                                 \
            ETHER_ELOGV("Invalid parameter: %s", #check); \
            do_what;                                      \
            err_action;                                   \
        }                                                 \
    } while (0)

#endif