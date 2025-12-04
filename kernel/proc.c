#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];
struct proc proc[NPROC];
struct proc *initproc;
int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);
extern char trampoline[]; // trampoline.S

#define DEBUG_MLFQ 1

// MLFQ structures
static struct proc *rq_head[NUM_QUEUES];
static struct proc *rq_tail[NUM_QUEUES];
static const int mlfq_quanta[NUM_QUEUES] = {1,2,4,8};
#define BOOST_INTERVAL 100

#ifdef MLFQ_DEBUG
#define MDBG(...) cprintf(__VA_ARGS__)
#else
#define MDBG(...) do {} while(0)
#endif

static struct spinlock mlfq_lock;

// forward declarations
static void mlfq_enqueue(struct proc *p, int q);
static struct proc* mlfq_dequeue(int q);
void mlfq_boost(void);
static void mlfq_remove(struct proc *p);
void mlfq_make_runnable(struct proc *p);
void mlfq_init(void);
void mlfq_dump(void);

// MLFQ implementation
void
mlfq_init(void)
{
  int i;
  initlock(&mlfq_lock, "mlfq");
  for(i=0;i<NUM_QUEUES;i++){
    rq_head[i] = rq_tail[i] = 0;
  }
  printf("MLFQ_INIT: Initialized\n");
}

void 
mlfq_make_runnable(struct proc *p) 
{
  printf("MLFQ_MAKE: Called for pid %d, state=%d, cur_q=%d\n", 
         p->pid, p->state, p->cur_q);
  
  mlfq_remove(p);
  if(p->cur_q < 0 || p->cur_q >= NUM_QUEUES) p->cur_q = 0;
  
  printf("MLFQ_MAKE: Enqueueing pid %d to queue %d\n", p->pid, p->cur_q);
  mlfq_enqueue(p, p->cur_q);
}

// enqueue at tail
static void 
mlfq_enqueue(struct proc *p, int q) 
{
  if(q < 0 || q >= NUM_QUEUES) q = NUM_QUEUES - 1;

  printf("ENQUEUE: START - pid %d to queue %d\n", p->pid, q);
  
  acquire(&mlfq_lock);
  p->mlfq_next = 0;
  
  if(rq_tail[q]){
    printf("ENQUEUE: Adding to existing queue, tail was pid %d\n", rq_tail[q]->pid);
    rq_tail[q]->mlfq_next = p;
    rq_tail[q] = p;
  } else {
    printf("ENQUEUE: First process in queue %d\n", q);
    rq_head[q] = rq_tail[q] = p;
  }
  
  p->cur_q = q;
  release(&mlfq_lock);
  
  printf("ENQUEUE: DONE - pid %d in queue %d\n", p->pid, q);
}

static struct proc*
mlfq_dequeue(int q)
{
  struct proc *p;

  acquire(&mlfq_lock);
  p = rq_head[q];
  if(!p){
    release(&mlfq_lock);
    return 0;
  }
  rq_head[q] = p->mlfq_next;
  if(!rq_head[q]) rq_tail[q] = 0;
  p->mlfq_next = 0;
  MDBG("mlfq_dequeue pid %d from q%d\n", p->pid, q);
  release(&mlfq_lock);
  return p;
}

// remove p from any queue
static void
mlfq_remove(struct proc *p)
{
  int q;
  acquire(&mlfq_lock);
  for(q=0;q<NUM_QUEUES;q++){
    struct proc *prev = 0;
    struct proc *cur = rq_head[q];
    while(cur){
      if(cur == p){
        if(prev) prev->mlfq_next = cur->mlfq_next;
        else rq_head[q] = cur->mlfq_next;
        if(rq_tail[q] == cur) rq_tail[q] = prev;
        cur->mlfq_next = 0;
        MDBG("mlfq_remove pid %d from q%d\n", p->pid, q);
        release(&mlfq_lock);
        return;
      }
      prev = cur;
      cur = cur->mlfq_next;
    }
  }
  release(&mlfq_lock);
}

