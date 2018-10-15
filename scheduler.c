#include "scheduler.h"
#include <lwp.h>
#include <stdlib.h>
#include <stdio.h>

static thread head = NULL;
static char runNext = FALSE;

void rr_init() {
   return;
}

void rr_shutdown() {
   return;
}

void rr_admit(thread new) {
   if(head != NULL){
      new->sched_two = head;
      new->sched_one = head->sched_one;
      new->sched_one->sched_two = new;
      head->sched_one = new;
   }
   else{
      runNext = FALSE;
      head = new;
      head->sched_one = new;
      head->sched_two = new;
   }
}

void rr_remove(thread victim) {
   victim->sched_one->sched_two = victim->sched_two;
   victim->sched_two->sched_one = victim->sched_one;

   if(victim == head){
      if(victim->sched_two == victim){
         head = NULL;
      }
      else{
         head = victim->sched_one;
      }
   }
}

thread rr_next() {
   if(head != NULL) {
      if(runNext){
         head = head->sched_two;
      }
      else{
         runNext = TRUE;
      }
   }

   return head;
}
