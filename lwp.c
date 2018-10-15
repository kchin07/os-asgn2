#include <stdio.h>
#include "lwp.h"
#include "scheduler.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>

#define VIRTUAL_MEM_BLOCK (1<<12)
#define STACK_SIZE (1<<16)

typedef struct stackNode {
   void* protectedStackBase;
   void* protectedPageBase;
   struct stackNode** next;
} sNode;

//--------------------------------------------------------
static char active = FALSE;
static thread threadHead = NULL;
static tid_t threadId;
static rfile originalSystemContext;
static struct scheduler sched = {NULL, NULL, rr_admit, rr_remove, rr_next};
scheduler Scheduler = &sched;
//--------------------------------------------------------

tid_t lwp_create(lwpfun func, void* arg, size_t stackSize){
   thread iter = threadHead;

   if(iter == NULL) {
/*    
      threadHead = safe_malloc(sizeof(context));
      threadHead->lib_one = NULL;
      threadHead->lib_two = NULL;
*/
      iter = safe_malloc(sizeof(context));
      iter->lib_one = NULL; 
      iter->lib_two = NULL;
      threadHead = iter;

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

/*
   stackPointer = safe_malloc(sizeof(tid_t) * stackSize);
   stackPointer += stackSize;

   *(--stackPointer) = (tid_t)lwp_exit;
   *(--stackPointer) = (tid_t)func;

   basePointer = --stackPointer;

   iter->state.rsp = (tid_t)stackPointer;
   iter->state.rbp = (tid_t)basePointer;
   iter->tid = ++threadId;
   iter->stacksize = stackSize;
   // TODO check if multiple args?
   // rdi, rsi, rdx, rcx, r8, r9 for arg
   iter->state.rdi = *((tid_t *)arg);
   iter->state.fxsave = FPU_INIT;
*/

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

   Scheduler->remove(threadHead);
   thread nextThread = Scheduler->next();
   loadNextThread(nextThread);
}

void loadNextThread(thread nextThread) {

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
   thread oldThreadHead = threadHead;
   thread nextThreadHead = Scheduler->next();


   if(nextThreadHead != NULL && nextThreadHead != oldThreadHead) {
      threadHead = nextThreadHead;
      swap_rfiles(&(oldThreadHead->state), &(threadHead->state));
   }
}

void lwp_start(){
   if(active) {
      return;
   }
/*
   // save
   swap_rfiles(&(originalSystemContext.state), NULL);
   threadHead = Scheduler->next();
   if(threadHead != NULL) {
      active = TRUE;
      swap_rfiles(NULL, &(threadHead->state));
   }
*/

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
   if(threadHead != NULL) {
      swap_rfiles(&(threadHead->state), NULL);
   }

   swap_rfiles(&(threadHead->state), &(originalSystemContext));

   active = FALSE;
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

   Scheduler->init();

   thread iter = oldScheduler->next();
   while(iter != NULL) {
      Scheduler->admit(iter);
      oldScheduler->remove(iter);
      iter = oldScheduler->next();
   }

   oldScheduler->shutdown();
}

scheduler lwp_get_scheduler(void) {
   return Scheduler;
}

thread tid2thread(tid_t tid){
   thread iter = threadHead;

   while(iter->lib_two != NULL) {
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
