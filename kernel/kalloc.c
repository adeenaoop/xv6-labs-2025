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
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
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

// Free the page of physical memory pointed at by pa,
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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Superpage allocation (2MB pages)
#define SUPERPG_SIZE (2 * 1024 * 1024)
#define MAX_SUPERPAGES 16

struct {
  struct spinlock lock;
  int allocated[MAX_SUPERPAGES];
  uint64 phys_addrs[MAX_SUPERPAGES];
} superalloc;

void
superinit(void)
{
  initlock(&superalloc.lock, "superalloc");
  for(int i = 0; i < MAX_SUPERPAGES; i++) {
    superalloc.allocated[i] = 0;
    superalloc.phys_addrs[i] = 0;
  }
}

void*
superalloc_page(void)
{
  acquire(&superalloc.lock);
  
  for(int i = 0; i < MAX_SUPERPAGES; i++) {
    if(superalloc.allocated[i] == 0) {
      superalloc.allocated[i] = 1;
      // Use a simple addressing scheme for the lab
      superalloc.phys_addrs[i] = (i + 1) * SUPERPG_SIZE;
      void *result = (void*)superalloc.phys_addrs[i];
      release(&superalloc.lock);
      return result;
    }
  }
  
  release(&superalloc.lock);
  return 0;
}

void
superfree_page(void *pa)
{
  if(pa == 0)
    return;
    
  acquire(&superalloc.lock);
  
  uint64 addr = (uint64)pa;
  for(int i = 0; i < MAX_SUPERPAGES; i++) {
    if(superalloc.allocated[i] == 1 && superalloc.phys_addrs[i] == addr) {
      superalloc.allocated[i] = 0;
      superalloc.phys_addrs[i] = 0;
      release(&superalloc.lock);
      return;
    }
  }
  
  release(&superalloc.lock);
  panic("superfree_page: invalid superpage");
}

int
is_superpage_aligned(void *pa)
{
  return ((uint64)pa & (SUPERPG_SIZE - 1)) == 0;
}
