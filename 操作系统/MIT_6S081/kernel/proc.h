// Saved registers for kernel context switches.
// 内核上下文切换时需要保存相关的寄存器
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  // 当前运行在本CPU上的进程
  struct proc *proc;          // The process running on this cpu, or null.
  // CPU的调度器上下文
  struct context context;     // swtch() here to enter scheduler().
  // 嵌套加锁深度
  int noff;                   // Depth of push_off() nesting.
  // 初始加锁之前的中断开启状态
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// the sscratch register points here.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
// 在处理trap时用户页表中存放进程数据的页（紧随trampoline页），由sscrach寄存器指向该页
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table 内核页表
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack 进程内核栈顶
  /*  16 */ uint64 kernel_trap;   // usertrap() 内核处理trap的入口
  /*  24 */ uint64 epc;           // saved user program counter 保存上一次执行位置
  /*  32 */ uint64 kernel_hartid; // saved kernel tp 内核tp寄存器的值(CPU的ID)
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

// 进程状态有5种：未使用、休眠、可运行、运行中、僵死
enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state 进程状态
  struct proc *parent;         // Parent process 父进程
  void *chan;                  // If non-zero, sleeping on chan 休眠链
  int killed;                  // If non-zero, have been killed 进程杀死标志
  int xstate;                  // Exit status to be returned to parent's wait 返回给父进程wait的退出状态
  int pid;                     // Process ID 进程ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack 内核栈虚拟地址
  uint64 sz;                   // Size of process memory (bytes) 进程占用内存大小
  pagetable_t pagetable;       // User page table 页表
  struct trapframe *trapframe; // data page for trampoline.S trapframe页
  struct context context;      // swtch() here to run process 进程上下文
  struct file *ofile[NOFILE];  // Open files 打开的文件
  struct inode *cwd;           // Current directory 当前工作目录
  char name[16];               // Process name (debugging) 进程名称


  // A kernel page table per process实验
  pagetable_t kernelpt;        // per process kernel pagetable
  // Alarm实验
  int interval;                // interval of alarm
  void (*handler)();           // pointer to the handler function
  int ticks;                   // how many ticks have passed since last call
  int in_handler;              // to prevent from reentering into handler when in handler
  // 下面的所有寄存器都是用来保护和恢复现场的。
  uint64 saved_epc;
  uint64 saved_ra;
  uint64 saved_sp;
  uint64 saved_gp;
  uint64 saved_tp;
  uint64 saved_t0;
  uint64 saved_t1; 
  uint64 saved_t2;
  uint64 saved_s0;
  uint64 saved_s1;
  uint64 saved_s2;
  uint64 saved_s3;
  uint64 saved_s4;
  uint64 saved_s5;
  uint64 saved_s6;
  uint64 saved_s7;
  uint64 saved_s8;
  uint64 saved_s9;
  uint64 saved_s10;
  uint64 saved_s11;
  uint64 saved_a0;
  uint64 saved_a1;
  uint64 saved_a2;
  uint64 saved_a3;
  uint64 saved_a4;
  uint64 saved_a5;
  uint64 saved_a6;
  uint64 saved_a7;
  uint64 saved_t3;
  uint64 saved_t4;
  uint64 saved_t5;
  uint64 saved_t6;

};
