#include <stdio.h>
#include "lwp.h"
#include <scheduler.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>

#define VIRTUAL_MEM_BLOCK (1<<12)
#define STACK_SIZE (1<<16)
#define FALSE 0
#define TRUE 1

typedef struct stackNode {
   void* protectedStackBase;
   void* protectedPageBase;
   struct stackNode** next;
} sNode;

//--------------------------------------------------------
static sNode* stackList=NULL;
static char extraStack[STACK_SIZE];
static char active = FALSE;
static thread currentProcess = NULL;
static thread threadHead = NULL;
static tid_t threadId;
static context originalSystemContext;
static struct scheduler rr_scheduler = {NULL, NULL, rr_admit, rr_remove, rr_next};
scheduler RoundRobin = &rr_scheduler;
//--------------------------------------------------------

void overflow_handler(int signum){
   abort();
}

void segv_handler(int signum, siginfo_t *info, void* other){
   abort();
}

static tid_t lwp_create(lwpfun func, void* arg, size_t stackSize){
   tid_t* stackPointer;
   tid_t* basePointer;

   thread iter = threadHead;

   if(iter == NULL) {
      threadHead = safe_malloc(sizeof(context));
      threadHead->lib_one = NULL;
      threadHead->lib_two = NULL;
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

   stackPointer = safe_malloc(sizeof(tid_t) * stackSize);
   stackPointer += stackSize;

   *(--stackPointer) = (tid_t)lwp_exit;
   *(--stackPointer) = (tid_t)func;

   basePointer = --stackPointer;
   iter->state.rsp = (tid_t)stackPointer;
   iter->state.rbp = (tid_t)basePointer;
   iter->tid = ++threadId;
   iter->stacksize = stackSize;
   // TODO
   // rdi, rsi, rdx, rcx, r8, r9 for arg
   iter->state.fxsave = FPU_INIT;

   RoundRobin->admit(iter);

   return iter->tid;
}

static void lwp_exit() {
   SetSP(originalSystemContext.state.rsp);

   thread nextThread = RoundRobin->next;
   thread oldThread = nextThread->lib_one;
   RoundRobin->remove(oldThread);
   removeFromLib(oldThread);

   if(nextThread != NULL) {
      threadHead = nextThread;
      swap_rfiles(NULL, &(threadHead->state));
   }
   else {
      threadHead = NULL;
      swap_rfiles(NULL, &(originalSystemContext.state));
   }
}

static void removeFromLib(thread victim) {
   if(victim == threadHead) {
      if(threadHead->lib_two != NULL) {
         threadHead->lib_two->lib_one = NULL;
         threadHead = threadHead->lib_two;
      }
      else {
         threadHead = NULL;
      }
   }
   else {
      if(victim->lib_one != NULL) {
         victim->lib_one->lib_two = victim->lib_two;
      }
      if(victim->lib_two != NULL) {
         victim->lib_two->lib_one = victim->lib_one;
      }
   }

   free(victim);
}

static tid_t lwp_gettid(){
   if(threadHead != NULL) {
      return threadHead->tid;
   }

   return NO_THREAD;
}

static void lwp_yield(){
   thread oldThreadHead = threadHead;
   thread nextThreadHead = RoundRobin->next();


   if(nextThreadHead != NULL && nextThreadHead != oldThreadHead) {
      threadHead = nextThreadHead;
      swap_rfiles(&(oldThreadHead->state), &(threadHead->state));
   }
}

// TODO
static void lwp_start(){
   // start LWP system
   // save original stack and move to one of the lightweight stacks
   if(active){
      fprintf(stderr, "can't start because already active");
   }
   else{
      currentProcess = ourScheduler->next();
      if(currentProcess){
         active = TRUE;
         swap_rfiles(originalSystemContext->state, currentProcess->state);
      }
   }
}

static void lwp_stop(){
   // stop the LWP system and restore original stack
   if(threadHead != NULL) {
      swap_rfiles(&(threadHead->state), NULL);
   }

   swap_rfiles(&(threadHead->state), &(originalSystemContext.state));
}

// TODO
static void lwp_set_scheduler(scheduler fun){
   scheduler prevSched = ourScheduler;
   thread tempThread;
   
   if(fun){
      ourScheduler = fun;
   }
   if(fun == ourScheduler){
      return;
   }
   else{
      ourScheduler = roundRobinScheduler; // temp placeholder for our RRSched
   }

   if(ourScheduler->init){
      ourScheduler->init();
   }

   tempThread = prevSched->next()
   while(tempThread){
      ourScheduler->admit(tempThread);
      prevSched->remove(tempThread);
      tempThread = prevSched->next();
   }
   
   if(prevSched->shutdown){
      prevSched->shutdown();
   }
}

static void lwp_get_scheduler(void) {
   return RoundRobin;
}

static thread tid2thred(tid_t tid){
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
void *safe_malloc(size_t n)
{
    void *p = malloc(n);
    if (p == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %zu bytes.\n", n);
        abort();
    }
    return p;
}
