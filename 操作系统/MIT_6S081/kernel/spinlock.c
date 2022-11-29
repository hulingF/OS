// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  // 关中断，保存最外层临界区开始时的中断状态
  push_off(); // disable interrupts to avoid deadlock.
  // CPU拿到锁后并且还没有release之前不能再次获取锁，否则会发生死锁
  if(holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  // 原子指令，自旋swap
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  // 设置内存屏障，防止指令重排
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

// Release the lock.
void
release(struct spinlock *lk)
{
  // CPU首先已经拿到了锁才能释放锁
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  // 设置内存屏障，防止指令重排
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  // 原子指令，swap复制
  __sync_lock_release(&lk->locked);

  
  pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  // 开始的中断状态
  int old = intr_get();
  // 关闭中断
  intr_off();
  // 当前CPU第一次想要获取锁时，保存此前的中断状态
  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  // 锁嵌套级别+1(进程内可以获得多个锁如锁1、锁2，只有全部都release后才能开中断)
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  // 当前中断必须是关闭状态
  if(intr_get())
    panic("pop_off - interruptible");
  // 嵌套级别错误
  if(c->noff < 1)
    panic("pop_off");
  // 锁嵌套级别-1
  c->noff -= 1;
  // 到达最外层退出区并且最外层临界区保存的中断状态是开启，则恢复中断开启
  if(c->noff == 0 && c->intena)
    intr_on();
}
