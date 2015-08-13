#include <stdint.h>
#include <sys/time.h>

#include "time.h"

uint64_t unified_time() {
    /* TODO: either use ntpd or make an ntpd offset alignemnt here */
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}
