// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// struct {
//   struct spinlock lock;
//   struct run *freelist;
// } kmem;
struct {
  struct spinlock lock;//锁
  struct run *freelist;// 空闲资源链表
  char lock_name[7];// 锁的名称
} kmem[NCPU];//定义NCPU个kmem结构体,用于管理不同CPU的内存分配

void
kinit()//初始化内存
{
  for (int i = 0; i < NCPU; i++) {//给每个CPU设置锁
    snprintf(kmem[i].lock_name, sizeof(kmem[i].lock_name), "kmem_%d", i);// 格式化锁的名称
    initlock(&kmem[i].lock, kmem[i].lock_name);// 初始化锁
  }
  freerange(end, (void*)PHYSTOP);// 初始化空闲资源
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;//用来指向要释放的内存块
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)// 检查释放的内存块是否合法
    panic("kfree");
  // 将内存块填充为垃圾值，以捕获悬空引用
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;
  push_off();// 关闭中断，禁止CPU切换
  int id = cpuid();// 获取当前CPU的ID

  acquire(&kmem[id].lock);//获取当前CPU对应的锁，确保对空闲资源链表的互斥访问
  r->next = kmem[id].freelist;//将释放的内存块插入到当前CPU的空闲资源链表头部
  kmem[id].freelist = r;//更新当前CPU的空闲资源链表头指针
  release(&kmem[id].lock);//释放当前CPU对应的锁

  pop_off();//打开中断，允许CPU切换
}

//分配一个4096字节的物理内存页。
//返回一个内核可以使用的指针。
//如果无法分配内存，则返回0。
void *
kalloc(void)
{
  struct run *r;

  push_off();// 关闭中断，禁止CPU切换
  int id = cpuid();

  acquire(&kmem[id].lock);// 获取当前CPU对应的锁，确保互斥操作
  r = kmem[id].freelist;// 将当前CPU空闲资源链表的头节点赋值给r
  if(r) {// 如果空闲资源链表非空，将头节点指向下一个节点
    kmem[id].freelist = r->next;
  }
  else {// 分配失败，尝试从其他CPU上窃取
    int success = 0;
    int i = 0;
    for(i = 0; i < NCPU; i++) {
      if (i == id) continue;
      acquire(&kmem[i].lock);// 获取其他CPU对应的锁
      struct run *p = kmem[i].freelist; // 将其他CPU空闲资源链表的头节点赋值给p
      if(p) {
        // 窃取一半的内存
        struct run *fp = p; // 快速指针
        struct run *pre = p;//p的前一个
        while (fp && fp->next) {//fp 指针每次向后移动两个节点，而 p 指针和 pre 指针则每次向后移动一个节点
          fp = fp->next->next;
          pre = p;
          p = p->next;
        }//当 fp 到达链表末尾时，p 正好位于链表的中间位置。同时，pre 指针是 p 的前一个节点。
        kmem[id].freelist = kmem[i].freelist; // 将窃取的一半内存分配给当前CPU
        if (p == kmem[i].freelist) {
          // 只有一页内存可用，将其他CPU空闲资源链表置为0
          kmem[i].freelist = 0;
        }
        else {//其他CPU还有空闲资源可用
          kmem[i].freelist = p; // 更新其他CPU空闲资源链表的头指针
          pre->next = 0;// 断开链表
        }
        success = 1;//成功
      }
      release(&kmem[i].lock); // 释放其他CPU对应的锁
      if (success) {
        r = kmem[id].freelist;// 将当前CPU空闲资源链表的头节点赋值给r
        kmem[id].freelist = r->next;// 更新当前CPU空闲资源链表的头指针
        break;
      }
    }
  }
  release(&kmem[id].lock);// 释放当前CPU对应的锁
  pop_off();// 打开中断，允许CPU切换

  if(r)
    memset((char*)r, 5, PGSIZE); // 填充垃圾值到分配的内存块
  return (void*)r;// 返回分配的内存块的指针
}
