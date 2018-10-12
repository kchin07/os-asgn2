#ifndef SCHED_H
#define SCHED_H

#include "lwp.h"
#include <sys/types.h>

void rr_init();
void rr_shutdown();
void rr_admit(thread new);
void rr_remove(thread victim);
thread next();

#endif
