#ifndef _CPU_H_
#define _CPU_H_

#include "history.h"

typedef struct
cpu_snapshot {
    history *cpu;
    history *ctxt;
} cpu_snapshot;

void cpu_snapshot_create(cpu_snapshot **);
void cpu_snapshot_delete(cpu_snapshot *);
void cpu_snapshot_tick(cpu_snapshot *);

#endif /* _CPU_H_ */
