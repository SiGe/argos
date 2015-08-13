#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"
#include "snapshot.h"
#include "transform.h"
#include "util.h"

#include "disk.h"

static void
disk_create(disk_snapshot **_disk, const char *name) {
    disk_snapshot *disk = *_disk = (disk_snapshot*)(malloc(sizeof(disk_snapshot)));

    disk->name = (char *)malloc(strlen(name) + 1);
    strcpy(disk->name, name);

    char buf[256] = {0};

    *buf = 0;
    strcat(buf, "proc/disk/");
    strcat(buf, name);
    strcat(buf, "/read_time");
    history_create(&disk->read_time, buf);
    disk->read_time->transform = transform_delta;
    
    *buf = 0;
    strcat(buf, "proc/disk/");
    strcat(buf, name);
    strcat(buf, "/write_time");
    history_create(&disk->write_time, buf);
    disk->write_time->transform = transform_delta;

    *buf = 0;
    strcat(buf, "proc/disk/");
    strcat(buf, name);
    strcat(buf, "/io_time");
    history_create(&disk->io_time, buf);
    disk->io_time->transform = transform_delta;


    *buf = 0;
    strcat(buf, "proc/disk/");
    strcat(buf, name);
    strcat(buf, "/weighted_io_time");
    history_create(&disk->weighted_io_time, buf);
    disk->weighted_io_time->transform = transform_delta;

    disk->next = 0;
}

static disk_snapshot *
disk_find_or_create(disk_snapshot **list, char const *name) {
    disk_snapshot *tmp = *list;
    while (tmp) {
        if (strcmp(tmp->name, name) == 0)
            return tmp;

        tmp = tmp->next;
    }

    tmp = 0;
    disk_create(&tmp, name);

    tmp->next = *list;
    *list = tmp;

    return tmp;
}

static void
disk_delete(disk_snapshot *disk) {
    history_save(disk->read_time);
    history_save(disk->write_time);
    history_save(disk->io_time);
    history_save(disk->weighted_io_time);

    free(disk->name);

    history_delete(disk->read_time);
    history_delete(disk->write_time);
    history_delete(disk->io_time);
    history_delete(disk->weighted_io_time);

    free(disk);
}

void disks_snapshot_create(disks_snapshot **snap) {
    *snap = (disks_snapshot *)malloc(sizeof(disks_snapshot));
    (*snap)->disks = 0;
}

void disks_snapshot_delete(disks_snapshot *snap) {
    disk_snapshot *disk = snap->disks;
    disk_snapshot *tmp  = disk;

    while (disk) {
        tmp = disk->next;
        disk_delete(disk);
        disk = tmp;
    }

    free(snap);
}

void disks_snapshot_tick(disks_snapshot *disks) {
    snapshot *snap = 0;

    if (snapshot_create("/proc/diskstats", &snap) < 0) {
        fprintf(stderr, "Failed to save /proc/diskstats.\n");
        snapshot_delete(snap);
        return;
    }

    char const *data = snap->data;
    char buf[32];

    while (data) {
        uint64_t t1;
        column(data, 2, buf, 32);

        /* save the total number of jiffies spent in memory user+system time */
        if (startswith(buf, "xvd") || startswith(buf, "hd") || startswith(buf, "sd")) {
            disk_snapshot *disk = disk_find_or_create(&disks->disks, buf);

            /* Get the read time in ms */
            column(data, 6, buf, 32);
            t1 = strtoull(buf, 0, 10);
            history_append(disk->read_time, snap->time, t1);

            /* Get the write time in ms */
            column(data, 10, buf, 32);
            t1 = strtoull(buf, 0, 10);
            history_append(disk->write_time, snap->time, t1);

            /* Get the total io in ms */
            column(data, 12, buf, 32);
            t1 = strtoull(buf, 0, 10);
            history_append(disk->io_time, snap->time, t1);

            /* Get the total weighted io in ms */
            column(data, 13, buf, 32);
            t1 = strtoull(buf, 0, 10);
            history_append(disk->weighted_io_time, snap->time, t1);
        }
        data = next_line(data);
    }

    snapshot_delete(snap);
}

