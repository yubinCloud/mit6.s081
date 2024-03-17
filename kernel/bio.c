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

#define N_BUCKETS 13   // buffer buckets 的数量

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

struct BcacheBucket {
  struct spinlock lock;
  struct buf head;
} hash_table[N_BUCKETS];

void
binit(void)
{
  struct buf *b;
  struct BcacheBucket *bucket;
  // 初始化 hash table
  for (int i = 0; i < N_BUCKETS; i++) {
    bucket = hash_table + i;
    initlock(&bucket->lock, "bcache.bucket");
    bucket->head.prev = &bucket->head;
    bucket->head.next = &bucket->head;
  }
  // 初始化 bcache
  initlock(&bcache.lock, "bcache");
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->tick = 0;
    initsleeplock(&b->lock, "buffer");
  }
}

// 在一个 buffer 中填入缓存块的信息
void
replace_buffer(struct buf* buffer, uint dev, uint blockno, uint tick) {
  buffer->dev = dev;
  buffer->blockno = blockno;
  buffer->tick = tick;
  buffer->valid = 0;  // 表示数据还未写入 buffer 的 data 字段中
  buffer->refcnt = 1;
}

void
bucket_add(struct BcacheBucket *bucket, struct buf *buffer) {
  buffer->next = bucket->head.next;
  bucket->head.next->prev = buffer;
  bucket->head.next = buffer;
  buffer->prev = &bucket->head;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucketNo = blockno % N_BUCKETS;  // 根据 blockno 计算 hash table 的 bucket 序号
  struct BcacheBucket *bucket = hash_table + bucketNo;
  acquire(&bucket->lock);
  
  // 检查这个 block 是否存在于 cache 中
  for (b = bucket->head.next; b != &bucket->head; b = b->next) {  // 遍历这个 bucket 的链表
    // 如果没找到：
    if (b->dev != dev || b->blockno != blockno) {
      continue;
    }
    // 如果找到了：
    b->tick = ticks;
    b->refcnt++;
    release(&bucket->lock);
    acquiresleep(&b->lock);
    return b;
  }

  // 如果 bucket 中没有找到 cache，则需要从 kcache 中找一块未使用的 buffer
  acquire(&bcache.lock);
  struct buf* victim = 0;  // 根据 LRU 策略所决定淘汰的 buffer
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {  // 遍历所有 buffer，寻找一个未使用的 buffer
    // 如果 buffer 不能使用：
    if (b->refcnt != 0) {
      continue;
    }
    // 如果 buffer 可以使用，则根据时间戳来决定是否将它作为 victim
    else {
      if (victim == 0 || victim->tick > b->tick) {
        victim = b;
      }
    }
  }

  // 是否能够找到 victim?
  if (victim == 0) {
    panic("bget: no buffers");
  }

  // 将 victim 的 buffer 中填入数据，并将其移动到 bucket 中
  if (victim->tick == 0) {  // 如果 victim 还未加入到 hash table 中
    replace_buffer(victim, dev, blockno, ticks);
    bucket_add(bucket, victim);
  } else if ((victim->blockno % N_BUCKETS) != bucketNo) {  // 如果 victim 之前所在的 bucket 与现在需要加入的 bucket 不同的话
    struct BcacheBucket *old_bucket = &hash_table[victim->blockno % N_BUCKETS];
    acquire(&old_bucket->lock);
    replace_buffer(victim, dev, blockno, ticks);
    victim->prev->next = victim->next;
    victim->next->prev = victim->prev;
    release(&old_bucket->lock);
    bucket_add(bucket, victim);
  } else {  // 如果 victim 之前就是在现在需要加入的 bucket 的话
    replace_buffer(victim, dev, blockno, ticks);
  }

  // 释放掉相关的 lock
  release(&bcache.lock);
  release(&bucket->lock);
  acquiresleep(&victim->lock);

  return victim;
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

  releasesleep(&b->lock);

  struct BcacheBucket *bucket = &hash_table[b->blockno % N_BUCKETS];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}

void
bpin(struct buf *b) {
  struct BcacheBucket *bucket = &hash_table[b->blockno % N_BUCKETS];
  acquire(&bucket->lock);
  b->refcnt++;
  release(&bucket->lock);
}

void
bunpin(struct buf *b) {
  struct BcacheBucket *bucket = &hash_table[b->blockno % N_BUCKETS];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}


