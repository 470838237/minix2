#ifndef PROC_H
#define PROC_H

/* Here is the declaration of the process table.  It contains the process'
 * registers, memory map, accounting, and message send/receive information.
 * Many assembly code routines reference fields in it.  The offsets to these
 * fields are defined in the assembler include file sconst.h.  When changing
 * 'proc', be sure to change sconst.h to match.
 */

struct proc {
  struct stackframe_s p_reg;	/* process' registers saved in stack frame */

#if (CHIP == INTEL)
  reg_t p_ldt_sel;		/* selector in gdt giving ldt base and limit*/
  struct segdesc_s p_ldt[4];	/* local descriptors for code and data */
				/* 4 is LDT_SIZE - avoid include protect.h */
#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
  reg_t p_splow;		/* lowest observed stack value */
  int p_trap;			/* trap type (only low byte) */
  char *p_crp;			/* mmu table pointer (really struct _rpr *) */
  int p_nflips;			/* statistics */
#if defined(FPP)
  struct fsave p_fsave;		/* FPP state frame and registers */
  int align2;			/* make the struct size a multiple of 4 */
#endif
#endif /* (CHIP == M68000) */

  reg_t *p_stguard;		/* stack guard word */

  int p_nr;			/* number of this process (for fast access) */

  char p_int_blocked;		/* nonzero if int msg blocked by busy task */
  char p_int_held;		/* nonzero if int msg held by busy syscall */
  struct proc *p_nextheld;	/* next in chain of held-up int processes */

  int p_flags;			/* SENDING, RECEIVING, etc. */
  //fork时或p_pendcount为0时清楚标志位SIG_PENDING
  //p_flags为0时才可以运行
  struct mem_map p_map[NR_SEGS];/* memory map */
  pid_t p_pid;			/* process id passed in from MM */
  int p_priority;		/* task, server, or user process */

  clock_t user_time;		/* user time in ticks */
  clock_t sys_time;		/* sys time in ticks */
  clock_t child_utime;		/* cumulative user time of children */
  clock_t child_stime;		/* cumulative sys time of children */

  timer_t *p_exptimers;		/* list of expired timers */

  struct proc *p_callerq;	/* head of list of procs wishing to send */
  struct proc *p_sendlink;	/* link to next proc wishing to send */
  message *p_messbuf;		/* pointer to message buffer */
  int p_getfrom;		/* from whom does process want to receive? */
  int p_sendto;

  struct proc *p_nextready;	/* pointer to next ready process */
  sigset_t p_pending;		/* bit map for pending signals */
  unsigned p_pendcount;		/* count of pending and unfinished signals */
  //fork和exit操作时将p_pending和p_pendcount清0
  //p_pending和p_pendcount同增同减,p_pendcount对p_pending被设置的信号位计数
  //之所以需要单独增加p_pendcount计数而不是遍历p_pending。是为了减少计算量
  char p_name[16];		/* name of the process */
};

/* Guard word for task stacks. */
#define STACK_GUARD	((reg_t) (sizeof(reg_t) == 2 ? 0xBEEF : 0xDEADBEEF))

/* Bits for p_flags in proc[].  A process is runnable iff p_flags == 0. */
#define NO_MAP		0x01	/* keeps unmapped forked child from running */
#define SENDING		0x02	/* set when process blocked trying to send */
#define RECEIVING	0x04	/* set when process blocked trying to recv */
#define PENDING		0x08	/* set when inform() of signal pending */
//inform调用时清除PENDING，拥有待处理进程数-1，当内核cause_sig时标志位不存在PENDING时置位PENDING，拥有待处理进程数+1
//sig_procs：用于统计拥有待处理进程数，不必遍历进程列表来统计待处理进程数，用于判断是否需要触发inform调用
//PENDING作用用于限制调用mm次数。每次mm成功调用后mm进程保存了所有的信号。因此如果没有新的信号到来时不需要通知到mm
#define SIG_PENDING	0x10	/* keeps to-be-signalled proc from running */
//标识当前进程是否存在待处理信号
#define P_STOP		0x20	/* set when process is being traced */
//设置该标志位用于组织进程被调度
/* Values for p_priority */
#define PPRI_NONE	0	/* Slot is not in use */
#define PPRI_TASK	1	/* Part of the kernel */
#define PPRI_SERVER	2	/* System process outside the kernel */
#define PPRI_USER	3	/* User process */
#define PPRI_IDLE	4	/* Idle process */

/* Magic process table addresses. */
#define BEG_PROC_ADDR (&proc[0])
#define END_PROC_ADDR (&proc[NR_TASKS + NR_PROCS])
#define END_TASK_ADDR (&proc[NR_TASKS])
#define BEG_SERV_ADDR (&proc[NR_TASKS])
#define BEG_USER_ADDR (&proc[NR_TASKS + LOW_USER])

#define NIL_PROC          ((struct proc *) 0)
#define isidlehardware(n) ((n) == IDLE || (n) == HARDWARE)
#define isokprocn(n)      ((unsigned) ((n) + NR_TASKS) < NR_PROCS + NR_TASKS)
#define isoksrc_dest(n)   (isokprocn(n) || (n) == ANY)
#define isrxhardware(n)   ((n) == ANY || (n) == HARDWARE)
#define issysentn(n)      ((n) == FS_PROC_NR || (n) == MM_PROC_NR)
#define isemptyp(p)       ((p)->p_priority == PPRI_NONE)
#define istaskp(p)        ((p)->p_priority == PPRI_TASK)
#define isservp(p)        ((p)->p_priority == PPRI_SERVER)
#define isuserp(p)        ((p)->p_priority == PPRI_USER)
#define proc_addr(n)      (pproc_addr + NR_TASKS)[(n)]
#define cproc_addr(n)     (&(proc + NR_TASKS)[(n)])
#define proc_number(p)    ((p)->p_nr)
#define proc_vir2phys(p, vir) \
			  (((phys_bytes)(p)->p_map[D].mem_phys << CLICK_SHIFT) \
							+ (vir_bytes) (vir))

EXTERN struct proc proc[NR_TASKS + NR_PROCS];	/* process table */
EXTERN struct proc *pproc_addr[NR_TASKS + NR_PROCS];
/* ptrs to process table slots; fast because now a process entry can be found
   by indexing the pproc_addr array, while accessing an element i requires
   a multiplication with sizeof(struct proc) to determine the address */
EXTERN struct proc *bill_ptr;	/* ptr to process to bill for clock ticks */
EXTERN struct proc *rdy_head[NQ];	/* pointers to ready list headers */
EXTERN struct proc *rdy_tail[NQ];	/* pointers to ready list tails */

#endif /* PROC_H */
