
#include <assert.h>
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"
#include "kernel_sched.h"


/* 
 The process table and related system calls:
 - Exec
 - Exit
 - WaitPid
 - GetPid
 - GetPPid

 */

/* The process table */
PCB PT[MAX_PROC];

NTCB NTT[MAX_NTCB];

unsigned int process_count, nt_count;

PCB* get_pcb(Pid_t pid)
{
  return PT[pid].pstate==FREE ? NULL : &PT[pid];
}

Pid_t get_pid(PCB* pcb)
{
  return pcb==NULL ? NOPROC : pcb-PT;
}

/* Initialize a PCB */
static inline void initialize_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->argl = 0;
  pcb->args = NULL;

  for(int i=0;i<MAX_FILEID;i++)
    pcb->FIDT[i] = NULL;

  rlnode_init(& pcb->children_list, NULL);
  rlnode_init(& pcb->exited_list, NULL);
  rlnode_init(& pcb->children_node, pcb);
  rlnode_init(& pcb->exited_node, pcb);
  pcb->child_exit = COND_INIT;
  rlnode_init(& pcb->NT, NULL);
  pcb->ntcb_count=0;
  pcb->active_thread_count=0;
}

/* Initialize a NTCB */
static inline void initialize_NTCB(NTCB* ntcb)
{
  ntcb->argl = 0;
  ntcb->args = NULL;
  ntcb->parent = NULL;
  ntcb->ntcb_thread = NULL;     
  ntcb->join_var=COND_INIT;
  ntcb->flag_detach=0;
}

static PCB* pcb_freelist;
static NTCB* ntcb_freelist;

void initialize_processes()
{
  /* initialize the PCBs */
  for(Pid_t p=0; p<MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }

  /* Initialize the NTCBs */
  for(Pid_t i=0; i<MAX_NTCB; i++) {
    initialize_NTCB(&NTT[i]);
  }
  
  /* Use the parent field to build a free list */
  PCB* pcbiter;
  pcb_freelist = NULL;
  for(pcbiter = PT+MAX_PROC; pcbiter!=PT; ) {
    --pcbiter;
    pcbiter->parent = pcb_freelist;
    pcb_freelist = pcbiter;
  }

  process_count = 0;

  /* Use the ntcb_next field to build a free list */
  NTCB* ntcbiter;
  ntcb_freelist = NULL;
  for(ntcbiter = NTT+MAX_NTCB; ntcbiter!=NTT; ) {
    --ntcbiter;
    ntcbiter->ntcb_next = ntcb_freelist;
    ntcb_freelist = ntcbiter;
  }

  nt_count = 0;

  /* Execute a null "idle" process */
  if(Exec(NULL,0,NULL)!=0)
    FATAL("The scheduler process does not have pid==0");
}


/*
  Must be called with kernel_mutex held
*/
PCB* acquire_PCB()
{
  PCB* pcb = NULL;

  if(pcb_freelist != NULL) {
    pcb = pcb_freelist;
    pcb->pstate = ALIVE;
    pcb_freelist = pcb_freelist->parent;
    process_count++;
  }

  return pcb;
}

/*
  Must be called with kernel_mutex held
*/
void release_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->parent = pcb_freelist;
  pcb_freelist = pcb;
  process_count--;
}

/*
  Must be called with kernel_mutex held
*/
NTCB* acquire_NTCB() {

  NTCB* ntcb = NULL;
  if(ntcb_freelist != NULL) {
    ntcb = ntcb_freelist;
    ntcb_freelist = ntcb_freelist->ntcb_next;
    nt_count++;
  }

  return ntcb;
}

/*
  Must be called with kernel_mutex held
*/
void release_NTCB(NTCB* ntcb) {

  ntcb->ntcb_next = ntcb_freelist;
  ntcb_freelist = ntcb;
  nt_count--;
}

/*
 *
 * Process creation
 *
 */

/*
	This function is provided as an argument to spawn,
	to execute the main thread of a process.
*/
void start_main_thread()
{
  int exitval;

  Task call =  CURPROC->main_task;
  int argl = CURPROC->argl;
  void* args = CURPROC->args;

  exitval = call(argl,args);
  
  while (CURPROC->active_thread_count>0) {
    rlnode* node=rlist_pop_back(& CURPROC->NT);
    rlist_push_back(& CURPROC->NT,node);
    if(node->ntcb->ntcb_thread->state != EXITED)
      ThreadJoin((Tid_t)node->ntcb->ntcb_thread,&node->ntcb->exitval);
    release_NTCB(node->ntcb);
  }
  
  Exit(exitval);
}

