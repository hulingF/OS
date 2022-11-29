// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
// 物理内存分配器，分配单位是4k字节

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// 每个空闲页的 run 结构体存储在空闲页本身
struct run {
  struct run *next;
};

// 空闲链表由一个自旋锁保护
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// struct to maintain the ref counts
struct {
  struct spinlock lock;
  int count[PGROUNDUP(PHYSTOP) / PGSIZE];
} refc;

void
refcinit()
{
  initlock(&refc.lock, "refc");
  for (int i = 0; i < PGROUNDUP(PHYSTOP) / PGSIZE; i++) {
    refc.count[i] = 0;
  }
}
   
void
refcinc(void *pa)
{
  acquire(&refc.lock);
  refc.count[PA2IDX(pa)]++;
  release(&refc.lock);
}
   
void
refcdec(void *pa)
{
  acquire(&refc.lock);
  refc.count[PA2IDX(pa)]--;
  release(&refc.lock);
}
   
int
getrefc(void *pa)
{
  return refc.count[PA2IDX(pa)];
}

void
kinit()
{
  // COW实验
  refcinit();

  initlock(&kmem.lock, "kmem");
  // 初始空闲页链表，以保存内核地址结束end到PHYSTOP之间的每一页
  // xv6 应该通过解析硬件提供的配置信息来确定有多少物理内存可用。
  // 但是它没有做，而是假设机器有128M字节的RAM
  freerange(end, (void*)PHYSTOP);

  // COW实验
  // 每一页的初始refc.count都变成-1，因此需要在kinit时再给每一个物理页的refc.count加1
  char *p;
  p = (char*)PGROUNDUP((uint64)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE) {
    refcinc((void*)p);
  }
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

  refcdec(pa);// cow实验
  if (getrefc(pa) > 0) return;// cow实验
  
  // Fill with junk to catch dangling refs.
  // 这将使得释放内存后使用内存的代码(使用悬空引用)读取垃圾而不是旧的有效内容；
  // 希望这将导致这类代码更快地崩溃
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

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    refcinc((void*)r);// cow实验
  }
  
  // kalloc 移除并返回空闲链表中的第一个元素
  return (void*)r;
}
