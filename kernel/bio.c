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

struct {
  struct spinlock locks[NBUCKETS];  // Buffer Cache自己有一把自旋锁，而每个缓存块也有一把睡眠锁
                         // Buffer Cache的自旋锁保护哪些块已经被缓存的信息
                         // 而每个缓存块的睡眠锁保护对该块内容的读与写
  struct buf buf[NBUF];  // NBUF = MAXOPBLOCKS * 3 = 30

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKETS];
} bcache;

// 首先获取bcache.lock，然后将NBUF个槽位连接成一个链表。初始化之后，所有对于Buffer Cache的访问将通过bcache.head，而不是通过静态数组bcache.buf
void
binit(void)
{
  struct buf *b;

  for(int i=0;i<NBUCKETS;i++)
  {
    //初始化所有锁
    initlock(&bcache.locks[i], "bcache"); 
    //初始化所有链表表头
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  //把所有块插到0号链表里
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){   
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id=blockno%NBUCKETS;
  acquire(&bcache.locks[id]);

  // Is the block already cached?
  for(b = bcache.head[id].next; b != &bcache.head[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head[id].prev; b != &bcache.head[id]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.locks[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // steal
  for(uint i=1;i<NBUCKETS;i++){
    uint steal_id=(id+i)%NBUCKETS;
    acquire(&bcache.locks[steal_id]);
    for(b = bcache.head[steal_id].prev; b != &bcache.head[steal_id]; b = b->prev){ 
      if(b->refcnt == 0){
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1; 

          //从原来链表剥离，查找自己链表的head.next处
          b->next->prev = b->prev;
          b->prev->next = b->next;              
          b->next = bcache.head[id].next;
          b->prev = &bcache.head[id];
          bcache.head[id].next->prev = b;
          bcache.head[id].next = b;  

          release(&bcache.locks[id]);        
          release(&bcache.locks[steal_id]);
          acquiresleep(&b->lock);
          return b;
      } 
    }
    release(&bcache.locks[steal_id]);
  }
  panic("bget: no buffers");
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

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  uint id=(b->blockno)%NBUCKETS;
  releasesleep(&b->lock);

  acquire(&bcache.locks[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[id].next;
    b->prev = &bcache.head;
    bcache.head[id].next->prev = b;
    bcache.head[id].next = b;
  }
  
  release(&bcache.locks);
}

void
bpin(struct buf *b) {
  uint id=(b->blockno)%NBUCKETS;
  acquire(&bcache.locks[id]);
  b->refcnt++;
  release(&bcache.locks[id]);
}

void
bunpin(struct buf *b) {
  uint id=(b->blockno)%NBUCKETS;
  acquire(&bcache.locks[id]);
  b->refcnt--;
  release(&bcache.locks[id]);
}


