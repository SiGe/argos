#ifndef _SNAPSHOT_H_
#define _SNAPSHOT_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct snapshot {
    char        *data;
    uint32_t    length;
    uint64_t    time;
    int         error;
    bool        truncated;
} snapshot;

int snapshot_create(char const *, snapshot **);
void snapshot_delete(snapshot *);

#endif /* _SNAPSHOT_H_ */

