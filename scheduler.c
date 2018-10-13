#include "scheduler.h"
#include "lwp.h"
#include <stdlib.h>
#include <stdio.h>

static thread head = NULL;

void rr_init() {
   return;
}

void rr_shutdown() {
   return;
}

void rr_admit(thread new) {
   if(head == NULL) {
      head = new;
      new->sched_one = NULL;
      new->sched_two = NULL;
   }
   else {
      thread iter = head;
      while(iter->sched_two != NULL) {
         iter = iter->sched_two;
      }

      iter->sched_two = new;
      new->sched_one = iter;
      new->sched_two = NULL;
   }
}

void rr_remove(thread victim) {
   if(head == victim) {
      head = head->sched_two;
      head->shed_one = NULL;
   }

   if(victim->sched_one) {
      victim->sched_one->sched_two = victim->sched_two;
   }

   if(victim->sched_two) {
      victim->sched_two->sched_one = victim->sched_one;
   }

   victim->sched_one = NULL;
   victim->sched_two = NULL;
}

thread rr_next() {
   if(head != NULL) {
      head = head->sched_two;
   }

   return head;
}
