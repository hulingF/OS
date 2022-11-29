// On-disk file system format.
// Both the kernel and user programs use this header file.
// 磁盘上的文件系统的格式和布局

#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC 魔数
  uint size;         // Size of file system image (blocks) 文件系统影像大小（块数）
  uint nblocks;      // Number of data blocks data块数
  uint ninodes;      // Number of inodes inode数
  uint nlog;         // Number of log blocks log块数
  uint logstart;     // Block number of first log block log起始块号
  uint inodestart;   // Block number of first inode block inode起始块号
  uint bmapstart;    // Block number of first free map block bmap起始块号
};

#define FSMAGIC 0x10203040

// 一个文件关联的直接块数(12)
#define NDIRECT 12
// 一个文件关联的间接块数(1024/4=256)
#define NINDIRECT (BSIZE / sizeof(uint))
// 一个文件最大拥有的数据块数(12+256=268)
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type 文件类型：空闲/目录/文件/设备
  short major;          // Major device number (T_DEVICE only) 
  short minor;          // Minor device number (T_DEVICE only) 
  // 引用这个dinode的目录项数量，当nlink为0且inode的引用数也为0时就释放磁盘上的dinode及其数据块
  short nlink;          // Number of links to inode in file system 
  uint size;            // Size of file (bytes) 文件大小
  // addrs 数组记录了持有文件内容的磁盘块的块号
  uint addrs[NDIRECT+1];   // Data block addresses 
};

// Inodes per block. 一个块的inode数量
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i 编号为i的inode所在的块号
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block 每块的bmap比特数
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b 块b所在的bmap块号
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
// 目录名称的最大长度
#define DIRSIZ 14

// directory entry 目录项（inode编号+对应的文件名）
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

