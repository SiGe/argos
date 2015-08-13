#ifndef _DISK_H_
#define _DISK_H_

#include "history.h"

typedef struct
disk_snapshot {
    char *name;

    history *read_time;
    history *write_time;

    history *io_time;
    history *weighted_io_time;

    struct disk_snapshot *next;
} disk_snapshot;

typedef struct
disks_snapshot {
    disk_snapshot *disks;
} disks_snapshot;

void disks_snapshot_create(disks_snapshot **);
void disks_snapshot_delete(disks_snapshot *);
void disks_snapshot_tick(disks_snapshot *);

#endif /* _DISK_H_ */
