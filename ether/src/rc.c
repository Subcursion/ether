#include "ether/rc.h"

const char *ether_rc_str(ether_rc rc)
{
#define X(name, val) \
    case val:        \
        return #name;

    switch (rc)
    {
        RC_LIST
    default:
        return "UNKNOWN";
    }
#undef X
}