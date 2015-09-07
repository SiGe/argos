#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"
#include "snapshot.h"
#include "transform.h"
#include "util.h"

#include "cpu.h"

static void
cpu_history_create(cpu_history **cpu, char const* prefix) {
    cpu_history *tmp = *cpu = (cpu_history *)(malloc(sizeof(cpu_history)));

    char buffer[256] = {0};
    memset(buffer, 0, sizeof(buffer));
    strcat(buffer, prefix);
    strcat(buffer, "/cpu");
    history_create(&tmp->cpu, buffer);

    memset(buffer, 0, sizeof(buffer));
    strcat(buffer, prefix);
    strcat(buffer, "/sys");
    history_create(&tmp->sys, buffer);

    memset(buffer, 0, sizeof(buffer));
    strcat(buffer, prefix);
    strcat(buffer, "/user");
    history_create(&tmp->user, buffer);

    memset(buffer, 0, sizeof(buffer));
    strcat(buffer, prefix);
    strcat(buffer, "/iowait");
    history_create(&tmp->iowait, buffer);

    memset(buffer, 0, sizeof(buffer));
    strcat(buffer, prefix);
    strcat(buffer, "/idle");
    history_create(&tmp->idle, buffer);

    memset(buffer, 0, sizeof(buffer));
    strcat(buffer, prefix);
    strcat(buffer, "/steal");
    history_create(&tmp->steal, buffer);

    tmp->cpu->transform = transform_delta;
    tmp->sys->transform = transform_delta;
    tmp->user->transform = transform_delta;
    tmp->idle->transform = transform_delta;
    tmp->steal->transform = transform_delta;
    tmp->iowait->transform = transform_delta;
}

static void
cpu_history_delete(cpu_history *cpu) {
    history_save(cpu->cpu);
    history_save(cpu->sys);
    history_save(cpu->user);
    history_save(cpu->iowait);
    history_save(cpu->steal);
    history_save(cpu->idle);

    history_delete(cpu->cpu);
    history_delete(cpu->sys);
    history_delete(cpu->user);
    history_delete(cpu->iowait);
    history_delete(cpu->steal);
    history_delete(cpu->idle);
}


void cpu_snapshot_create(cpu_snapshot **snap) {
    cpu_snapshot *tmp = *snap = (cpu_snapshot*)(malloc(sizeof(cpu_snapshot)));

    history_create(&tmp->ctxt, "proc/stat/ctxt");
    tmp->ctxt->transform = transform_delta;

    for (unsigned i = 0; i < MAX_CPU; ++i) {
        char prefix[256] = "proc/stat/cpu";
        char cpu_id[32] = {0};

        snprintf(cpu_id, 31, "%d", i);
        strcat(prefix, cpu_id);

        cpu_history_create(&(tmp->cpus[i]), prefix);
    }
    cpu_history_create(&(tmp->main), "proc/stat/cpu");
}

void cpu_snapshot_delete(cpu_snapshot *snap) {
    for (unsigned i = 0; i < MAX_CPU; ++i) {
        cpu_history_delete(snap->cpus[i]);
    }
    cpu_history_delete(snap->main);

    history_save(snap->ctxt);
    history_delete(snap->ctxt);

    free(snap);
}

static void
cpu_save_snapshot_to_history(char const *data, uint64_t time, cpu_history *cpu) {
    char buf[32];
    uint64_t t1, t2;

    column(data, 1, buf, 32);
    t1 = strtoull(buf, 0, 10);
    history_append(cpu->user, time, t1);
    
    /* Get the system time */
    column(data, 3, buf, 32);
    t2 = strtoull(buf, 0, 10);
    history_append(cpu->sys, time, t2);
    history_append(cpu->cpu, time, t1+t2);

    /* Get the idle time */
    column(data, 4, buf, 32);
    t1 = strtoull(buf, 0, 10);
    history_append(cpu->idle, time, t1);

    /* Get the io_wait time */
    column(data, 5, buf, 32);
    t1 = strtoull(buf, 0, 10);
    history_append(cpu->iowait, time, t1);

    /* Get the steal time */
    column(data, 8, buf, 32);
    t1 = strtoull(buf, 0, 10);
    history_append(cpu->steal, time, t1);
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
            cpu_save_snapshot_to_history(data, snap->time, cpu->main);
        } else if (startswith(buf, "cpu")) {
            int cpu_num = atoi(buf+3);
            cpu_save_snapshot_to_history(data, snap->time, cpu->cpus[cpu_num]);
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
