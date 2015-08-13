#include <stdio.h>
#include <stdlib.h>

#include "history.h"
#include "snapshot.h"
#include "transform.h"
#include "util.h"

#include "memory.h"

void memory_snapshot_create(memory_snapshot **snap) {
    memory_snapshot *tmp = *snap = (memory_snapshot*)(malloc(sizeof(memory_snapshot)));

    history_create(&tmp->mem_available, "proc/memory/available");
    history_create(&tmp->cached, "proc/memory/cached");

    tmp->mem_available->transform = transform_identity;
    tmp->cached->transform = transform_identity;
}

void memory_snapshot_delete(memory_snapshot *snap) {
    history_save(snap->mem_available);
    history_save(snap->cached);

    history_delete(snap->mem_available);
    history_delete(snap->cached);

    free(snap);
}

void memory_snapshot_tick(memory_snapshot *memory) {
    snapshot *snap = 0;

    if (snapshot_create("/proc/meminfo", &snap) < 0) {
        fprintf(stderr, "Failed to save /proc/meminfo");
        snapshot_delete(snap);
        return;
    }

    char const *data = snap->data;
    char buf[32];

    while (data) {
        uint64_t t1;
        column(data, 0, buf, 32);

        /* save the total number of jiffies spent in memory user+system time */
        if (startswith(buf, "MemAvailable")) {
            /* Get the available memory */
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);
            
            history_append(memory->mem_available, snap->time, t1);
        }

        /* save the number of context switches */
        if (startswith(buf, "Cached")) {
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);

            history_append(memory->cached, snap->time, t1);
        }

        data = next_line(data);
    }

    snapshot_delete(snap);
}