// boost: reset all to queue 0
void 
mlfq_boost(void) 
{
  printf("MLFQ BOOST: Starting priority boost\n");
  
  // First pass: Reset all RUNNABLE/RUNNING processes' queue fields
  for(struct proc *p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state == RUNNABLE || p->state == RUNNING){
      printf("BOOST: Resetting pid %d to queue 0\n", p->pid);
      p->cur_q = 0;
      p->qtick = 0;
    }
    release(&p->lock);
  }
  
  // Second pass: Clear and rebuild queues
  acquire(&mlfq_lock);
  for(int i=0; i<NUM_QUEUES; i++){
    rq_head[i] = rq_tail[i] = 0;
  }
  
  // Rebuild queue 0 with all RUNNABLE processes
  for(struct proc *p = proc; p < &proc[NPROC]; p++){
    if(p->state == RUNNABLE && p->cur_q == 0){
      p->mlfq_next = 0;
      if(rq_tail[0]){
        rq_tail[0]->mlfq_next = p;
        rq_tail[0] = p;
      } else {
        rq_head[0] = rq_tail[0] = p;
      }
    }
  }
  release(&mlfq_lock);
  
  printf("MLFQ BOOST: Completed\n");
}

void 
mlfq_dump(void) 
{
  acquire(&mlfq_lock);
  printf("\n=== MLFQ QUEUE DUMP ===\n");
  for(int q = 0; q < NUM_QUEUES; q++) {
    printf("Queue %d: ", q);
    struct proc *p = rq_head[q];
    int count = 0;
    while(p && count < 20) {
      printf("pid%d ", p->pid);
      p = p->mlfq_next;
      count++;
    }
    if(count == 0) printf("(empty)");
    printf("\n");
  }
  printf("======================\n\n");
  release(&mlfq_lock);
}

// helps ensure that wakeups of wait()ing parents are not lost
struct spinlock wait_lock;

void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int) (p - proc));
    p->mlfq_next = 0;
    p->cur_q = 0;
    p->qtick = 0;
    p->total_ticks = 0;
  }
  mlfq_init();
}

int
cpuid()
{
  int id = r_tp();
  return id;
}

struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  p->alarm_interval = 0;
  p->ticks_left = 0;
  p->alarm_ticks = 0; 
  p->alarm_handler = 0;
  p->handling_alarm = 0;
  p->inhandler = 0;
  p->cur_q = 0;
  p->qtick = 0;
  p->total_ticks = 0;
  p->mlfq_next = 0;
  
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// *** CRITICAL FIX: userinit() ***
void
userinit(void)
{
  struct proc *p;

  printf("USERINIT: Starting first process\n");
  
  p = allocproc();
  initproc = p;
  
  p->cwd = namei("/");

  printf("USERINIT: Setting pid %d to RUNNABLE\n", p->pid);
  p->state = RUNNABLE;
  
  printf("USERINIT: Calling mlfq_make_runnable for pid %d\n", p->pid);
  mlfq_make_runnable(p);
  
  printf("USERINIT: Releasing lock for pid %d\n", p->pid);
  release(&p->lock);
  
  printf("USERINIT: First process initialized\n");
}

int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  if((np = allocproc()) == 0){
    return -1;
  }

  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  *(np->trapframe) = *(p->trapframe);
  np->trapframe->a0 = 0;

  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  mlfq_make_runnable(np);
  release(&np->lock);

  return pid;
}

void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

void
kexit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);
  reparent(p);
  wakeup(p->parent);
  
  acquire(&p->lock);
  p->xstate = status;
  p->state = ZOMBIE;
  mlfq_remove(p);

  release(&wait_lock);
  sched();
  panic("zombie exit");
}

