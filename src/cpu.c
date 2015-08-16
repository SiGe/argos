#include <stdio.h>
#include <stdlib.h>

#include "history.h"
#include "snapshot.h"
#include "transform.h"
#include "util.h"

#include "cpu.h"

void cpu_snapshot_create(cpu_snapshot **snap) {
    cpu_snapshot *tmp = *snap = (cpu_snapshot*)(malloc(sizeof(cpu_snapshot)));

    history_create(&tmp->cpu, "proc/stat/cpu");
    history_create(&tmp->ctxt, "proc/stat/ctxt");
    history_create(&tmp->iowait, "proc/stat/iowait");

    tmp->cpu->transform = transform_delta;
    tmp->ctxt->transform = transform_delta;
    tmp->iowait->transform = transform_delta;
}

void cpu_snapshot_delete(cpu_snapshot *snap) {
    history_save(snap->cpu);
    history_save(snap->ctxt);
    history_save(snap->iowait);

    history_delete(snap->cpu);
    history_delete(snap->ctxt);
    history_delete(snap->iowait);

    free(snap);
}

void cpu_snapshot_tick(cpu_snapshot *cpu) {
    snapshot *snap = 0;

    if (snapshot_create("/proc/stat", &snap) < 0) {
        fprintf(stderr, "Failed to save /proc/stat.\n");
        snapshot_delete(snap);
        return;
    }

    char const *data = snap->data;
    char buf[32];

    while (data) {
        uint64_t t1;
        column(data, 0, buf, 32);

        /* save the total number of jiffies spent in cpu user+system time */
        if (equal(buf, "cpu")) {
            /* Get the user time */
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);
            
            /* Get the system time */
            column(data, 3, buf, 32);
            t1 += strtoull(buf, 0, 10);

            history_append(cpu->cpu, snap->time, t1);

            /* Get the system time */
            column(data, 5, buf, 32);
            t1 = strtoull(buf, 0, 10);
            history_append(cpu->iowait, snap->time, t1);
        }

        /* save the number of context switches */
        if (equal(buf, "ctxt")) {
            column(data, 1, buf, 32);
            t1 = strtoull(buf, 0, 10);

            history_append(cpu->ctxt, snap->time, t1);
        }

        data = next_line(data);
    }

    snapshot_delete(snap);
}
