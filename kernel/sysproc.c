
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
void mlfq_boost(void);

struct uprocinfo {
  int pid;
  int state;
  int cur_q;
  int qtick;
  int total_ticks;
  char name[16];
};
extern struct proc proc[NPROC];
// sys_getprocinfo: copy info of all processes to user-space
uint64
sys_getprocinfo(void)
{
    struct uprocinfo info[NPROC];  // Array for ALL processes
    struct proc *p;
    uint64 addr;
    int i = 0;
    
    argaddr(0, &addr);
    
    // Fill array with ALL processes
    for(p = proc; p < &proc[NPROC]; p++){
        acquire(&p->lock);
        if(p->state != UNUSED){
            info[i].pid = p->pid;
            info[i].state = p->state;
            info[i].cur_q = p->cur_q;
            info[i].qtick = p->qtick;
            info[i].total_ticks = p->total_ticks;
            safestrcpy(info[i].name, p->name, sizeof(info[i].name));
            i++;
        }
        release(&p->lock);
    }
    
    // Copy array to user
    if(copyout(myproc()->pagetable, addr, (char*)&info, sizeof(struct uprocinfo) * i) < 0) {
        return -1;
    }
    
    return i;  // Return count
}
// sys_boostproc: boost all processes to the top queue
uint64
sys_boostproc(void)
{
    struct proc *p;
    
    // Boost all runnable processes to queue 0
    for(p = proc; p < &proc[NPROC]; p++){
        acquire(&p->lock);
        if(p->state == RUNNABLE || p->state == RUNNING){
            p->cur_q = 0;
            p->qtick = 0;
        }
        release(&p->lock);
    }
    
    return 0;
}
uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  
  acquire(&tickslock);
  ticks0 = ticks;
  
  while(ticks - ticks0 < n) {
    if(myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  
  release(&tickslock);
  return 0;
}

uint64
sys_sigalarm(void)
{
  int ticks;
  uint64 handler;
  struct proc *p = myproc();

  // Now use argint/argaddr
  argint(0, &ticks);
  argaddr(1, &handler);

  acquire(&p->lock);
  if (ticks > 0) {
    p->alarm_interval = ticks;
    p->alarm_ticks = ticks;
    p->alarm_handler = handler;
    p->inhandler = 0;
  } else {
    p->alarm_interval = 0;
    p->alarm_ticks = 0;
    p->alarm_handler = 0;
    p->inhandler = 0;
  }
  release(&p->lock);

  return 0;
}
uint64
sys_sigreturn(void)
{
    struct proc *p = myproc();

    acquire(&p->lock);
    memmove(p->trapframe, &p->alarm_tf, sizeof(struct trapframe));
    p->inhandler = 0;  // allow next alarm
    release(&p->lock);

    return p->trapframe->a0;
}


uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if( n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;
  backtrace(); 
  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
