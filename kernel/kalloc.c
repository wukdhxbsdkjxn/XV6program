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

//修改
struct {
  struct spinlock lock;
  struct run *freelist;
  struct spinlock reflock;
  uint *ref_count;//物理内存引用计数器
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");// 初始化内存分配器的锁
  initlock(&kmem.reflock,"kmemref");// 初始化页面引用计数器的锁
  // end:内核之后的第一个可以内存单元地址，它在kernel.ld中定义
  uint64 rc_pages = ((PHYSTOP - (uint64)end) >> 12) +1; // 物理页数
  rc_pages = ((rc_pages * sizeof(uint)) >> 12) + 1;// 计算引用计数器占用的页数
  kmem.ref_count = (uint*)end;//将存放引用计数器的起始地址设置为 end，即内核之后的第一个可用内存单元地址
  uint64 rc_offset = rc_pages << 12;  // 计算引用计数器存储空间大小
  freerange(end + rc_offset, (void*)PHYSTOP);// 调用freerange函数初始化空闲链表
}
 
inline int
kgetrefindex(void *pa)//这是一个内联函数，用于将物理地址转换为对应的引用计数器索引
{
   return ((char*)pa - (char*)PGROUNDUP((uint64)end)) >> 12;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // 初始化kmem.ref_count
     kmem.ref_count[kgetrefindex((void *)p)] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
//释放内存时查看引用计数是否为0，为0释放内存，否则返回，由其他进程继续使用
  acquire(&kmem.lock);
  if(--kmem.ref_count[kgetrefindex(pa)])
  {
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
  {
    kmem.freelist = r->next;
    kmem.ref_count[kgetrefindex((void *)r)]=1;//在分配页面时将引用计数器设置为1
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
//辅助函数
int kgetref(void *pa)
{//获取给定物理地址 pa 对应的引用计数
  return kmem.ref_count[kgetrefindex(pa)];
}

void kaddref(void *pa)
{//给给定的物理地址 pa 对应的引用计数加一
  kmem.ref_count[kgetrefindex(pa)]++;
}

inline void
acquire_refcnt()
{//获取页面引用计数器的锁
  acquire(&kmem.reflock);
}

inline void
release_refcnt()
{//释放之前获取的页面引用计数器的锁
  release(&kmem.reflock);
}