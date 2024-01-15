#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
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
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
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
  int period;
  // uint ticks0;
  uint64 f_addr;
  if(argint(0, &period) < 0)
    return -1;
  if(argaddr(1, &f_addr) < 0)
    return -1;
  printf("sigalarm say hi %d %p\n", period, f_addr);
  myproc()->interval = period;
  myproc()->alarm_hander = f_addr;
  myproc()->ticks = 0;
  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  p->ticks = 0;
  p->trapframe->ra = p->pre_trapframe->ra;
  p->trapframe->sp = p->pre_trapframe->sp;
  p->trapframe->s0 = p->pre_trapframe->s0;
  p->trapframe->a0 = p->pre_trapframe->a0;
  p->trapframe->a1 = p->pre_trapframe->a1;
  p->trapframe->a5 = p->pre_trapframe->a5;
  p->trapframe->s1 = p->pre_trapframe->s1;
  p->trapframe->epc = p->pre_trapframe->epc;
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
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
