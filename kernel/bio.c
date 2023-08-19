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

// bucket number for bufmap
#define NBUFMAP_BUCKET 13
// hash function for bufmap
#define BUFMAP_HASH(dev, blockno) ((((dev) << 27) | (blockno)) % NBUFMAP_BUCKET)

struct {
    struct spinlock lock;
    struct buf      buf[NBUF];

    // // Linked list of all buffers, through prev/next.
    // // Sorted by how recently the buffer was used.
    // // head.next is most recent, head.prev is least.
    // struct buf head;

    // lab8
    struct buf      buckets[NBUFMAP_BUCKET];
    struct spinlock bucketlocks[NBUFMAP_BUCKET];
} bcache;

void
binit(void)
{
    for (int i = 0; i < NBUFMAP_BUCKET; i++) {
        initlock(&bcache.bucketlocks[i], "bcache_bucket");
        bcache.buckets[i].next = 0;
    }

    for (int i = 0; i < NBUF; i++) {
        struct buf* b = &bcache.buf[i];
        initsleeplock(&b->lock, "buffer");
        b->lastuse = 0;
        b->refcnt  = 0;
        b->next                = bcache.buckets[0].next;
        bcache.buckets[0].next = b;
    }

    initlock(&bcache.lock, "bcache");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // lab8
  uint key = BUFMAP_HASH(dev, blockno);

  acquire(&bcache.bucketlocks[key]);
  for (b = bcache.buckets[key].next; b; b = b->next) {
      if (b->dev == dev && b->blockno == blockno) {
          b->refcnt++;
          release(&bcache.bucketlocks[key]);
          acquiresleep(&b->lock);
          return b;
      }
  }
  release(&bcache.bucketlocks[key]);

  acquire(&bcache.lock);
  for (b = bcache.buckets[key].next; b; b = b->next) {
      if (b->dev == dev && b->blockno == blockno) {
          acquire(&bcache.bucketlocks[key]);  // must do, for `refcnt++`
          b->refcnt++;
          release(&bcache.bucketlocks[key]);
          release(&bcache.lock);
          acquiresleep(&b->lock);
          return b;
      }
  }

  struct buf* before_least   = 0;
  uint        holding_bucket = -1;
  for (int i = 0; i < NBUFMAP_BUCKET; i++) {
      acquire(&bcache.bucketlocks[i]);
      int newfound = 0;

      for (b = &bcache.buckets[i]; b->next; b = b->next) {
          if (b->next->refcnt == 0 &&
              (!before_least ||
               b->next->lastuse < before_least->next->lastuse)) {
              before_least = b;
              newfound     = 1;
          }
      }

      if (!newfound) {
          release(&bcache.bucketlocks[i]);
      }
      else {
          if (holding_bucket != -1)
              release(&bcache.bucketlocks[holding_bucket]);
          holding_bucket = i;
      }
  }
  if (!before_least) {
      panic("bget: no buffers");
  }
  b = before_least->next;

  if (holding_bucket != key) {
      before_least->next = b->next;
      release(&bcache.bucketlocks[holding_bucket]);
      acquire(&bcache.bucketlocks[key]);
      b->next                  = bcache.buckets[key].next;
      bcache.buckets[key].next = b;
  }

  b->dev     = dev;
  b->blockno = blockno;
  b->refcnt  = 1;
  b->valid   = 0;
  release(&bcache.bucketlocks[key]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
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

  // lab8
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bucketlocks[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
      b->lastuse = ticks;
  }
  release(&bcache.bucketlocks[key]);
}

void
bpin(struct buf *b) {
    // lab8
    uint key = BUFMAP_HASH(b->dev, b->blockno);

    acquire(&bcache.bucketlocks[key]);
    b->refcnt++;
    release(&bcache.bucketlocks[key]);
}

void
bunpin(struct buf *b) {
    // lab8
    uint key = BUFMAP_HASH(b->dev, b->blockno);

    acquire(&bcache.bucketlocks[key]);
    b->refcnt--;
    release(&bcache.bucketlocks[key]);
}


