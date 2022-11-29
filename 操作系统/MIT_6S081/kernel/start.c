#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
// 定义了 entry.S 中的 stack0 ，它要求 16bit 对齐
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// scratch area for timer interrupt, one per CPU.
// 定义了共享变量，即每个 CPU 的暂存区用于 machine-mode 定时器中断，它是和 timer 驱动之间传递数据用的。
uint64 mscratch0[NCPU * 32];

// assembly code in kernelvec.S for machine-mode timer interrupt.
// 声明了 timer 中断处理函数，在接下来的 timer 初始化函数中被用到
extern void timervec();

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  // 使 CPU 进入 supervisor mode 
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  // 设置了汇编指令 mret 后 PC 指针跳转的函数，也就是 main 函数
  w_mepc((uint64)main);

  // disable paging for now.
  // 暂时关闭了分页功能，即直接使用物理地址
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  // 将所有中断异常处理设定在给 supervisor mode 下
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  // External Interupt | Software Interupt | Timer Interupt
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // ask for clock interrupts.
  // 请求时钟中断，也就是 clock 的初始化
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  // 将 CPU 的 ID 值保存在寄存器 tp 中
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}

// set up to receive timer interrupts in machine mode,
// which arrive at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  // 首先读出 CPU 的 ID
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  // 设置中断时间间隔，这里设置的是 0.1 秒
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..3] : space for timervec to save registers.
  // scratch[4] : address of CLINT MTIMECMP register.
  // scratch[5] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &mscratch0[32 * id];
  scratch[4] = CLINT_MTIMECMP(id);
  scratch[5] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  // 设置中断处理函数
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  // 打开中断
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  // 打开时钟中断
  w_mie(r_mie() | MIE_MTIE);
}
