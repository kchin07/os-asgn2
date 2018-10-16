#include <stdio.h>
#include "lwp.h"
#include "scheduler.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>



//--------------------------------------------------------
static char active = FALSE;
static thread threadHead = NULL;
static thread realHead = NULL;
static tid_t threadId = 0;
static rfile originalSystemContext;
static struct scheduler sched = {NULL, NULL, rr_admit, rr_remove, rr_next};
scheduler Scheduler = &sched;
//--------------------------------------------------------

tid_t lwp_create(lwpfun func, void* arg, size_t stackSize){
   thread iter = threadHead;

   if(iter == NULL) {
      iter = safe_malloc(sizeof(context));
      iter->lib_one = NULL; 
      iter->lib_two = NULL;
      threadHead = iter;
      realHead = iter;
   }
   else {
      while(iter->lib_two != NULL) {
         iter = iter->lib_two;
      }
      iter->lib_two = safe_malloc(sizeof(context));
      iter->lib_two->lib_one = iter;
      iter->lib_two->lib_two = NULL;
      iter = iter->lib_two;
   }

   tid_t* newStack = safe_malloc(sizeof(tid_t) * stackSize * 8);
   tid_t* sp = newStack + (stackSize * 8);
   *(--sp) = (tid_t)lwp_exit;
   *(--sp) = (tid_t)func;
   *(--sp) = (tid_t)0xBADBEEEF;

   iter->tid = ++threadId;
   iter->stack = newStack;
   iter->stacksize = stackSize;
   iter->state.rdi = (tid_t)arg;
   iter->state.rsp = (tid_t)sp;
   iter->state.rbp = (tid_t)sp;
   iter->state.fxsave = FPU_INIT;
   Scheduler->admit(iter);
   return iter->tid;
}

void lwp_exit() {
   SetSP(originalSystemContext.rsp);
   loadNextThread();
}

void loadNextThread() {

   Scheduler->remove(threadHead);
   thread nextThread = Scheduler->next();

   free(threadHead->stack);
   free(threadHead);

   threadHead = nextThread;

   if(nextThread != NULL) {
      threadHead = nextThread;
      swap_rfiles(NULL, &(threadHead->state));
   }
   else {
      active = FALSE;
      threadHead = NULL;
      swap_rfiles(NULL, &(originalSystemContext));
   }
}

tid_t lwp_gettid(){
   if(threadHead != NULL) {
      return threadHead->tid;
   }

   return NO_THREAD;
}

void lwp_yield(){
   thread oldThread = threadHead;
   threadHead = Scheduler->next();

   if(!threadHead){
      swap_rfiles(&(oldThread->state), &(originalSystemContext));
   }
   swap_rfiles(&(oldThread->state), &(threadHead->state));

}

void lwp_start(){
   if(active) {
      return;
   }

   threadHead = Scheduler->next();
   if(threadHead){
      active = TRUE;
      swap_rfiles(&(originalSystemContext), &(threadHead->state));
   }
}

void lwp_stop(){
   if(!active) {
      return;
   }
   // stop the LWP system and restore original stack
   active = FALSE;
   swap_rfiles(&(threadHead->state), &(originalSystemContext));
}

void lwp_set_scheduler(scheduler fun){
   scheduler oldScheduler = Scheduler;
   
   if(fun == NULL) {
      //transfer to RR
      Scheduler = &sched;
   }
   else {
      //transfer to fun
      Scheduler = fun;
   }

   if(Scheduler->init){
      Scheduler->init();
   }    


   thread iter = oldScheduler->next();
   while(iter != NULL) {
      oldScheduler->remove(iter);      
      Scheduler->admit(iter);
      iter = oldScheduler->next();
   }

   if(oldScheduler->shutdown){
      oldScheduler->shutdown();
   }
}

scheduler lwp_get_scheduler(void) {
   return Scheduler;
}

thread tid2thread(tid_t tid){
   thread iter = realHead;

   while(iter) {
      if(iter->tid == tid) {
         return iter;
      }

      iter = iter->lib_two;
   }

   return NULL;
}

// from S.O.
void *safe_malloc(size_t n) {
    void *p = malloc(n);
    if (p == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %zu bytes.\n", n);
        abort();
    }
    return p;
}
