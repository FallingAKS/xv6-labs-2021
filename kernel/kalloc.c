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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];  // lab8

void
kinit()
{
    // lab8
    for (int i = 0; i < NCPU; ++i) {
        char name[8];
        snprintf(name, 8, "kmem_%d", i);
        initlock(&kmem[i].lock, name);
    }
  freerange(end, (void*)PHYSTOP);
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // lab8
  //不许中断
  push_off();
  int cid = cpuid();
  pop_off();
  acquire(&kmem[cid].lock);
  r->next            = kmem[cid].freelist;
  kmem[cid].freelist = r;
  release(&kmem[cid].lock);
}

// lab8
void steal(int cid)
{
    int stolen_num = 0;
    for (int i = 0; i < NCPU; ++i) {
        if (i == cid)
            continue;
        acquire(&kmem[i].lock);

        struct run* r = kmem[i].freelist;
        while (r && stolen_num < NSTEAL) {
            kmem[i].freelist   = r->next;
            r->next            = kmem[cid].freelist;
            kmem[cid].freelist = r;
            r                  = kmem[i].freelist;
            ++stolen_num;
        }

        release(&kmem[i].lock);
        if (stolen_num == NSTEAL)
            break;
    }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void* kalloc(void)
{
  struct run *r;

  // lab8
  //不许中断
  push_off();
  int cid = cpuid();
  pop_off();
  acquire(&kmem[cid].lock);

  if (!kmem[cid].freelist) {
      steal(cid);
  }

  r = kmem[cid].freelist;
  if(r)
      kmem[cid].freelist = r->next;
  release(&kmem[cid].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
