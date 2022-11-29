#define NPROC        64  // maximum number of processes 最大进程数
#define NCPU          8  // maximum number of CPUs 最大CPU数
#define NOFILE       16  // open files per process 进程打开文件最大数
#define NFILE       100  // open files per system  系统打开文件最大数
#define NINODE       50  // maximum number of active i-nodes 活跃的最大Inode数
#define NDEV         10  // maximum major device number 最大主设备号
#define ROOTDEV       1  // device number of file system root disk 文件系统的ROOT盘的设备号
#define MAXARG       32  // max exec arguments exec系统调用的最大参数个数
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log 磁盘的日志块最大数量
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache 磁盘块缓存的数量
#define FSSIZE       1000  // size of file system in blocks 文件系统的块数
#define MAXPATH      128   // maximum file path name 文件的路径名最大长度
