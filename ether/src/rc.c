#include "ether/rc.h"

#include <errno.h>
#include <string.h>

const char *ether_rc_str(ether_rc rc)
{
#define X(name, val) \
    case val:        \
        return #name;

    switch (rc)
    {
        RC_LIST
    default:
        if (rc < _ERROR(0x0))
        {
            return strerror(rc);
        }
        return "UNKNOWN";
    }
#undef X
}