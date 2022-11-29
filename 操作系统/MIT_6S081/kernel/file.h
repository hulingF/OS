struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  char readable; // 可读权限
  char writable; // 可写权限
  struct pipe *pipe; // FD_PIPE 指向的管道
  struct inode *ip;  // FD_INODE and FD_DEVICE 指向的Inode
  uint off;          // FD_INODE 偏移量
  short major;       // FD_DEVICE 主设备号
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// in-memory copy of an inode
struct inode {
  uint dev;           // Device number 设备号
  uint inum;          // Inode number Inode编号
  // 指向 inode 的指针的数量，如果引用数量减少到零，icacahe就会考虑把它重新分配
  int ref;            // Reference count
  // 保证了可以独占访问 inode 的其他字段（如文件长度）以及 inode的文件或目录内容块
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};

// map major device number to device functions.
// 通过设备号映射设备读写函数
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
