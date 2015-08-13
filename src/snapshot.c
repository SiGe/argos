#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "time.h"

#include "snapshot.h"

#define MAX_SNAPSHOT_SIZE 65536

int
snapshot_create(char const *fname, snapshot **snp) {
    /* Allocate space for snapshot */
    snapshot *snap = *snp = (snapshot *)malloc(sizeof(snapshot));
    memset(snap, 0, sizeof(snapshot));

    FILE *f = fopen(fname, "r");
    if (!f) {
        snap->error = -1;
        return -1;
    }

    /* Read at most 64k of bytes */
    char buffer[MAX_SNAPSHOT_SIZE];
    snap->length = fread(buffer, 1, MAX_SNAPSHOT_SIZE, f);

    /* Save the time of the snapshot */
    snap->time = unified_time();

    /* If we haven't reached the end of the file, either:
     *
     * 1) File is larger than 64k, in which case mark the snapshot as
     * truncated and continue.
     *
     * 2) An error has occured, in which case we save the error and
     * return.
     *
     * */
    if (!feof(f)) {
        if (snap->length == MAX_SNAPSHOT_SIZE) {
            snap->truncated = true;
        } else {
            snap->truncated = false;
            snap->error = ferror(f);
            fclose(f);
            return -1;
        }
    }

    /* Allocate space + 1 for the null termination */
    snap->data = (char *)malloc(snap->length+1);
    memcpy(snap->data, buffer, snap->length);
    snap->data[snap->length] = 0;
    fclose(f);

    return 0;
}

void
snapshot_delete(snapshot *snap) {
    /* Delete a snapshot by freeing the space used */
    if (snap->data)
        free(snap->data);
    free(snap);
}
