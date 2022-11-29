#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  // 日志头占一块大小
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  // 崩溃恢复
  recover_from_log();
}

// Copy committed blocks from log to their home location
// 日志块中的数据写入磁盘相应正确的位置
static void
install_trans(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    // log_write()时曾经调用bpin(buf)保证缓冲区不会被重用，现在使用完可以释放了
    bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
// 内存中的日志头写入磁盘，真正的事务提交点
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}


// 它读取日志头，如果日志头显示日志中包含一个已提交的事务，则会像 end_op 那样执行日志。
static void
recover_from_log(void)
{
  read_head();
  install_trans(); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    // 会一直等到日志系统没有 commiting
    if(log.committing){
      sleep(&log, &log.lock);
    // 并且有足够的日志空间来容纳这次调用的写
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      // log.outstanding 统计当前系统调用的数量 ，可以通过log.outstanding 乘以 MAXOPBLOCKS 来计算已使用的日志空间。
      // 自增log.outstanding 既预留空间，又能防止该系统调用期间进行提交。该代码假设每次系统调用最多写入MAXOPBLOCKS个块
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  // 文件系统调用结束，因此log.outstanding--
  log.outstanding -= 1;
  // 此时不可能是事务提交状态
  if(log.committing)
    panic("log.committing");
  // 最后一个文件系统调用，允许事务提交
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    // 此时事务还不能提交，尝试唤醒其他休眠的事务(因为预留空间不够而休眠)
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    // 提交事务，完成文件系统调用的整体原子操作
    commit();
    // 重置log.committing=0
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    // write_log()将事务中修改的每个块从 buffer 缓存中复制到磁盘上的日志槽中。
    write_log();     // Write modified blocks from cache to log
    // write_head()将 header块写到磁盘上就表明已提交，为提交点，写完日志后的崩溃，会导致在重启后重新执行日志。
    write_head();    // Write header to disk -- the real commit
    // install_trans从日志中读取每个块，并将其写到文件系统中对应的位置。
    install_trans(); // Now install writes to home locations
    // 最后修改日志块计数为 0，并写入日志空间的 header部分。这必须在下一个事务开始之前修改，这样崩溃就不会导致重启后的恢复使用这次的 header 和下次的日志块
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
// 它将扇区号记录在内存中，在磁盘上的日志中使用一个槽，并自增 buffer.refcnt 防止该 buffer 被重用。
// 在提交之前，块必须留在缓存中，即该缓存的副本是修改的唯一记录；在提交之后才能将其写入磁盘上的位置；
// 该次修改必须对其他读可见
void
log_write(struct buf *b)
{
  int i;

  // 本次事务写入数据过多
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  // 活跃的文件系统调用数>=1
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    // 注意，当一个块在一个事务中被多次写入时，他们在日志中的槽是相同的。这种优化通常被称为 absorption(吸收)
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  log.lh.block[i] = b->blockno;
  // 日志增加一个新块
  if (i == log.lh.n) {  // Add new block to log?
    // 自增 buffer.refcnt 防止该 buffer 被重用
    // 在提交之前块必须留在缓存中，即该缓存的副本是修改的唯一记录；在提交之后才能将其写入磁盘上的位置
    bpin(b);
    log.lh.n++;
  }
  release(&log.lock);
}

