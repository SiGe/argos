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
    history_create(&tmp->mem_free, "proc/memory/free");
    history_create(&tmp->mem_total, "proc/memory/total");

    history_create(&tmp->swap_free, "proc/memory/swap_free");
    history_create(&tmp->swap_total, "proc/memory/swap_total");

    history_create(&tmp->cached, "proc/memory/cached");

    tmp->mem_available->transform = transform_identity;
    tmp->mem_free->transform = transform_identity;
    tmp->cached->transform = transform_identity;
}

void memory_snapshot_delete(memory_snapshot *snap) {
    history_save(snap->mem_available);
    history_save(snap->mem_free);
    history_save(snap->mem_total);
    history_save(snap->swap_free);
    history_save(snap->swap_total);
    history_save(snap->cached);

    history_delete(snap->mem_available);
    history_delete(snap->mem_free);
    history_delete(snap->mem_total);
    history_delete(snap->swap_free);
    history_delete(snap->swap_total);
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

        /* save the available memory for spawning new processes ... available after kernel 3.14 */
        if (startswith(buf, "MemAvailable")) {
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);
            
            history_append(memory->mem_available, snap->time, t1);
        }

        /* save the free memory */
        if (startswith(buf, "MemFree")) {
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);
            
            history_append(memory->mem_free, snap->time, t1);
        }

        /* save the total memory */
        if (startswith(buf, "MemTotal")) {
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);
            
            history_append(memory->mem_total, snap->time, t1);
        }

        /* save the free swap */
        if (startswith(buf, "SwapFree")) {
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);
            
            history_append(memory->swap_free, snap->time, t1);
        }

        /* save the total swap */
        if (startswith(buf, "SwapTotal")) {
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);
            
            history_append(memory->swap_total, snap->time, t1);
        }

        /* save the amount of cached spaces for files */
        if (startswith(buf, "Cached")) {
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);

            history_append(memory->cached, snap->time, t1);
        }

        data = next_line(data);
    }

    snapshot_delete(snap);
}
