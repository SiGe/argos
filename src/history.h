#ifndef _HISTORY_H_
#define _HISTORY_H_

#include <stdint.h>

typedef struct
history_node {
    uint64_t time; /* Time of this snapshot */
    uint64_t value;  /* Value at this snapshot in time */
    int64_t out;  /* Value that will be saved into the file, after the
                      transformations */

    struct history_node *next;
} history_node;

typedef struct
history {
    char *path; /* Path of this history */
    history_node *head;
    history_node *cur;

    int64_t (*transform)(history_node const *, history_node const *);
} history;


/* Save the history into history->path as a csv file */
int history_save(history *);

/* Release the resources used by this history */
void history_delete(history *);

/* Create a new history */
int history_create(history **, char const *);

/* Append a history node to a history */
int history_append(history*, uint64_t, uint64_t);

#endif /* _HISTORY_H_ */
