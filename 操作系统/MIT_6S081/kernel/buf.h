struct buf {
  int valid;   // has data been read from disk? 是否包含该块的副本（是否从磁盘读取了数据）
  int disk;    // does disk "own" buf? 缓冲区的内容已经被修改需要被重新写入磁盘
  uint dev;    // 设备号
  uint blockno;// 块号
  struct sleeplock lock; // 保证进程同步访问buf
  uint refcnt; // 引用计数
  struct buf *prev; // LRU cache list
  struct buf *next; // LRU cache list
  uchar data[BSIZE];
};

