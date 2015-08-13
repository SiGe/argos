#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "history.h"

// static int
// _file(char const *path, char *file, uint16_t size) {
//     int len = strlen(path);
//     char const *end = path + len;
// 
//     /* Find the directory path */
//     while ((*end-- != '/') && (end != path));
// 
//     /* Pass '/' */
//     if (end == path) {
//         if (*end == '/') {
//             end += 1;
//         }
//     } else {
//         end += 2;
//     }
// 
//     /* Copy file */
//     while ((*file++ = *end++) && (size--));
// 
//     /* Null terminate file string */
//     *file = 0;
//     return 0;
// }


/* Saves the directory of the file path in the (char *dir) */
static int
_dir(char const *path, char *dir, uint16_t size) {
    int len = strlen(path);
    char const *end = path + len;

    /* Find the directory path */
    while ((*end-- != '/') && (end != path));

    /* Copy from path to dir */
    while ((path != (end+1)) && ((end - path) < (size-1)))
        *dir++ = *path++;

    /* Null terminate dir string */
    *dir = 0;
    return 0;
}

static int
_mkdir(char const *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++)
        if(*p == '/') {
            *p = 0;
            if (mkdir(tmp, S_IRWXU) < 0)
                if (errno != EEXIST)
                    return -1;
            *p = '/';
        }

    int ret = mkdir(tmp, S_IRWXU);

    if (ret == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}

int
history_create(history **hist, char const *path) {
    *hist = (history *)malloc(sizeof(history));

    /* Copy the path */
    (*hist)->path = (char *)malloc(strlen(path) + 1);
    strcpy((*hist)->path, path);
    (*hist)->transform = 0;
    

    /* Initiate current and head values */
    (*hist)->head = 0;
    (*hist)->cur = 0;
    return 0;
}

void
history_delete(history *hist) {
    history_node *node = hist->head;
    history_node *next = 0;

    while (node) {
        next = node->next;
        free(node);
        node = next;
    }

    free(hist->path);
    free(hist);
}

int
history_append(history *hist, uint64_t time, uint64_t value) {
    history_node *node = (history_node *)malloc(sizeof(history_node));
    
    node->value = value;
    node->time = time;
    node->next = 0; 
    node->out  = 0;

    if (!hist->cur) {
        /* If current or head are not defined, set them up */
        hist->cur = node;
        hist->head = node;
    } else {
        /* If a transformation function is defined used it */
        if (hist->transform) {
            node->out = hist->transform(hist->cur, node);
        }

        /* link the list, by appending the new node at the end */
        hist->cur->next = node;
        hist->cur = node;
    }

    return 0;
}

int
history_save(history *hist) {
    char dir[256];

    /* Save the directory */
    _dir(hist->path, dir, 256);

    /* Create the directory to save our data */
    if (*dir != 0 && _mkdir(dir) < 0) {
        fprintf(stderr, "Failed to create directory: %s\n", dir);
        return -1;
    }

    /* Write to file */
    FILE *f = fopen(hist->path, "w+");
    history_node *node = hist->head;

    /* Save the history */
    while (node) {
        fprintf(f, "%" PRIu64 ",%" PRIu64 ",%" PRId64 "\n",
                node->time, node->value, node->out);
        node = node->next;
    }
    fclose(f);

    return 0;
}
