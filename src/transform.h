#ifndef _TRANSFORM_H_
#define _TRANSFORM_H_

#include "history.h"

/* Calculate delta transformation between two consecutive nodes */
int64_t transform_delta(history_node const *, history_node const *);
int64_t transform_identity(history_node const *, history_node const *);

#endif /* _TRANSFORM_H_ */