int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    sleep(p, &wait_lock);
  }
}
// Per-CPU process scheduler.
void
scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  int last_boost_ticks = ticks;

  printf("MLFQ scheduler starting on CPU %d\n", cpuid());
  
  for(;;){
    intr_on();

    // Check for boost
    if(ticks - last_boost_ticks >= BOOST_INTERVAL){
      printf("BOOST: Time for priority boost (ticks=%d)\n", ticks);
      mlfq_boost();
      last_boost_ticks = ticks;
    }

    // Search queues from highest to lowest priority
    struct proc *p = 0;
    int q;
    for(q = 0; q < NUM_QUEUES; q++){
      p = mlfq_dequeue(q);
      if(p) {
        printf("SCHED: Found pid %d in queue %d\n", p->pid, q);
        break;
      }
    }

    if(!p){
      // No runnable process, idle
      continue;
    }

    // Acquire process lock
    acquire(&p->lock);
    if(p->state != RUNNABLE){
      printf("SCHED: pid %d not runnable anymore (state=%d), skipping\n", p->pid, p->state);
      release(&p->lock);
      continue;
    }
    
    // Run the process
    p->state = RUNNING;
    c->proc = p;
    
    printf("SCHED: Running pid %d from queue %d\n", p->pid, p->cur_q);
    
    int start = ticks;
    swtch(&c->context, &p->context);
    
    // Process returned (yielded or preempted)
    int used = ticks - start;
    if(used <= 0) used = 1;
    
    printf("SCHED: pid %d ran for %d ticks\n", p->pid, used);
    
    // Update quantum counters
    p->total_ticks += used;
    p->qtick += used;
    
    printf("SCHED: pid %d qtick=%d, total_ticks=%d, cur_q=%d\n", 
           p->pid, p->qtick, p->total_ticks, p->cur_q);

    // ========== CRITICAL FIX ==========
    // Apply MLFQ demotion/promotion logic REGARDLESS of state
    int curq = p->cur_q;
    
    printf("MLFQ_CHECK: pid %d, cur_q=%d, qtick=%d, quantum=%d\n",
           p->pid, curq, p->qtick, mlfq_quanta[curq]);
    
    // Check for demotion (used full quantum)
    if(p->qtick >= mlfq_quanta[curq]){
        int newq = curq + 1;
        if(newq >= NUM_QUEUES) newq = NUM_QUEUES - 1;
        printf("DEMOTE: pid %d q%d->q%d (qtick=%d >= quantum=%d)\n",
               p->pid, curq, newq, p->qtick, mlfq_quanta[curq]);
        p->cur_q = newq;
        p->qtick = 0;
    } 
    // Check for promotion (used less than half quantum) 
    else if(p->qtick < (mlfq_quanta[curq] + 1) / 2 && p->cur_q > 0){
        printf("PROMOTE: pid %d q%d->q%d (qtick=%d < half of quantum=%d)\n",
               p->pid, curq, p->cur_q - 1, p->qtick, mlfq_quanta[curq]);
        p->cur_q = p->cur_q - 1;
        p->qtick = 0;
    }
    // Same level
    else {
        printf("SAME_QUEUE: pid %d stays in q%d (qtick=%d)\n",
               p->pid, curq, p->qtick);
        p->qtick = 0;
    }
    // ========== END FIX ==========

    // Now handle state - re-enqueue if still runnable
    if(p->state == RUNNABLE){
      printf("SCHED: Re-enqueuing pid %d to queue %d (state=RUNNABLE)\n", 
             p->pid, p->cur_q);
      mlfq_enqueue(p, p->cur_q);
    } else {
      printf("SCHED: pid %d not RUNNABLE (state=%d), removing from queues\n", 
             p->pid, p->state);
      mlfq_remove(p);
    }

    c->proc = 0;
    release(&p->lock);
    
    printf("---\n");  // Separator for readability
  }
}
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched RUNNING");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  mlfq_make_runnable(p);
  sched();
  release(&p->lock);
}

void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  release(&p->lock);

  if (first) {
    fsinit(ROOTDEV);
    first = 0;
    __sync_synchronize();

    p->trapframe->a0 = kexec("/init", (char *[]){ "/init", 0 });
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  mlfq_remove(p);

  p->chan = chan;
  p->state = SLEEPING;

  sched();

  p->chan = 0;

  release(&p->lock);
  acquire(lk);
}

void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
        mlfq_make_runnable(p);
      }
      release(&p->lock);
    }
  }
}

int
kkill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
        mlfq_make_runnable(p);
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
