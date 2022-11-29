#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap()
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  // SPP 位表示 trap 是来自用户模式还是监督者模式，并控制sret 返回到什么模式
  // 只能处理来自用户空间的中断、异常、系统调用
  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  // 我们现在已经进入内核空间了，在内核中再发生的 trap 将由 kernelvec 处理
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  // 它保存了 sepc(用户 PC)，这也是因为 usertrap 中可能会有一个进程切换，导致 sepc 被覆盖
  p->trapframe->epc = r_sepc();
  
  // 系统调用交给syscall处理
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    // 返回地址为ecall指令的下一个指令
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    // 开启设备中断
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok

    // Alarm实验
    // 如果发生时钟中断且进程未处于handler函数内
    if (which_dev == 2 && p->in_handler == 0) {
      // 跳数+1
      p->ticks += 1;
      // 到达规定的时间间隔，需要转去执行handler函数
      if ((p->ticks == p->interval) && (p->interval != 0)) {
        // 设置标志位in_handler
        p->in_handler = 1;
        // 重置跳数
        p->ticks = 0;
        // 保存现场
        p->saved_epc = p->trapframe->epc;
        p->saved_ra = p->trapframe->ra;
        p->saved_sp = p->trapframe->sp;
        p->saved_gp = p->trapframe->gp;
        p->saved_tp = p->trapframe->tp;
        p->saved_t0 = p->trapframe->t0;
        p->saved_t1 = p->trapframe->t1;
        p->saved_t2 = p->trapframe->t2;
        p->saved_t3 = p->trapframe->t3;
        p->saved_t4 = p->trapframe->t4;
        p->saved_t5 = p->trapframe->t5;
        p->saved_t6 = p->trapframe->t6;
        p->saved_s0 = p->trapframe->s0;
        p->saved_s1 = p->trapframe->s1;
        p->saved_s2 = p->trapframe->s2;
        p->saved_s3 = p->trapframe->s3;
        p->saved_s4 = p->trapframe->s4;
        p->saved_s5 = p->trapframe->s5;
        p->saved_s6 = p->trapframe->s6;
        p->saved_s7 = p->trapframe->s7;
        p->saved_s8 = p->trapframe->s8;
        p->saved_s9 = p->trapframe->s9;
        p->saved_s10 = p->trapframe->s10;
        p->saved_s11 = p->trapframe->s11;
        p->saved_a0 = p->trapframe->a0;
        p->saved_a1 = p->trapframe->a1;
        p->saved_a2 = p->trapframe->a2;
        p->saved_a3 = p->trapframe->a3;
        p->saved_a4 = p->trapframe->a4;
        p->saved_a5 = p->trapframe->a5;
        p->saved_a6 = p->trapframe->a6;
        p->saved_a7 = p->trapframe->a7;
        // 通过向p->trapframe->epc加载handler地址实现的，这样当从usertrap()中返回时
        // pc在恢复现场时会加载p->trapframe->epc，这样就会跳转到handler地址。
        // 在跳转到handler之前先要保存p->trapframe的寄存器，因为执行handler函数会导致这些寄存器被覆盖。
        p->trapframe->epc = (uint64)p->handler;
      }
    }
  } else {
    // 在sbrk()时只增长进程的myproc()->sz而不实际分配内存。
    // 在kernel/trap.c中修改从而在产生page fault时分配物理内存给相应的虚拟地址。
    // 相应的虚拟地址可以通过r_stval()获得。r_scause()为13或15表明trap的原因是page fault
    if (r_scause() == 13 || r_scause() == 15) {
      // page fault
      uint64 va = r_stval();

      // copy on fork 实验
      if (va >= MAXVA || (va <= PGROUNDDOWN(p->trapframe->sp) && va >= PGROUNDDOWN(p->trapframe->sp) - PGSIZE)) {
        p->killed = 1;
      } else if (cow_alloc(p, va) != 0) {
        p->killed = 1;
      }
      // copy on fork 实验


      // sbrk虚拟内存分配实验
      char *mem;
      va = PGROUNDDOWN(va);
      if ((mem = kalloc()) == 0) {
		    panic("cannot allocate for lazy alloc\n");
        exit(-1);
      } 
      if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0) {
        kfree(mem);
		    panic("cannot map for lazy alloc\n");
		    exit(-1);
      }
      // sbrk虚拟内存分配实验

    }
    else {
      // 发生异常，内核杀死异常进程
      printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
      printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
      p->killed = 1;
    }
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  // 时钟中断处理，让出CPU重新调度
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
// 开始回到用户空间，首先设置 RISC-V 控制寄存器，为以后用户空间 trap 做准备
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  // 返回用户空间时关闭中断
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  // 改变 stvec 指向 uservec
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  // 准备 uservec 所依赖的 trapframe 字段
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap; 
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  // 在sret前设置sstatus寄存器，确保返回用户模式
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  // 在sret前设置sstatus寄存器，将 sepc 设置为先前保存的用户程序计数器
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  // 用户进程页表
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  // 调用 userret
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  // this is a supervisor external interrupt, via PLIC.
  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // irq indicates which device interrupted.
    int irq = plic_claim();
    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);
    return 1;
  } 
  // software interrupt from a machine-mode timer interrupt,
  // forwarded by timervec in kernelvec.S
  else if(scause == 0x8000000000000001L){
    if(cpuid() == 0){
      clockintr();
    } 
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);
    return 2;
  } 
  else {
    return 0;
  }
}