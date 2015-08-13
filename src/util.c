#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"


/**
 * Copy the column content into val.
 *
 * Assume that line is null terminated.
 *
 * If the number of columns in the line is less than col, return -1.
 */
int
column(char const *line, uint16_t col, char *val, uint16_t bufsize) {
    char const *ptr = line;
    uint16_t cur_col = -1;
    *val = 0;

    while (*ptr && (*ptr != '\n')) {
        /* if it is white space, ignore it */
        if (*ptr == ' ' || *ptr == '\t') {
            ptr++; continue;
        }

        /* else update the column that we are traversing right now */
        cur_col += 1;

        if (cur_col != col) {
            while (*ptr && 
                    (*ptr != '\n') && 
                    (*ptr != '\t') &&
                    (*ptr != ' '))
                    ptr++;
            continue;
        }

        /* if we reached the column save the value in it */
        assert(cur_col == col);
        {
            uint16_t len = 0;

            /* Save the column, while making sure we don't go over the
             * boundaris */
            while (*ptr && 
                    (*ptr != '\n') && 
                    (*ptr != '\t') &&
                    (*ptr != ' ') && 
                    (len < bufsize-1) ) {
                *val++ = *ptr++;
                len++;
            }

            /* Null terminate the string */
            *val = 0;
            return len;
        }
    }

    return -1;
}

/* Returns a pointer to the beginning of the next line
 *
 * if there is no other line, return 0
 * */
char const *next_line(char const *string) {
    char const *ptr = string;

    while (*ptr && (*ptr != '\n'))
        ptr++;

    if ((*ptr == '\n') && (*(ptr+1) != 0))
        return ptr+1;

    return 0;
}

/**
 *  Returns true if src ~= /^begin/
 */
bool startswith(char const *src, char const *begin) {
    while (true) {
        /* If begin == src all the way, return true */
        if (! (*begin)) return true;

        /* If we have reached the end of src but not begin, return false */
        if (! (*src)) return false;

        /* If any discrepency return false */
        if (*src != *begin) return false;

        src++;
        begin++;
    }
}


/** 
 * Returns true if two strings are equal 
 */
bool equal(char const *str1, char const *str2) {
    while (true) {
        if (*str1 != *str2) return false;

        /* If we have reached the end of one of the strings return 
         * true.  Note that because of the condition above this one,
         * str1 and str2 have been equal up until this point. */
        if (! (*str1)) return true;

        str1++;
        str2++;
    }
}
