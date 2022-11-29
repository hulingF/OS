//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint x;

  // 计算xx的绝对值
  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  // xx的绝对值的各位数字(个位、十位、百位...)放在buf中(0位、1位、2位...)
  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  // 如果xx<0，最后需要加上一个'-'符号位
  if(sign)
    buf[i++] = '-';

  // 从后往前逆序输出完整的整数
  while(--i >= 0)
    consputc(buf[i]);
}

static void
printptr(uint64 x)
{
  int i;
  // 输出十六进制标志"0x"
  consputc('0');
  consputc('x');
  // 十六进制的输出按照4位对齐(如0x00000000003fffff)
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s
// printf格式化输出字符，仅限于处理 %d十进制整型 %x十六进制整型 %p指针类型 %s字符串类型
void
printf(char *fmt, ...)
{
  va_list ap;
  int i, c, locking;
  char *s;
  // 开启锁定标志，每次调用printf都需要串行化等待获得pr.lock
  locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  // fmt内容不能为空
  if (fmt == 0)
    panic("null fmt");

  va_start(ap, fmt);
  // 开始逐个解析字符并打印到控制台
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    // 一般的字符直接输出即可
    if(c != '%'){
      consputc(c);
      continue;
    }
    // 遇到'%'需要判断输出格式，依据是"%"的后一个字符
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    // 10进制整数
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    // 16进制整数
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    // 指针类型
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    // 字符串类型
    case 's':
      // 如果字符串为空，则默认输出"(null)"
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    // 如果想要输出%需要两个连续的%
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      // 位置格式的按照原样输出，例如"%f"
      consputc('%');
      consputc(c);
      break;
    }
  }

  // 输出完后释放自旋锁
  if(locking)
    release(&pr.lock);
}

void
panic(char *s)
{
  pr.locking = 0;
  printf("panic: ");
  printf(s);
  printf("\n");
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
    ;
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}


// 当前函数的return address位于fp-8的位置，previous frame pointer位于fp-16的位置。
// 每个kernel stack都是一页，因此可以通过计算PGROUNDDOWN(fp)和PGROUNDUP(fp)来确定当前的fp地址是否还位于这一页内，从而可以是否已经完成了所有嵌套的函数调用的backtrace。
void
backtrace(void)
{
  uint64 fp, ra, top, bottom;
  printf("backtrace:\n");
  fp = r_fp();  // frame pointer of currently executing function
  top = PGROUNDUP(fp);
  bottom = PGROUNDDOWN(fp);
  while (fp < top && fp > bottom)
  {
    ra = *(uint64 *)(fp-8);
    printf("%p\n", ra);
    fp = *(uint64 *)(fp-16); // saved previous frame pointer
  }
}
