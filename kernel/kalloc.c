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

struct Kmem {
  struct spinlock lock;
  struct run *freelist;
} kmems[NCPU];  // 每个 CPU 一个 kmem

int freelist_size;  // 每个 kmem 上 freelist 的最初大小

void
kinit()
{
  // 初始化 kmems 的 lock
  for (int i = 0; i < NCPU; i++) {
    char buf[10];
    snprintf(buf, 10, "kmem-%d", i);
    initlock(&kmems[i].lock, buf);
    kmems[i].freelist = 0;
  }

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  
  // 计算 freelist_size
  int page_num = 0;
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    page_num++;
  }
  freelist_size = page_num / NCPU + 1;

  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// 计算物理地址 pa 应该由哪个 kmem 的 freelist 来管
int kmem_number(void* pa) {
  return ( (uint64)pa - (uint64)end ) / PGSIZE / freelist_size;
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  struct Kmem* kmem;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  kmem = kmems + kmem_number(pa);

  acquire(&kmem->lock);
  r->next = kmem->freelist;
  kmem->freelist = r;  // 把归还的 page 放到列表头部
  release(&kmem->lock);
}

struct Kmem*
find_freelist()
{
  int cpu_id = cpuid();
  struct Kmem* kmem = kmems + cpu_id;

  // 先检查自己的 freelist 是否能够分配
  acquire(&kmem->lock);
  if (kmem->freelist) {
    return kmem;
  }
  release(&kmem->lock);

  // 如果自己的 freelist 空了的话，就从其他人那里找
  for (int i = 0; i < NCPU; i++) {
    if (i == cpu_id) {
      continue;
    }
    kmem = kmems + i;
    acquire(&kmem->lock);
    if (kmem->freelist) {
      return kmem;
    }
    release(&kmem->lock);
  }

  return 0;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  
  // 找到一个 freelist 不为空的 kmem
  struct Kmem *kmem = find_freelist();
  if (kmem == 0) {
    return 0;
  }
  // 从 freelist 列表头部取出一个空闲的 page
  r = kmem->freelist;
  if(r)
    kmem->freelist = r->next;
  release(&kmem->lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
