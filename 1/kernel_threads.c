
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

Mutex exitmutex = MUTEX_INIT; 
/** 
  @brief Create a new thread in the current process.
  */
Tid_t CreateThread(Task task, int argl, void* args) {

  Mutex_Lock(&kernel_mutex);

  PCB* current_proc=CURPROC;
    
  NTCB* curntcb;
  curntcb=(NTCB*)acquire_NTCB();
  curntcb->parent=current_proc;
  curntcb->main_task=task; 
  curntcb->argl=argl;
  curntcb->args=args;
  rlnode* node=(rlnode*)xmalloc(sizeof(rlnode));
  node=rlnode_init(node, curntcb);
  (&current_proc->NT)->ntcb=(NTCB*)acquire_NTCB();
  current_proc->NT=*node;
  rlist_push_front(&current_proc->NT,node);
  current_proc->ntcb_count++;
  current_proc->active_thread_count++; 

  (&current_proc->NT)->ntcb->ntcb_thread=spawn_thread(current_proc, start_thread);
  wakeup((&current_proc->NT)->ntcb->ntcb_thread);
  
  Mutex_Unlock(&kernel_mutex);

  Tid_t tid=(Tid_t)(&current_proc->NT)->ntcb->ntcb_thread;
  release_NTCB(curntcb);

  return tid;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t ThreadSelf() {
	return (Tid_t) CURTHREAD;
}

/**
  @brief Join the given thread.
  */
int ThreadJoin(Tid_t tid, int* exitval) {
  
  Mutex_Lock(& kernel_mutex);
  
  rlnode* NT = &CURPROC->NT;
  TCB *tcb_tmp = (TCB*)tid;
  NTCB* owner;
  owner=(NTCB*)acquire_NTCB();
  owner=tcb_tmp->owner_ntcb;
  
  rlnode* node=(rlnode*)xmalloc(sizeof(rlnode));
  rlnode* fail=(rlnode*)xmalloc(sizeof(rlnode));
  node=rlnode_init(node, owner);
  fail=rlnode_init(fail, owner);
  if (CURPROC->ntcb_count==0)
    goto unsuccessful;
  node=rlist_find(NT, owner, fail);

  /* Legality checks */
  if (CURTHREAD==tcb_tmp || owner->flag_detach==1 || node==fail ) { 
    goto unsuccessful;
  }
  
  /* Wait for it to exit. */
  Cond_Wait(&kernel_mutex, &owner->join_var,0); 

  if (owner->flag_detach!=1) {
    *exitval=owner->exitval;  
  }
  else {
    goto unsuccessful;
  }
  
  /*success*/
  Mutex_Unlock(& kernel_mutex);
  release_NTCB(owner);
  return 0;

  unsuccessful:
    Mutex_Unlock(& kernel_mutex);
    release_NTCB(owner);
    return -1;
}

/**
  @brief Detach the given thread.
  */
int ThreadDetach(Tid_t tid) { 

  Mutex_Lock(& kernel_mutex);

  TCB *tcb_tmp = (TCB*)tid;
  NTCB* owner;
  owner=(NTCB*)acquire_NTCB();
  owner=tcb_tmp->owner_ntcb;

  rlnode* NT = &CURPROC->NT;
  rlnode* node=(rlnode*)xmalloc(sizeof(rlnode));
  rlnode* fail=(rlnode*)xmalloc(sizeof(rlnode));
  node=rlnode_init(node, NULL);
  fail=rlnode_init(fail, NULL);
  node=rlist_find(NT, tcb_tmp, fail);

	if (tcb_tmp->state==EXITED || node==fail) {
    goto unsuccessful;
  }

  owner->flag_detach=1;
  Cond_Broadcast(&owner->join_var);

  Mutex_Unlock(& kernel_mutex);
  release_NTCB(owner);
  return 0;

  unsuccessful:
    Mutex_Unlock(& kernel_mutex);
    release_NTCB(owner);
    return -1;
}

/**
  @brief Terminate the current thread.
  */
void ThreadExit(int exitval) { 
  
  Mutex_Lock(& kernel_mutex); 
  TCB* thread=CURTHREAD;
  CondVar* cv=&thread->owner_ntcb->join_var;
  CURPROC->active_thread_count--;
  Cond_Broadcast(cv);
  
  sleep_releasing(EXITED, & kernel_mutex,0);
}


/**
  @brief Awaken the thread, if it is sleeping.

  This call will set the interrupt flag of the
  thread.

  */
int ThreadInterrupt(Tid_t tid)
{
	return -1;
}


/**
  @brief Return the interrupt flag of the 
  current thread.
  */
int ThreadIsInterrupted()
{
	return 0;
}

/**
  @brief Clear the interrupt flag of the
  current thread.
  */
void ThreadClearInterrupt()
{

}


