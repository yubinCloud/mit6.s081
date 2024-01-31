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
  struct proc *p = myproc();
  addr = p->sz;

  // 检查 user process 的内存是否超过了 PLIC
  if (addr + n >= PLIC) {
    return -1;
  }

  if(growproc(n) < 0)
    return -1;

  if (n > 0) {
    // 将 sbrk 扩张的内存的 mapping 复制到 kernel page table
    pte_t *pte, *kpte;
    for (int j = addr; j < addr + n; j += PGSIZE) {
      pte = walk(p->pagetable, j, 0);
      kpte = walk(p->kpt, j, 1);
      *kpte = (*pte) & (~PTE_U);
    }
  } else if (n < 0) {
    for (int j = addr - PGSIZE; j >= addr + n; j -= PGSIZE) {
      uvmunmap(p->kpt, j, 1, 0);
    }
  }
  
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
