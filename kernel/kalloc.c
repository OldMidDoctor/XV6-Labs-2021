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

struct kmem{
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct kmem kmems[NCPU];

void
kinit()
{
  push_off();
  int currentid = cpuid();
  pop_off();
  printf("# cpuId:%d \n",currentid);
  for (int i = 0; i < NCPU; i++){
    initlock(&kmems[i].lock, "kmem");
  }
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  printf("# kinit end:%d \n",currentid);
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

  push_off();
  int currentid = cpuid();
  pop_off();

  acquire(&kmems[currentid].lock);
  r->next = kmems[currentid].freelist;
  kmems[currentid].freelist = r;
  release(&kmems[currentid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  int currentid = cpuid();
  pop_off();

  struct run *r;

  acquire(&kmems[currentid].lock);
  r = kmems[currentid].freelist;
  if(r)
    kmems[currentid].freelist = r->next;
  else{
    int nextid = 0;
    struct run *nextr;
    for (int i = 1; i < NCPU; i++){
      nextid = (i + currentid) % NCPU;
      acquire(&kmems[nextid].lock);
      nextr = kmems[nextid].freelist;
      if (nextr){
        kmems[nextid].freelist = nextr->next;
        nextr->next = r;
        r = nextr;
        kmems[currentid].freelist = r->next;
        release(&kmems[nextid].lock);
        break;
      }
      release(&kmems[nextid].lock);
    }

  }
  release(&kmems[currentid].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
