


typedef struct thread_control_block
{
  PCB* owner_pcb;       /**< This is null for a free TCB */
  NTCB* owner_ntcb;

  ucontext_t context;     /**< The thread context */

#ifndef NVALGRIND
  unsigned valgrind_stack_id; /**< This is useful in order to register the thread stack to valgrind */
#endif

  Thread_type type;       /**< The type of thread */
  Thread_state state;    /**< The state of the thread */
  Thread_phase phase;    /**< The phase of the thread */

  void (*thread_func)();   /**< The function executed by this thread */

  Mutex state_spinlock;       /**< A spinlock for setting state and phase */


  /* scheduler data */  
  rlnode sched_node;      /**< node to use when queueing in the scheduler list */

  struct thread_control_block * prev;  /**< previous context */
  struct thread_control_block * next;  /**< next context */

  
} TCB;

typedef struct new_thread_control_block {

  PCB* parent;            /**< Parent's pcb. */
  NTCB* ntcb_next;
  TCB* ntcb_thread;       /**< The thread */
  Task main_task;         /**< The thread's function */
  int argl;               /**< The thread's argument length */
  void* args;             /**< The thread's argument string */
  int exitval;            /**< The exit value */
  CondVar join_var;     /**< Condition variable for @c ThreadJoin */
  //rlnode ntcb_child;
  //rlnode ntcb_parent;
  int flag_detach;

} NTCB;

typedef struct process_control_block {
  pid_state  pstate;      /**< The pid state for this PCB */

  PCB* parent;            /**< Parent's pcb. */
  int exitval;            /**< The exit value */

  TCB* main_thread;       /**< The main thread */
  Task main_task;         /**< The main thread's function */
  int argl;               /**< The main thread's argument length */
  void* args;             /**< The main thread's argument string */

  rlnode children_list;   /**< List of children */
  rlnode exited_list;     /**< List of exited children */

  rlnode children_node;   /**< Intrusive node for @c children_list */
  rlnode exited_node;     /**< Intrusive node for @c exited_list */
  CondVar child_exit;     /**< Condition variable for @c WaitChild */

  FCB* FIDT[MAX_FILEID];  /**< The fileid table of the process */

  rlnode NT;              /*List of new control block*/

} PCB;

typedef struct resource_list_node {

  union {
    PCB* pcb; 
    TCB* tcb;
    CCB* ccb;
    DCB* dcb;
    FCB* fcb;
    NTCB* ntcb;
    void* obj;
    rlnode_ptr node;
    intptr_t num;
    uintptr_t unum;
  };

  /* list pointers */
  rlnode_ptr prev;  /**< @brief Pointer to previous node */
  rlnode_ptr next;  /**< @brief Pointer to next node */
} rlnode;













