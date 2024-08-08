#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

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
sys_trace(void)//从用户空间获取了参数，然后把它设为进程的trace_mask
{
  int n;
  //获取追踪的mask
  if(argint(0, &n) < 0)//把第一个参数存进n 它代表追踪的mask，值可以是0、1、2
    return -1;
  //将mask保存在本进程的proc中
  struct proc *pro = myproc();
  printf("trace pid: %d\n", pro->pid);
  pro->trace_mask = n;
  return 0;
}

uint64
sys_info(void)
{
  uint64 addr;
  if(argaddr(0, &addr) < 0)//获取用户空间中第一个参数的地址
    return -1;
  struct sysinfo info;
  info.freemem = get_free_mem();//获取系统信息（空闲内存大小、进程数目和可用的文件描述符数目）
  info.nproc = get_proc_num();

  //copyout 参数：进程页表，用户态目标地址，数据源地址，数据大小 返回值：数据大小
  //将系统的状态信息返回给用户空间，以便用户可以方便地获取这些信息
  if(copyout(myproc()->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;

  return 0;
}
