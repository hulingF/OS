#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * the kernel's page table.
 * 内核页表，所有CPU共享
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

/*
 * create a direct-map page table for the kernel.
 * 为内核建立直接映射的页表
 */
void
kvminit()
{
  // 首先分配一页物理内存来存放根页表页
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // 调用 kvmmap 将内核所需要的硬件资源映射到物理地址,内存映射IO
  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  // 将根页表页的物理地址写入寄存器 satp 中
  w_satp(MAKE_SATP(kernel_pagetable));
  // 刷新当前 CPU 的 TLB
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  // 虚拟地址不能超出最大范围
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    //使用每一级的 9 位虚拟地址来查找下一级页表或最后一级PTE
    pte_t *pte = &pagetable[PX(level, va)];
    //PTE项有效
    if(*pte & PTE_V) {
      //当前PTE指向的下一级页表
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      //alloc为0或者新页表分配内存失败，直接退出
      if(!alloc || (pagetable = (pte_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      //填充当前的PTE
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  //最终返回最后一级PTE
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  // 校验虚拟地址范围正确性
  if(va >= MAXVA)
    return 0;
  // 获得最后一级PTE
  pte = walk(pagetable, va, 0);
  // ptr==0说明该虚拟地址没有对应的物理地址与之映射
  if(pte == 0)
    return 0;
  // 当前pte已失效
  if((*pte & PTE_V) == 0)
    return 0;
  // 允许用户模式下的指令查看页面
  if((*pte & PTE_U) == 0)
    return 0;
  //pte转为物理地址
  pa = PTE2PA(*pte);
  return pa;
}

//lazy allocation实验
uint64
walkaddr_lazy(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  // 校验虚拟地址范围正确性
  if(va >= MAXVA)
    return 0;
  // 获得最后一级PTE
  pte = walk(pagetable, va, 0);
  // 在exec中，loadseg调用了walkaddr，可能会找不到相应虚拟地址的PTE，此时需要分配物理地址.
  if((pte == 0) || ((*pte & PTE_V) == 0)) {
    if (va > myproc()->sz || va < PGROUNDDOWN(myproc()->trapframe->sp)) {
        return 0;
    } 
    if ((mem = (uint64)kalloc()) == 0) 
      return 0;
    va = PGROUNDDOWN(va);
    if (mappages(myproc()->pagetable, va, PGSIZE, mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0) {
        kfree((void*)mem);
        return 0;
    }
    return mem;
  }
  // 允许用户模式下的指令查看页面
  if((*pte & PTE_U) == 0)
    return 0;
  //pte转为物理地址
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
// 仅在boot启动阶段使用，映射的虚拟地址和物理地址都是连续的，注意在此之前end~PHYSTOP的物理内存块都被串联成单链表供内存分配
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  // 它将一个虚拟地址范围映射到一个物理地址范围。它将范围内地址分割成多页（忽略余数），每次映射一页的顶端地址
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(kernel_pagetable, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);// 开始页地址
  last = PGROUNDDOWN(va + size - 1);// 终止页地址
  for(;;){
    //调用 walk 找到该地址的最后一级 PTE 的地址
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    //如果PTE不空且有效则说明是重新映射
    if(*pte & PTE_V)
      panic("remap");
    //PTE的0~9位是标志位，10~53位是物理页号，54~63位保留
    *pte = PA2PTE(pa) | perm | PTE_V;
    //如果map的虚拟页到达终止页，结束该次mappages
    if(a == last)
      break;
    //当前映射页后移一页
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  //虚拟地址必须页对齐
  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    // 页目录项必须存在
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    // 页目录项必须有效
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    // PTE是中间目录项(不是叶子目录项)
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    // 可选项，是否释放物理内存页
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    //清空pte表项
    *pte = 0;
  }
}


// lazy allocation实验
// lazy allocation可能会造成myproc()-sz以下的内容没有被分配的情况，因此在unmap的过程中可能会出现panic，这是正常情况，需要continue
void
uvmunmap_lazy(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  //虚拟地址必须页对齐
  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      continue;
      // panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      continue;
      // panic("uvmunmap: not mapped");
    // pte不是最后一级的叶子目录项
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    //可选项，是否释放物理内存页
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    //清空pte表项
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
// 创建一个空用户页表
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  // 创建一个空的user页表
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
// uvminit(p->pagetable, initcode, sizeof(initcode));
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;
  // initcode.S不超过一页
  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  // 空页初始化
  memset(mem, 0, PGSIZE);
  // 初始化进程的虚拟地址0映射到实际分配的物理页
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  // 给实际的物理页填充内容
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;
  // 需要映射的下一个地址
  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    // 分配一页物理内存
    mem = kalloc();
    if(mem == 0){
      // 分配过程中失败，则页表应该恢复到原来的大小
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    // 初始化物理页内容
    memset(mem, 0, PGSIZE);
    // 扩展的虚拟地址映射到新的物理地址
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      // 映射失败则释放当前分配的内存页，页表应该恢复到原来的大小
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;
  // 解除用户页表的部分内存映射
  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    // 计算需要解除映射的页数
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    // 从PGROUNDUP(newsz)开始移除npages页，并且释放对应的物理内存(do_free=1)
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // this PTE points to a lower-level page table.
    // PTE项是中间页表目录项
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){    
      // 进入下一级页表进行页表页释放
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      // PTE对应的下级页表释放完毕，PTE项置零
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      // 所有叶子目录的映射本应该在此之前全部移除
      panic("freewalk: leaf");
    }
  }
  // 页目录项全部置零后释放该级页表
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  // 释放用户内存页
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  // 释放页表页
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
// fork创建子进程时会默认为子进程分配物理内存，并将父进程的内存复制到子进程中
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    // 页目录项必须存在
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    // 页目录项必须有效
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    // 虚拟地址从0开始的i对应的物理地址
    pa = PTE2PA(*pte);
    // 页目录项的低10位标志位
    flags = PTE_FLAGS(*pte);
    // 为子进程分配物理内存
    if((mem = kalloc()) == 0)
      goto err;
    // 把父进程的内存搬至子进程
    memmove(mem, (char*)pa, PGSIZE);
    // 完成子进程的页表地址映射
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  // 失败情况下需要释放所有已分配的页
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// lazy allocation实验
// fork()中将父进程的内存复制给子进程的过程中用到了uvmcopy，uvmcopy原本在发现缺失相应的PTE等情况下会panic，这里也要continue掉
int
uvmcopy_lazy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;
      // panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      continue;
      // panic("uvmcopy: page not present");
    // 虚拟地址从0开始的i对应的物理地址
    pa = PTE2PA(*pte);
    // 页目录项的低10位标志位
    flags = PTE_FLAGS(*pte);
    // 为子进程分配物理内存
    if((mem = kalloc()) == 0)
      goto err;
    // 把父进程的内存搬至子进程
    memmove(mem, (char*)pa, PGSIZE);
    // 完成子进程的页表地址映射
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}


// copy on write fork实验
int
uvmcopy_cow(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    // 页目录项必须存在
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    // 页目录项必须有效
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    // 虚拟地址从0开始的i对应的物理地址
    pa = PTE2PA(*pte);
    // 页目录项的低10位标志位
    flags = PTE_FLAGS(*pte);
    // 要设置PTE_COW(1L >> 8)来表明这是一个copy-on-write页，在陷入page fault时需要进行特殊处理。
    // 将PTE_W置零，将该物理页的refc设置为1
    if (flags & PTE_W) {
      flags = (flags | PTE_COW) & (~PTE_W);
      *pte = PA2PTE(pa) | flags;
    } 
    // increase the reference count of the physical page to 1
    refcinc((void*)pa); 
    // 完成子进程的页表地址映射
    if(mappages(new, i, PGSIZE, pa, flags) != 0){
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// allocate a physical address for virtual address va in pagetable
// for copy on write lab
int cow_alloc(proc * p, uint64 va) {
  uint64 pa;
  pte_t *pte;
  uint flags;
   
  if (va >= MAXVA) return -1; 
  va = PGROUNDDOWN(va);
  pte = walk(pagetable, va, 0);
  if (pte == 0) return -1;
  if ((*pte & PTE_V) == 0) return -1;
  pa = PTE2PA(*pte);
  if (pa == 0) return -1;
  flags = PTE_FLAGS(*pte);
   
  if (flags & PTE_COW) {
    char *mem = kalloc();
    if (mem == 0) return -1;
    memmove(mem, (char*)pa, PGSIZE);
    flags = (flags & ~PTE_COW) | PTE_W;
    *pte = PA2PTE((uint64)mem) | flags;
    // kfree((void*)pa);
    if(!p->parent)
      panic("cow no parent")
    if(getrefc(p->parent) == 1){
      pte = walk(p->parent->pagetable,va,0);
      if (pte == 0) return -1;
      if ((*pte & PTE_V) == 0) return -1;
      pa = PTE2PA(*pte);
      if (pa == 0) return -1;
      flags = PTE_FLAGS(*pte) | PTE_W;
      *pte = PA2PTE(pa) | flags;
    }
  }
  return 0;
}



// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
// PTE项标记为用户模式无法访问，用于exec执行用户程序的用户栈区守护页(发生栈溢出时就会报错无法访问特权内存区)
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    // 虚拟地址dstva开始页
    va0 = PGROUNDDOWN(dstva);

    // COW实验
    if (cow_alloc(pagetable, va0) != 0) {
        return -1;
    }

    // 虚拟地址对应的物理地址
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    // 需要copy的字节数，开始页比较特殊（不一定全部copy）
    n = PGSIZE - (dstva - va0);
    // 多出的部分需要截断
    if(n > len)
      n = len;
    // 把src开始处的n个字节拷贝到实际的物理地址处pa0 + (dstva - va0)
    memmove((void *)(pa0 + (dstva - va0)), src, n);
    // 待拷贝字节数减少n，src后移n，dstva跳到下一页开始处（page aligend）
    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    // 虚拟地址srcva的开始页
    va0 = PGROUNDDOWN(srcva);
    // 虚拟地址对应的物理地址
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    // 当前页实际需要拷贝的字节数
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    // 把物理地址pa0 + (srcva - va0)处的n个字节拷贝到dst处
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);
    // 待拷贝字节数减少n，dst后移n，srcva跳到下一页开始处（page aligend）
    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    // srcva虚拟地址的起始页
    va0 = PGROUNDDOWN(srcva);
    // walkaddr模拟分页硬件确定 srcva 的物理地址 pa0
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    // 需要复制的字节数
    n = PGSIZE - (srcva - va0);
    // 确保不多复制
    if(n > max)
      n = max;
    // 待复制字节的源物理地址开始处
    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      // 遇到终止符结束copy
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        // 否则一直复制
        *dst = *p;
      }
      --n;
      --max;
      p++;
      // 可以看出，目标地址(dst==buf)应该是连续一块内存
      dst++;
    }

    // 切换到下一页继续复制
    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}


void
_helper_vmprint(pagetable_t pagetable, int level)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      // this pte points to a valid lower level page table
      uint64 child = PTE2PA(pte);
      for (int j=0; j <= level; j++) {
        printf("..");
        if ((j+1) <= level) {
          printf(" ");
        }
      }
      printf("%d: pte %p pa %p\n", i, pte, child);
      _helper_vmprint((pagetable_t)child, level+1);
    }
    else if (pte & PTE_V) {
      uint64 child = PTE2PA(pte);
      // the lowest valid page table
      printf(".. .. ..%d: pte %p pa %p\n", i, pte, child);
    }
  }
}
   
// print the page table
void vmprint(pagetable_t pagetable)
{
  printf("page table %p\n", pagetable);
  _helper_vmprint(pagetable, 0);
}


pagetable_t
proc_kpt_init()
{
  pagetable_t kpt;
  kpt = uvmcreate();
  if (kpt == 0) return 0;
  uvmmap(kpt, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  uvmmap(kpt, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  uvmmap(kpt, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  uvmmap(kpt, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  uvmmap(kpt, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  uvmmap(kpt, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
  uvmmap(kpt, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
  return kpt;
}
void 
uvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}