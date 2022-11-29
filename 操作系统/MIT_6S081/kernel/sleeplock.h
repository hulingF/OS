// Long-term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?
  // 必须使用自旋锁保护，保证并发下的正确性
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

