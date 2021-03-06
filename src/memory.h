#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "history.h"

typedef struct
memory_snapshot {
    history *mem_available;
    history *mem_free;
    history *mem_total;

    history *swap_free;
    history *swap_total;

    history *cached;
} memory_snapshot;

void memory_snapshot_create(memory_snapshot **);
void memory_snapshot_delete(memory_snapshot *);
void memory_snapshot_tick(memory_snapshot *);

#endif /* _MEMORY_H_ */
