#include <stdio.h>
#include "lwp.h"
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
static context originalSystemContext;
static scheduler ourScheduler;
//--------------------------------------------------------

void overflow_handler(int signum){
   abort();
}

void segv_handler(int signum, siginfo_t *info, void* other){
   abort();
}

void* create_stack(size_t sizeRequest, void** stackPointer){
   // allocate space for stack
   size_t sizeRequestInBytes = sizeRequest*sizeof(tid_t);
   size_t emptyMemSpace = 2*VIRTUAL_MEM_BLOCK;
   void* baseAddr = malloc(sizeRequestInBytes + emptyMemSpace);

   // create protected page/memory space
   void* protectedPage = baseAddr;
   unsigned int remainder = (uintptr_t)baseAddr % VIRTUAL_MEM_BLOCK;
   if(remainder){
      protectedPage= baseAddr + VIRTUAL_MEM_BLOCK - remainder;
   }
   
   // move stack pointer to the bottom of the stack
   *stackPointer = protectedPage + VIRTUAL_MEM_BLOCK + sizeRequestInBytes;

   // protect memory
   if( mprotect(baseAddr, VIRTUAL_MEM_BLOCK, PROT_NONE) == -1){
      protectedPage = NULL;
      sNode* tempNode;
      
      // initialize sig handlers
      if(!stackList){
         struct sigaction sa;
         stack_t signalStack;
         signalStack.ss_sp = extraStack;
         signalStack.ss_flags = 0;
         signalStack.ss_size = STACK_SIZE;
 
         if(sigaltstack(&signalStack, NULL)){
            perror("sigaltstack failure in creating a stack");
            exit(EXIT_FAILURE);
         }

         sa.sa_handler = overflow_handler;
         sigemptyset(&sa.sa_mask);
         sa.sa_flags = SA_ONSTACK;

         if(sigaction(SIGSTKFLT, &sa, NULL) == -1){
            perror("sigaction for SIGSTKFLT");
            exit(EXIT_FAILURE);
         }

         sa.sa_sigaction = segv_handler;
         sigemptyset(&sa.sa_mask);
         sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

         if(sigaction(SIGSEGV, &sa, NULL) == -1){
            perror("sigaction for SIGSEGV");
            exit(EXIT_FAILURE);
         } 
      }

   }
   return baseAddr;
}


static tid_t lwp_create(lwpfun func, void* arg, size_t sizeRequest){
   thread newThread;
   tid_t threadId = 0;
   tid_t* stackPointer

   // create stack ---------------------------------------------

   // create set of registers   

}

static void lwp_exit(){
   // terminate current process and select next process
}

static tid_t lwp_gettid(){
   // return tid_t of current lwp
   if(currentProcess){
      return currentProcess->tid;
   }
   return 0;
}

static void lwp_yield(){
   thread tempThread;
   tempThread = currentProcess;
   currentProcess = ourScheduler->nextProcess();
   if(!currentProcess){
      currentProcess = &originalSystemContext;
   }
   swap_rfiles(tempThread->state, currentProcess->state);
}

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
   if(!active){
      fprintf(stderr, "can't stop because not active");
   }
   else{
      active = FALSE;
      swap_rfiles(currentProcess-state, originalSystemContext->state);
   }
}

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

static void lwp_get_scheduler(scheduler fun){
   return ourScheduler;
}

static thread tid2thred(tid_t tid){
   thread tempThread;
   tid_t existingTid_t;

}
