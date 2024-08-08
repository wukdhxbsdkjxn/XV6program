// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUFMAP_BUCKET 13//分成多少个桶来存储缓冲区映射关系
//哈希函数，用于根据给定的设备号dev和块号blockno来计算对应的桶索引，从而定位到缓冲区映射表中的某个桶
#define BUFMAP_HASH(dev, blockno) ((((dev)<<27)|(blockno))%NBUFMAP_BUCKET)

struct {
  struct buf buf[NBUF]; // 缓冲区数组，NBUF是实际需要的缓冲区的数量
  struct spinlock eviction_lock;//自旋锁，用于保护缓冲区
  struct buf bufmap[NBUFMAP_BUCKET];// 缓冲区对应的桶，可以根据设备号和块号找到对应的桶
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];// 桶的锁，用于保护并发访问桶的操作
} bcache;//块缓存

void
binit(void)
{
  // 初始化bufmap
  for(int i=0;i<NBUFMAP_BUCKET;i++) {
    initlock(&bcache.bufmap_locks[i], "bcache_bufmap");// 初始化bufmap的锁
    bcache.bufmap[i].next = 0;// 将bufmap的next字段初始化为0
  }

   // 初始化buffers
  for(int i=0;i<NBUF;i++){
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0; // 将buffer的lastuse字段初始化为0
    b->refcnt = 0;
    // put all the buffers into bufmap[0]
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }

  initlock(&bcache.eviction_lock, "bcache_eviction");
}

//在设备dev上查找块。
//如果没有找到，分配一个缓冲区。
//无论哪种情况，都返回锁定的缓冲区。
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = BUFMAP_HASH(dev, blockno);// 根据设备和块号计算哈希值，确定bufmap的索引位置

  acquire(&bcache.bufmap_locks[key]);// 获取bufmap对应的锁

  // 检查是否已经缓存了该块
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;// 引用计数加1
      release(&bcache.bufmap_locks[key]);// 释放bufmap的锁
      acquiresleep(&b->lock);// 获取缓冲区的睡眠锁
      return b;// 返回被锁定的缓冲区
    }
  }
  release(&bcache.bufmap_locks[key]);// 释放bufmap的锁
  acquire(&bcache.eviction_lock);// 获取eviction锁，保护缓冲区的驱逐（eviction）过程

  // 缓冲区未被缓存
  // 回收最近未使用的未使用缓冲区
   for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      acquire(&bcache.bufmap_locks[key]); // 获取bufmap锁
      b->refcnt++;//引用计数加1
      release(&bcache.bufmap_locks[key]);// 释放bufmap的锁
      release(&bcache.eviction_lock);// 释放eviction锁
      acquiresleep(&b->lock);
      return b;
    }
  }
  struct buf *before_least = 0; 
  uint holding_bucket = -1;
  for(int i = 0; i < NBUFMAP_BUCKET; i++){
    // 在获取锁之前，要么不持有任何锁，要么只持有当前桶左侧的桶的锁
    // 所以这里不会出现循环等待的情况（避免死锁）
    acquire(&bcache.bufmap_locks[i]);// 获取bufmap中桶对应的锁
    int newfound = 0; // new least-recently-used buf found in this bucket
    for(b = &bcache.bufmap[i]; b->next; b = b->next) {
      if(b->next->refcnt == 0 && (!before_least || b->next->lastuse < before_least->next->lastuse)) {
        before_least = b;
        newfound = 1;
      } // 更新最近未使用的缓冲区的前一个节点
    }
    if(!newfound) {// 如果在当前桶中没有找到最近未使用的缓冲区，则释放该锁
      release(&bcache.bufmap_locks[i]);
    } else {
      if(holding_bucket != -1) release(&bcache.bufmap_locks[holding_bucket]);
      holding_bucket = i;
      // keep holding this bucket's lock....
    }
  }
  if(!before_least) { // 如果没有找到未使用的缓冲区，则发生错误
    panic("bget: no buffers");
  }
  b = before_least->next;
  
  if(holding_bucket != key) {
     // 从原来的桶中移除缓冲区
    before_least->next = b->next;
    release(&bcache.bufmap_locks[holding_bucket]);// 释放持有的桶的锁
    // 重新计算哈希并将其添加到目标桶中
    acquire(&bcache.bufmap_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }
  
  b->dev = dev; // 设置缓冲区的设备号
  b->blockno = blockno; // 设置缓冲区的块号
  b->refcnt = 1; // 引用计数设置为1，表示有一个进程使用了该缓冲区
  b->valid = 0; // 缓冲区的内容无效
  release(&bcache.bufmap_locks[key]); // 释放bufmap的锁
  release(&bcache.eviction_lock); // 释放eviction锁
  acquiresleep(&b->lock); // 获取缓冲区的睡眠锁
  return b; // 返回被锁定的缓冲区
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// 释放一个被锁定的缓冲区。
void brelse(struct buf *b) {
  if(!holdingsleep(&b->lock)) // 检查当前线程是否持有睡眠锁，如果没有持有，则发生错误
    panic("brelse");

  releasesleep(&b->lock); // 释放睡眠锁

  uint key = BUFMAP_HASH(b->dev, b->blockno); // 根据设备和块号计算哈希值，确定bufmap的索引位置
  acquire(&bcache.bufmap_locks[key]); // 获取bufmap对应的锁
  b->refcnt--; // 引用计数减1，表示当前线程不再使用该缓冲区
  if (b->refcnt == 0) { // 如果引用计数为0，则表示缓冲区没有被其他线程使用
    b->lastuse = ticks; // 更新最后使用时间为当前系统滴答数
  }
  release(&bcache.bufmap_locks[key]); // 释放bufmap的锁

}

// 将缓冲区锁定，使其不能被回收。
void bpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno); // 根据设备和块号计算哈希值，确定bufmap的索引位置

  acquire(&bcache.bufmap_locks[key]); // 获取bufmap对应的锁
  b->refcnt++; // 引用计数加1，表示有一个进程正在使用该缓冲区
  release(&bcache.bufmap_locks[key]); // 释放bufmap的锁
}

// 取消对缓冲区的锁定，允许其被回收。
void bunpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno); // 根据设备和块号计算哈希值，确定bufmap的索引位置

  acquire(&bcache.bufmap_locks[key]); // 获取bufmap对应的锁
  b->refcnt--; // 引用计数减1，表示有一个进程不再使用该缓冲区
  release(&bcache.bufmap_locks[key]); // 释放bufmap的锁
}

