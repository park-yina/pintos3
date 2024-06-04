#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "filesys/file.h"  /* P2_3 System Call 추가 */
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* ------------------ project2 -------------------- */
#define FDT_PAGES 3     /* pages to allocate for file descriptor tables (thread_create, process_exit) */
#define FDCOUNT_LIMIT FDT_PAGES *(1 << 9)       /* limit fd_idx */
/* ------------------------------------------------ */

struct thread {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    int priority;                       /* Priority. */

    /* 쓰레드 디스크립터 필드 추가 */
    int64_t wakeup_tick;    /* 깨어나야 할 tick을 저장할 변수 추가 */
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /* ----- PROJECT 1 --------- */
    int64_t wake_up_tick; /* thread's wakeup_time */
    int initial_priority; /* thread's initial priority */
    struct lock *wait_on_lock; /* which lock thread is waiting for  */
    struct list donation_list; /* list of threads that donate priority to **this thread** */
    struct list_elem donation_elem; /* prev and next pointer of donation_list where **this thread donate** */
    /* ------------------------- */

    int exit_status;         /* to give child exit_status to parent */
    int fd_idx;                     /* for open file's fd in fd_table */
    struct intr_frame parent_if;    /* Information of parent's frame */
    struct list child_list; /* list of threads that are made by this thread */
    struct list_elem child_elem; /* elem for this thread's parent's child_list */
    struct semaphore fork_sema; /* parent thread should wait while child thread copy parent */
    struct semaphore wait_sema;
    struct semaphore free_sema;
    struct file **fd_table;   /* allocated in thread_create */  
    struct file *running;
    
#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
    /* Table for whole virtual memory owned by thread. */
    struct supplemental_page_table spt;
    void *stack_bottom;
#endif

    // Project 3-2 stack growth
    uint64_t rsp; // a page fault occurs in the kernel
    
    /* Owned by thread.c. */
    struct intr_frame tf;               /* Information for switching */
    unsigned magic;                     /* Detects stack overflow. */
    /* 이 값은 thread.c에 정의된 임의의 숫자이며, 스택 오버플로를 감지하는데 사용된다. 
    thread_current()는 실행 중인 스레드 구조체의 magic 멤버가 THREAD_MAGIC으로 설정 되었는지 확인한다. 
    스택 오버플로로 인해 이 값이 변경되어 ASSERT가 발생하는 경우가 있다. */

    /* Donate 추가 */
    int init_priority; /* 스레드가 priority 를 양도받았다가 다시 반납할 때 원래의 priority 를 복원할 수 있도록 고유의 priority 값을 저장하는 변수 */
    struct lock *wait_on_lock;        /* 스레드가 현재 얻기 위해 기다리고 있는 lock 으로 스레드는 이 lock 이 release 되기를 기다린다 */
    struct list donations;            /* 자신에게 priority 를 나누어준 스레드들의 리스트 */
    struct list_elem donation_elem;    /* 이 리스트를 관리하기 위한 element 로 thread 구조체의 그냥 elem 과 구분하여 사용한다 */
    /* P2_3 System Call 추가 */
    struct file **fd_table;
    int fd_idx;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

/* ------------- project 1 ------------ */
void thread_sleep(int64_t ticks);
void thread_awake(int64_t ticks);
int64_t get_next_tick_to_awake(void);
bool thread_priority_compare (struct list_elem *element1, struct list_elem *element2, void *aux);
bool preempt_by_priority(void);
bool thread_donate_priority_compare (struct list_elem *element1, struct list_elem *element2, void *aux);
/* ------------------------------------- */
/* ------------------- project 2 -------------------- */
struct thread* get_child_by_tid(tid_t tid);
/* -------------------------------------------------- */
#endif /* threads/thread.h */
ㄴ