void start_thread() {   

  int exitval;  
  Task call =  CURTHREAD->owner_ntcb->main_task;
  int argl = CURTHREAD->owner_ntcb->argl;
  void* args = CURTHREAD->owner_ntcb->args;

  exitval = call(argl,args);
  Mutex_Unlock(&kernel_mutex);
  ThreadExit(exitval);
}


/*
	System call to create a new process.
 */
Pid_t Exec(Task call, int argl, void* args)
{
  PCB *curproc, *newproc;
  
  Mutex_Lock(&kernel_mutex);

  /* The new process PCB */
  newproc = acquire_PCB();

  if(newproc == NULL) goto finish;  /* We have run out of PIDs! */

  if(get_pid(newproc)<=1) {
    /* Processes with pid<=1 (the scheduler and the init process) 
       are parentless and are treated specially. */
    newproc->parent = NULL;
  }
  else
  {
    /* Inherit parent */
    curproc = CURPROC;

    /* Add new process to the parent's child list */
    newproc->parent = curproc;
    rlist_push_front(& curproc->children_list, & newproc->children_node);

    if (!accept_flag) {
    /* Inherit file streams from parent */
    for(int i=0; i<MAX_FILEID; i++) {
       newproc->FIDT[i] = curproc->FIDT[i];
       if(newproc->FIDT[i]) 
          FCB_incref(newproc->FIDT[i]);
    }
  }
  }


  /* Set the main thread's function */
  newproc->main_task = call;

  /* Copy the arguments to new storage, owned by the new process */
  newproc->argl = argl;
  if(args!=NULL) {
    newproc->args = malloc(argl);
    memcpy(newproc->args, args, argl);
  }
  else
    newproc->args=NULL;
  
  /* 
    Create and wake up the thread for the main function. This must be the last thing
    we do, because once we wakeup the new thread it may run! so we need to have finished
    the initialization of the PCB.
   */
  if(call != NULL) {
   
    

    newproc->main_thread = spawn_thread(newproc, start_main_thread);
    wakeup(newproc->main_thread);
  }


finish:
  Mutex_Unlock(&kernel_mutex);
  return get_pid(newproc);
}


/* System call */
Pid_t GetPid()
{
  return get_pid(CURPROC);
}


Pid_t GetPPid()
{
  return get_pid(CURPROC->parent);
}


static void cleanup_zombie(PCB* pcb, int* status)
{
  if(status != NULL)
    *status = pcb->exitval;

  rlist_remove(& pcb->children_node);
  rlist_remove(& pcb->exited_node);

  release_PCB(pcb);
}


static Pid_t wait_for_specific_child(Pid_t cpid, int* status)
{
  Mutex_Lock(& kernel_mutex);

  /* Legality checks */
  if((cpid<0) || (cpid>=MAX_PROC)) {
    cpid = NOPROC;
    goto finish;
  }

  PCB* parent = CURPROC;
  PCB* child = get_pcb(cpid);
  if( child == NULL || child->parent != parent)
  {
    cpid = NOPROC;
    goto finish;
  }

  /* Ok, child is a legal child of mine. Wait for it to exit. */
  while(child->pstate == ALIVE)
    Cond_Wait(& kernel_mutex, & parent->child_exit,0);
  
  cleanup_zombie(child, status);
  
finish:
  Mutex_Unlock(& kernel_mutex);
  return cpid;
}


static Pid_t wait_for_any_child(int* status)
{
  Pid_t cpid;
  Mutex_Lock(&kernel_mutex);
  PCB* parent = CURPROC;
  
  /* Make sure I have children! */
  if(is_rlist_empty(& parent->children_list)) {
    cpid = NOPROC;
    goto finish;
  }
  
  while(is_rlist_empty(& parent->exited_list)) {
    Cond_Wait(& kernel_mutex, & parent->child_exit,0);
  }
  
  PCB* child = parent->exited_list.next->pcb;
  assert(child->pstate == ZOMBIE);
  cpid = get_pid(child);
  cleanup_zombie(child, status);
  
finish:
  Mutex_Unlock(& kernel_mutex);
  return cpid;
}


Pid_t WaitChild(Pid_t cpid, int* status)
{
  /* Wait for specific child. */
  if(cpid != NOPROC) {
    return wait_for_specific_child(cpid, status);
  }
  /* Wait for any child */
  else {
    return wait_for_any_child(status);
  }

}


