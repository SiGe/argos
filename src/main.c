#define _BSD_SOURCE

#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "cpu.h"
#include "disk.h"
#include "memory.h"
#include "net.h"

cpu_snapshot *cpu = 0;
memory_snapshot *mem = 0;
disks_snapshot *disks = 0;
links_snapshot *links = 0;

uint32_t freq = 0;
#define MONITOR_FREQ 10

void monitors_create(void) {
    if (freq++ > MONITOR_FREQ) {
        cpu_snapshot_create(&cpu);
        memory_snapshot_create(&mem);
        disks_snapshot_create(&disks);
        freq = 0;
    }

    links_snapshot_create(&links);
}

void monitors_delete(void) {
    cpu_snapshot_delete(cpu);
    memory_snapshot_delete(mem);
    disks_snapshot_delete(disks);
    links_snapshot_delete(links);
}

void monitors_tick(void) {
    cpu_snapshot_tick(cpu);
    memory_snapshot_tick(mem);
    disks_snapshot_tick(disks);
    links_snapshot_tick(links);
}

void sig_handler(int signo) {
    if (signo == SIGINT)
        monitors_delete();
    exit(1);
}

int main(int argc, char **argv) {
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        fprintf(stderr, "Failed to handle SIGINT.\n");
        return -1;
    }

    monitors_create();

    while (true) {
        monitors_tick();
        usleep(10000);
    }

    monitors_delete();
    return 0;
}
