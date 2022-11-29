// Mutual exclusion lock.
// 自旋锁
struct spinlock {
  // 锁是否被持有，0表示锁空闲，1表示锁已占用
  uint locked;       // Is the lock held?】
  // For debugging:
  // 锁的名字
  char *name;        // Name of lock.
  // 持有锁的CPU
  struct cpu *cpu;   // The cpu holding the lock.
};

