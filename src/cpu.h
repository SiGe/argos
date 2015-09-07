#ifndef _CPU_H_
#define _CPU_H_

#include "history.h"

#define MAX_CPU 64

typedef struct
cpu_history {
    history *cpu;
    history *sys;
    history *user;
    history *iowait;
    history *idle;
    history *steal;
} cpu_history;

typedef struct
cpu_snapshot {
    cpu_history *main;
    cpu_history *cpus[MAX_CPU];

    history *ctxt;
} cpu_snapshot;

void cpu_snapshot_create(cpu_snapshot **);
void cpu_snapshot_delete(cpu_snapshot *);
void cpu_snapshot_tick(cpu_snapshot *);

#endif /* _CPU_H_ */