void Exit(int exitval)
{
  /* Right here, we must check that we are not the boot task. If we are, 
     we must wait until all processes exit. */
  if(GetPid()==1) {
    while(WaitChild(NOPROC,NULL)!=NOPROC);
  }

  /* Now, we exit */
  Mutex_Lock(& kernel_mutex);

  PCB *curproc = CURPROC;  /* cache for efficiency */

  /* Do all the other cleanup we want here, close files etc. */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }

  /* Reparent any children of the exiting process to the 
     initial task */
  PCB* initpcb = get_pcb(1);
  while(!is_rlist_empty(& curproc->children_list)) {
    rlnode* child = rlist_pop_front(& curproc->children_list);
    child->pcb->parent = initpcb;
    rlist_push_front(& initpcb->children_list, child);
  }

  /* Add exited children to the initial task's exited list 
     and signal the initial task */
  if(!is_rlist_empty(& curproc->exited_list)) {
    rlist_append(& initpcb->exited_list, &curproc->exited_list);
    Cond_Broadcast(& initpcb->child_exit);
  }

  /* Put me into my parent's exited list */
  if(curproc->parent != NULL) {   /* Maybe this is init */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    Cond_Broadcast(& curproc->parent->child_exit);
  }

  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;
  curproc->exitval = exitval;

  /* Bye-bye cruel world */
  sleep_releasing(EXITED, & kernel_mutex,0);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

typedef struct OpenInfo_ctrl_block {

  FCB * reader ; 
  int PCB_counter ; 
  procinfo pcb_info ; 

} OICB ; 

int openInfo_close(void* ctrl_block) {

	OICB * open_ctrl = (OICB*) ctrl_block;
	open_ctrl->reader = NULL ;
	free (open_ctrl) ;

	return 0; 
}

int openInfo_read(void* ctrl_block, char *buf, unsigned int size) {

  OICB *OI_ctrl = (OICB*) ctrl_block;  

  // This will fill the procinfo struct with the PCB info

  while (PT[OI_ctrl->PCB_counter].pstate == FREE)
    OI_ctrl->PCB_counter++ ;  

    if (OI_ctrl->PCB_counter < MAX_PROC) {
	    OI_ctrl->pcb_info.pid = get_pid (&PT[OI_ctrl->PCB_counter]) ;
	    OI_ctrl->pcb_info.ppid = get_pid (PT[OI_ctrl->PCB_counter].parent) ;
    
    if (PT[OI_ctrl->PCB_counter].pstate == ALIVE) // Non zero
	    OI_ctrl->pcb_info.alive = 1 ;

    if (PT[OI_ctrl->PCB_counter].pstate == ZOMBIE) 
      	    OI_ctrl->pcb_info.alive = 0 ;

  	OI_ctrl->pcb_info.main_task = PT[OI_ctrl->PCB_counter].main_task ;
  	OI_ctrl->pcb_info.argl = PT[OI_ctrl->PCB_counter].argl ;

  	OI_ctrl->pcb_info.thread_count = (unsigned long) PT[OI_ctrl->PCB_counter].ntcb_count+1 ;  

  	if(PT[OI_ctrl->PCB_counter].args) // Alive
  	{
  	 	void * tmp = PT [OI_ctrl->PCB_counter].args ;
  		int i = 0 ; 
  		for (i = 0; i < PROCINFO_MAX_ARGS_SIZE ; i++)
  		{
  			OI_ctrl->pcb_info.args[i] = * (char *) tmp  ; 
  			tmp += sizeof(char) ; 
  		}
    	}  
	//else the args is null so there is no need to copy anything		

	// This will package the data from the struct to the buffer	

	memcpy (buf,((char*) &OI_ctrl->pcb_info) , size);

	OI_ctrl->PCB_counter++ ;
	
	return size ; 
} 
else 
{
	return 0 ;
}
   
}


file_ops openInfo_fops = {
  .Open = NULL,
  .Read = openInfo_read,
  .Write = NULL,
  .Close = openInfo_close
};


Fid_t OpenInfo() {

  Fid_t FID ; 
  FCB * OI_fcb ; 

  if (!FCB_reserve(1, &FID, &OI_fcb)) { // If the fids are exhausted   	
  	return NOFILE;
  }
  else
  {    
      OICB* new_OI_ctrl_block = (OICB*) xmalloc(sizeof(OICB));
      new_OI_ctrl_block->PCB_counter = 0 ; 
      new_OI_ctrl_block->reader = OI_fcb ;    
      OI_fcb->streamobj = new_OI_ctrl_block ;
      OI_fcb->streamfunc = &openInfo_fops ;

      return FID ;
  }  


}

