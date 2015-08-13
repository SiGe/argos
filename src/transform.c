#include "history.h"

#include "transform.h"

int64_t transform_delta(history_node const *n1, history_node const *n2) {
    return ((int64_t)n2->value - (int64_t)n1->value);
}

int64_t transform_identity(history_node const *n1, history_node const *n2) {
    return (int64_t)n2->value;
}
