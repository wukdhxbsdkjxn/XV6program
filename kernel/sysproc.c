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
  backtrace();
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

uint64
sys_sigreturn(void)//恢复被中断的程序的执行，并清除一些相关的状态信息
{
  struct proc *p = myproc();//通过调用myproc()函数获取当前进程的指针
  //将当前进程的since_interval成员变量设置为0。
  p->since_interval = 0;//这个变量用于跟踪自上一次定时器中断以来经过的时间。
  //将当前进程的trapframe_cp成员变量的值复制到trapframe成员变量指向的内存中。
  *(p->trapframe) = p->trapframe_cp;//trapframe_cp用于保存中断处理程序的上下文，trapframe用于恢复被中断的程序的执行。
  p->running_hand = 0;//确保在恢复被中断的程序的执行之前，没有其他中断处理程序正在运行
  return 0;
}

uint64
sys_sigalarm(void)//设置定时器并关联中断处理函数
{
  int interval;//用于存储定时器的时间间隔
  uint64 pointer;//用于存储中断处理函数的指针
  if(argint(0, &interval) < 0)
    return -1;
  if(interval == 0)//如果定时器的时间间隔为0，直接返回0。这表示停止定时器，不需要设置中断处理函数。
    return 0;
  if(argaddr(1, &pointer) < 0)
    return -1;
  myproc()->handler = (void *)pointer;//将当前进程的handler成员变量设置为pointer的值，即设置中断处理函数的指针。
  myproc()->since_interval = 0;//将当前进程的since_interval成员变量设置为0。这个变量用于跟踪自上一次定时器中断以来经过的时间。
  myproc()->running_hand = 0;//确保在设置定时器之前，没有其他中断处理程序正在运行
  myproc()->interval = interval;//设置定时器的时间间隔。
  return 0;
}