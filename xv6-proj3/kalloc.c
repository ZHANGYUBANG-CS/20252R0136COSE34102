// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// [CoW] 定义用于引用计数的结构
struct {
  int num_free_pages;
  uint refcount[PHYSTOP >> PGSHIFT]; // 引用计数数组
} pmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
  // [CoW] 初始化引用计数为 0
  memset(&pmem.refcount, 0, sizeof(uint) * (PHYSTOP >> PGSHIFT));
  pmem.num_free_pages = 0;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;
  uint pa;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // [CoW] 获取锁（如果系统已初始化）
  if(kmem.use_lock)
    acquire(&kmem.lock);
  
  pa = V2P(v);
  
  // [CoW] 检查引用计数
  // 如果引用计数大于 1，说明还有其他进程在使用该页，
  // 我们只递减计数，不真正释放内存。
  if(pmem.refcount[pa >> PGSHIFT] > 1) {
    pmem.refcount[pa >> PGSHIFT]--;
    if(kmem.use_lock)
      release(&kmem.lock);
    return; 
  }

  // 如果代码运行到这里，说明引用计数为 1 (或 0，如果是初始化阶段)，
  // 我们可以真正释放该页了。
  pmem.refcount[pa >> PGSHIFT] = 0;

  // Fill with junk to catch dangling refs.
  // [CoW] 注意：必须在确定要释放后才填垃圾数据
  memset(v, 1, PGSIZE);

  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;

  pmem.num_free_pages++;

  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    // [CoW] 分配新页时，引用计数设为 1
    pmem.refcount[V2P((char*)r) >> PGSHIFT] = 1;
  }
    
  pmem.num_free_pages--;

  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

int
freemem(void)
{
  return pmem.num_free_pages;
}

// [CoW] 获取物理地址 pa 的引用计数
uint
get_refcount(uint pa)
{
  uint count;
  if(kmem.use_lock) acquire(&kmem.lock);
  count = pmem.refcount[pa >> PGSHIFT];
  if(kmem.use_lock) release(&kmem.lock);
  return count;
}

// [CoW] 增加物理地址 pa 的引用计数
void
inc_refcount(uint pa)
{
  if(kmem.use_lock) acquire(&kmem.lock);
  pmem.refcount[pa >> PGSHIFT]++;
  if(kmem.use_lock) release(&kmem.lock);
}

// [CoW] 减少物理地址 pa 的引用计数
void  
dec_refcount(uint pa)
{
  if(kmem.use_lock) acquire(&kmem.lock);
  pmem.refcount[pa >> PGSHIFT]--;
  if(kmem.use_lock) release(&kmem.lock);
}