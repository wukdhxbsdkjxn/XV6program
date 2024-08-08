#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int e1000_transmit(struct mbuf *m)
{
  //
  // 在这里填写你的代码。
  //
  // mbuf包含一个以太网帧；将其程序化到TX描述符环中，以便e1000发送它。保存一个指针，以便在发送后释放。
  //
  acquire(&e1000_lock);  // 获取e1000锁，保证线程安全
  uint32 idx = regs[E1000_TDT];  // 获取当前可用的TX描述符索引
  struct tx_desc* desc = &tx_ring[idx];  // 获取当前可用的TX描述符
//检查TX描述符的状态字段，如果标志位E1000_TXD_STAT_DD未设置（表示空闲），则表示发送缓冲区已经被占用，直接返回失败。
  if((desc->status & E1000_TXD_STAT_DD) == 0){
    release(&e1000_lock);
    printf("buffer overflow\n");
    return -1;
  }
  if(tx_mbufs[idx])
    mbuffree(tx_mbufs[idx]);  // 释放之前存储的mbuf

  desc->addr = (uint64)m->head;  // 设置TX描述符的地址字段为mbuf的头部地址
  desc->length = m->len;  // 设置TX描述符的长度字段为mbuf的长度
  desc->cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;  // 设置TX描述符的命令字段
  tx_mbufs[idx] = m;  // 保存mbuf的指针，以便在发送完成后释放

  regs[E1000_TDT] = (idx + 1) % TX_RING_SIZE;  // 更新TX描述符环的TDT（Transmit Descriptor Tail）指针
  __sync_synchronize();  // 同步内存，确保写入操作完成
  release(&e1000_lock);  // 释放e1000锁
  return 0;
}

static void e1000_recv(void)
{ // 在这里填写你的代码。
  // 检查从e1000接收到的数据包
  // 为每个数据包创建并传递一个mbuf（使用net_rx()函数）。
  int idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;  // 计算下一个要处理的RX描述符索引
  struct rx_desc* desc = &rx_ring[idx];  // 获取当前要处理的RX描述符

  while(desc->status & E1000_RXD_STAT_DD){//数据包已到达
    acquire(&e1000_lock);  // 获取e1000锁，保证线程安全性

    struct mbuf *buf = rx_mbufs[idx];  // 从rx_mbufs中获取存储的mbuf指针
    mbufput(buf, desc->length);  // 将desc->length赋值给buf的长度字段
    rx_mbufs[idx] = mbufalloc(0);  // 分配新的mbuf内存空间
    if (!rx_mbufs[idx])
      panic("mbuf alloc failed");  // 如果mbuf分配失败，则触发panic异常

    desc->addr = (uint64) rx_mbufs[idx]->head;  // 设置RX描述符的地址字段为新mbuf的头部地址
    desc->status = 0;  // 清空RX描述符的状态字段，表示已经处理完成
    regs[E1000_RDT] = idx;  // 更新RX描述符环的RDT（Receive Descriptor Tail）指针
    __sync_synchronize();  // 同步内存，确保写入操作完成

    release(&e1000_lock);  // 释放e1000锁
    net_rx(buf);  // 将接收到的mbuf传递给上层网络栈进行进一步处理
    idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;  // 计算下一个要处理的RX描述符索引
    desc = &rx_ring[idx];  // 获取当前要处理的RX描述符
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
