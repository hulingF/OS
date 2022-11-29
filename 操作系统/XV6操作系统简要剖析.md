## 第一部分：XV6操作系统简要剖析

> 学习目标：通过学习MIT的一套教学操作系统深度了解操作系统的内部简易实现机制，从代码层次更多把握OS的内部机制，可以作为操作系统的实践部分进行阅读学习！注意：从我个人的角度出发，我不会细抠内部的每一行代码，我想达到的效果是能建立一个OS的全局观，打破这个"黑盒子"，但不是制作这个"盒子"，更多还是从programmer而不是自始而终的builder出发看待问题！

### 第一章：操作系统接口

`操作系统的作用：`

- 在多个程序之间共享一台计算机，并提供一套比硬件单独支持更有用的服务，操作系统管理和抽象低级硬件，因此，例如，文字处理程序不需要关心使用的何种磁盘硬件。
- 操作系统在多个程序之间共享硬件，使它们能同时运行（或看起来是同时运行）。

- 最后，操作系统为程序提供了可控的交互方式，使它们能够共享数据或共同工作。

`操作系统如何提供上层服务：`

操作系统通过接口为用户程序提供服务。设计一个好的接口很困难的。一方面，我们希望接口是简单而单一的，因为这样更容易得到正确的实现。另一方面，我们可能会想为应用程序提供许多复杂的功能。解决这种矛盾的诀窍是设计出依靠一些机制的接口，这些机制可以通过组合提高通用性（如管道）。操作系统 xv6 提供了 Ken Thompson 和 Dennis Ritchie 的 Unix 操作系统所介绍的基本接口，同时也模仿了 Unix 的内部设计。

> Unix 提供了一个单一的接口，其机制结合得很好，提供了惊人的通用性。这种接口非常成功，以至于现代操作系统 BSD、Linux、Mac OS X、Solaris，甚至微软Windows都有类似Unix 的接口。理解xv6是理解这些系统和许多其它系统的一个良好开端。

`xv6的简单示意图：`

![image-20221114094042087](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114094042087.png)

xv6采用了传统的**内核**形式，==内核是一个特殊程序，可以为其他运行进程提供服务==。每个正在运行的程序，称为进程，拥有自己的内存，其中包含指令、数据和堆栈。指令实现了程序的计算。数据是计算操作对象。栈允许了函数调用。一台计算机通常有许多进程，但只有一个内核。==当一个进程需要调用一个内核服务时，它就会调用**系统调用**，这是操作系统接口中的一个调用==。系统调用进入内核，内核执行服务并返回。因此，一个进程在用户空间和内核空间中交替执行。

> 内核使用 CPU 提供的硬件保护机制来确保在用户空间中执行的每个进程只能访问其自己的内存。内核运行时拥有硬件特权，可以访问这些受到保护的资源;用户程序执行时没有这些特权。当用户程序调用系统调用(接口)时，硬件提高特权级别并开始执行内核中预先安排的函数。

`内核提供的系统调用集合是用户程序看到的接口`。xv6 内核提供了传统 Unix 内核所提供的服务和系统调用的一个子集。图 1.2 列出了 xv6 的所有系统调用。

![image-20221114094612346](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114094612346.png)

#### 1.1进程与内存

一个 xv6 进程由`用户空间内存`（指令、数据和堆栈）和`内核私有的进程状态`组成。xv6的进程共享 cpu，它透明地切换当前 cpu 正在执行的进程。当一个进程暂时不使用 cpu 时，xv6 会保存它的 CPU 寄存器，在下次运行该进程时恢复它们。内核为每个进程关联一个**PID**(进程标识符)。

我们可以使用 **fork** 系统调用创建一个新的进程。**Fork** 创建的新进程，称为子进程，`其内存内容与调用的进程完全相同`，原进程被称为父进程。在父进程和子进程中，fork 都会返回。`在父进程中，fork 返回子进程的 PID；在子进程中，fork 返回 0。`

```c
int pid = fork();
if (pid > 0)
{
printf("parent: child=%d\n", pid);
pid = wait((int *)0);
printf("child %d is done\n", pid);
}
else if (pid == 0)
{
printf("child: exiting\n");
exit(0);
}
else
{
printf("fork error\n");
}
```

`**exit** 系统调用退出调用进程，并释放资源，如内存和打开的文件`。**exit** 需要一个整数状态参数，通常 0 表示成功，1 表示失败。**wait** 系统调用返回当前进程的一个已退出（或被杀死）的子进程的 PID，并将该子进程的退出状态码复制到一个地址，该地址由 wait 参数提供；如果调用者的子进程都没有退出，则 **wait** 等待一个子进程退出。如果调用者没有子进程，wait 立即返回-1。如果父进程不关心子进程的退出状态，可以传递一个 0 地址给 wait。

```c
parent: child=3884
child: exiting
child 3884 is done
```

第一行和第二行可能会以任何一种顺序输出，这取决于是父进程还是子进程先执行它的 **printf** 调用。在子程序退出后，父进程的 **wait** 返回，父进程执行 **printf**。

> 虽然子进程最初与父进程拥有相同的内存内容，但父进程和子进程是在不同的内存和不同的寄存器中执行的：改变其中一个进程中的变量不会影响另一个进程。例如，当 **wait** 的返回值存储到父进程的 **pid** 变量中时，并不会改变子进程中的变量 **pid**。子进程中的 **pid** 值仍然为零。

**exec** 系统调用使用新内存映像来替换进程的内存， 新内存映像从文件系统中的文件中进行读取。这个文件必须有特定的格式，它指定了文件中哪部分存放指令，哪部分是数据，在哪条指令开始等等，xv6 使用 ELF 格式。当 **exec** 成功时，它并不返回到调用程序；相反，从文件中加载的指令在 ELF 头声明的入口点开始执行。**exec** 需要两个参数：包含可执行文件的文件名和一个字符串参数数组。

```c
char *argv[3];
argv[0] = "echo";
argv[1] = "hello";
argv[2] = 0;
exec("/bin/echo", argv);
printf("exec error\n");
```

说完了fork和exit系统调用，xv6 shell 正是使用上述调用来在用户空间运行程序。shell 的主结构很简单，主循环用 **getcmd** 读取用户的一行输入，然后调用 **fork**，创建 shell 副本。父进程调用 wait，而子进程则运行命令。例如，如果用户向 shell 输入了 **echo hello**，那么就会调用 **runcmd**，参数为 **echo hello**。**runcmd**  运行实际的命令。对于**echo hello**，它会调用 **exec** 。如果 **exec** 成功，那么子进程将执行 echo 程序的指令，而不是 **runcmd** 的。在某些时候，**echo** 会调用 **exit**，这将使父程序从 main中的 **wait** 返回。

```c
// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);
  // 根据cmd类型选择不同的执行策略
  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    // 首先强制转换为execcmd类型
    ecmd = (struct execcmd*)cmd;
    // 可执行文件名（argv[0]）不能为空
    if(ecmd->argv[0] == 0)
      exit(1);
    // 执行exec系统调用，不会返回
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    // 首先强制转换为listcmd类型
    lcmd = (struct listcmd*)cmd;
    // fork子进程优先执行lcmd->left
    if(fork1() == 0)
      runcmd(lcmd->left);
    // 等待子进程执行完lcmd->left后父进程接着执行lcmd->right
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    // 首先强制转换为pipecmd
    pcmd = (struct pipecmd*)cmd;
    // 调用pipe系统调用
    if(pipe(p) < 0)
      panic("pipe");
    // 子进程1标准输出重定向为p[1]即管道文件写端
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    // 子进程2标准输入重定向为p[0]即管道文件读端
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    // 父进程关闭管道文件描述符并且等待两个子进程的退出
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  // 首先打印shell的“$ ”提示符
  fprintf(2, "$ ");
  // 清空缓冲区
  memset(buf, 0, nbuf);
  // 读取字符到字符缓冲区buf中
  gets(buf, nbuf);
  // 读取到结束位置EOF
  if(buf[0] == 0) 
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  // 确保有三个文件描述符0、1、2
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  // 读取并解析允许命令
  while(getcmd(buf, sizeof(buf)) >= 0){
    // cd命令比较特殊，由shell进程直接执行
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // 替换末尾的\n形成一个C风格字符串
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    // 子进程执行cmd命令
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    // shell进程等待子进程结束
    wait(0);
  }
  exit(0);
}
```

> 你可能会奇怪为什么 **fork** 和 **exec** 没有结合在一次调用中，我们后面会看到 shell 在实现 I/O 重定向时利用了这种分离的特性。为了避免创建相同进程并立即替换它（使用 exec）所带来的浪费，内核通过使用虚拟内存技术（如 copy-on-write）来优化这种用例的 fork 实现。

#### 1.2I/O和文件描述符

`文件描述符是一个小整数，代表一个可由进程读取或写入的内核管理对象。`一个进程可以通过打开一个文件、目录、设备，或者通过创建一个管道，或者通过复制一个现有的描述符来获得一个文件描述符。为了简单起见，我们通常将文件描述符所指向的对象称为文件；`文件描述符接口将文件、管道和设备之间的差异抽象化，使它们看起来都像字节流。我们把输入和输出称为 I/O。`

`在内部，xv6 内核为每一个进程单独维护一个以文件描述符为索引的表，因此每个进程都有一个从 0 开始的文件描述符私有空间。按照约定，一个进程从文件描述符 0(标准输入)读取数据，向文件描述符 1(标准输出)写入输出，向文件描述符 2(标准错误)写入错误信息。`正如我们将看到的那样，shell 利用这个约定来实现 I/O 重定向和管道。shell 确保自己总是有三个文件描述符打开，这些文件描述符默认是控制台的文件描述符。

read/write 系统调用可以从文件描述符指向的文件读写数据。调用 read(fd, buf, n)从文件描述符 fd 中读取不超过 n 个字节的数据，将它们复制到 buf 中，并返回读取的字节数。每个引用文件的文件描述符都有一个与之相关的偏移量。读取从当前文件偏移量中读取数据，然后按读取的字节数推进偏移量，随后的读取将返回上次读取之后的数据。当没有更多的字节可读时，读返回零，表示文件的结束

write(fd, buf, n)表示将 buf 中的 n 个字节写入文件描述符 fd 中，并返回写入的字节数。若写入字节数小于 n 则该次写入发生错误。和 read 一样，write 在当前文件偏移量处写入数据，然后按写入的字节数将偏移量向前推进：每次写入都从上一次写入的地方开始。

下面的程序片段(程序 cat 的核心代码)将数据从其标准输入复制到其标准输出。如果出现错误，它会向标准错误写入一条消息。

```c
char buf[512];
int n;
for (;;)
{
    n = read(0, buf, sizeof buf);
    if (n == 0)
    break;
    if (n < 0)
    {
        fprintf(2, "read error\n");
        exit(1);
    }
    if (write(1, buf, n) != n)
    {
        fprintf(2, "write error\n");
        exit(1);
    }
}
```

> 在这个代码片段中，需要注意的是，**cat** 不知道它是从文件、控制台还是管道中读取的。同样，**cat** 也不知道它是在打印到控制台、文件还是其他什么地方。文件描述符的使用和 0代表输入，1 代表输出的约定，使得 **cat** 可以很容易实现。

close 系统调用会释放一个文件描述符，使它可以被以后的 open、pipe 或 dup 系统调用所重用（见下文）。`新分配的文件描述符总是当前进程中最小的未使用描述符。`

`文件描述符和 fork 相互作用，使 I/O 重定向易于实现。`Fork 将父进程的文件描述符表和它的内存一起复制，这样子进程开始时打开的文件和父进程完全一样。`系统调用 exec 替换调用进程的内存，但会保留文件描述符表。这种行为允许 shell 通过 fork 实现 I/O 重定向，在子进程中重新打开所选的文件描述符，然后调用 exec 运行新程序。`下面是 shell 运行 cat < input.txt 命令的简化版代码。

```c
char *argv[2];
argv[0] = "cat";
argv[1] = 0;
if (fork() == 0)
{
    close(0); // 释放标准输入的文件描述符
    open("input.txt", O_RDONLY); // 这时 input.txt 的文件描述符为 0
    // 即标准输入为 input.txt
    exec("cat", argv); // cat 从 0 读取，并输出到 1，见上个代码段
}
```

在子进程关闭文件描述符 0 后，open 保证对新打开的 input.txt 使用该文件描述符 0。因为此时 0 将是最小的可用文件描述符。然后 Cat 执行时，文件描述符 0（标准输入）引用input.txt。这不会改变父进程的文件描述符，它只会修改子进程的描述符。

> xv6 shell 中的 I/O 重定向代码正是以这种方式工作的。回想一下 shell 的代码，shell 已经 fork 子 shell，runcmd 将调用 exec 来加载新的程序。

open 的第二个参数由一组用位表示的标志组成，用来控制 open 的工作。可能的值在文件控制(fcntl)头中定义。`O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREATE`, 和 `O_TRUNC`, 它们指定 open 打开文件时的功能，读，写，读和写，如果文件不存在创建文件，将文件截断为零。

> 现在应该清楚为什么 fork 和 exec 是分开调用的：在这两个调用之间，shell 有机会重定向子进程的 I/O，而不干扰父进程的 I/O 设置。我们可以假设一个由 fork 和 exec 组成的系统调用 forkexec，但是用这种调用来做 I/O 重定向似乎很笨拙：shell 在调用 forkexec 之前修改自己的 I/O 设置（然后取消这些修改），或者 forkexec 可以将 I/O 重定向的指令作为参数，或者（最糟糕的方案）每个程序（比如 cat）都需要自己做 I/O 重定向。

虽然 fork 复制了文件描述符表，但每个底层文件的偏移量都是父子共享的。想一想下面的代码。

```c
if (fork() == 0)
{
    write(1, "hello ", 6);
    exit(0);
}
else
{
    wait(0);
    write(1, "world\n", 6);
}
```

在这个片段的最后，文件描述符 1 所引用的文件将包含数据 hello world。父文件中的write（由于有了 wait，只有在子文件完成后才会运行）会从子文件的 write 结束的地方开始。这种行为有助于从 shell 命令的序列中产生有序的输出，比如(echo hello; echo world) >output.txt。

`dup 系统调用复制一个现有的文件描述符，返回一个新的描述符，它指向同一个底层I/O 对象。两个文件描述符共享一个偏移量，就像被 fork 复制的文件描述符一样。`这是将hello world 写进文件的另一种方法。

```c
fd = dup(1);
write(1, "hello ", 6);
write(fd, "world\n", 6);
```

如果两个文件描述符是通过一系列的 fork 和 dup 调用从同一个原始文件描述符衍生出来的，那么这两个文件描述符共享一个偏移量。否则，文件描述符不共享偏移量，即使它们是由同一个文件的打开调用产生的。Dup 允许 shell 实现这样的命令：ls existing-file non-existing-file > tmp1 2>&1。2>&1 表示 2 是 1 的复制品（dup(1)），即重定向错误信息到标准输出，已存在文件的名称和不存在文件的错误信息都会显示在文件 tmp1 中。xv6 shell不支持错误文件描述符的 I/O 重定向，但现在你知道如何实现它了。

> 文件描述符是一个强大的抽象，因为它们隐藏了它们连接的细节：一个向文件描述符 写入的进程可能是在向一个文件、控制台等设备或向一个管道写入。

#### 1.3管道

`管道是一个小的内核缓冲区，作为一对文件描述符暴露给进程，一个用于读，一个用于写。`将数据写入管道的一端就可以从管道的另一端读取数据。管道为进程提供了一种通信方式。下面的示例代码运行程序 wc，标准输入连接到管道的读取端。

```c
int p[2];
char *argv[2];
argv[0] = "wc";
argv[1] = 0;
pipe(p);
if (fork() == 0)
{
    close(0); // 释放文件描述符 0
    dup(p[0]); // 复制一个 p[0](管道读端)，此时文件描述符 0（标准输入）也引用管道读端，故改变了标准输入。
    close(p[0]);
    close(p[1]);
    exec("/bin/wc", argv); // wc 从标准输入读取数据，并写入到参数中的每一个文件
}
else
{
    close(p[0]);
    write(p[1], "hello world\n", 12);
    close(p[1]);
}
```

程序调用 pipe，创建一个新的管道，并将读写文件描述符记录在数组 p 中，经过 fork后，父进程和子进程的文件描述符都指向管道。子进程调用 close 和 dup 使文件描述符 0 引用管道的读端，并关闭 p 中的文件描述符，并调用 exec 运行 wc。当 wc 从其标准输入端读取时，它将从管道中读取。父进程关闭管道的读端，向管道写入，然后关闭写端。

> 如果没有数据可用，管道上的 read 会等待数据被写入，或者等待所有指向写端的文件描述符被关闭；在后一种情况下，读将返回 0，就像数据文件的结束一样。事实上，如果没有数据写入，读会无限阻塞，直到新数据不可能到达为止（写端被关闭），这也是子进程在执行上面的 wc 之前关闭管道的写端很重要的一个原因：如果 wc 的一个文件描述符仍然引用了管道的写端，那么 wc 将永远看不到文件的关闭（被自己阻塞）。

xv6 的 shell 实现了管道，如 **grep fork sh.c | wc -l**，shell 的实现类似于上面的代码。执行 shell 的子进程创建一个管道来连接管道的左端和右端（去看源码，不看难懂）。然后，它在管道左端（写入端）调用 **fork** 和 **runcmd**，在右端（读取端）调用fork 和 runcmd，并等待两者的完成。管道的右端（读取端）可以是一个命令，也可以是包含管道的多个命令（例如，**a | b | c**），它又会分叉为两个新的子进程（一个是 **b**，一个是 **c**）。`因此，shell 可以创建一棵进程树。这棵树的叶子是命令，内部（非叶子）节点是等待左右子进程完成的进程。`

管道似乎没有比临时文件拥有更多的功能：

```shell
echo hello world | wc
```

不使用管道：

```shell
echo hello world >/tmp/xyz; wc </tmp/xyz
```

> 在这种情况下，管道比临时文件至少有四个优势。首先，管道会自动清理自己；如果是文件重定向，shell 在完成后必须小心翼翼地删除/tmp/xyz。第二，管道可以传递任意长的数据流，而文件重定向则需要磁盘上有足够的空闲空间来存储所有数据。第三，管道可以分阶段的并行执行，而文件方式则需要在第二个程序开始之前完成第一个程序。第四，如果你要实现进程间的通信，管道阻塞读写比文件的非阻塞语义更有效率。

#### 1.4文件系统

`xv6 文件系统包含了数据文件（拥有字节数组）和目录（拥有对数据文件和其他目录的命名引用）。这些目录形成一棵树，从一个被称为根目录的特殊目录开始。`像**/a/b/c** 这样的路径指的是根目录**/**中的 **a** 目录中的 **b** 目录中的名为 **c** 的文件或目录。不以**/**开头的路径是相对于调用进程的当前目录进行计算其绝对位置的，可以通过 **chdir** 系统调用来改变进程的当前目录。下面两个 **open** 打开了同一个文件（假设所有涉及的目录都存在）。

```c
chdir("/a");
chdir("b");
open("c", O_RDONLY);
open("/a/b/c", O_RDONLY);
```

有一些系统调用来可以创建新的文件和目录：**mkdir** 创建一个新的目录，用 **O_CREATE**标志创建并打开一个新的数据文件，以及 **mknod** 创建一个新的设备文件。这个例子说明了这两个系统调用的使用。

```c
mkdir("/dir");
fd = open("/dir/file", O_CREATE | O_WRONLY);
close(fd);
mknod("/console", 1, 1);
```

> **mknod** 创建了一个引用设备的特殊文件。与设备文件相关联的是主要设备号和次要设备号(**mknod** 的两个参数)，它们唯一地标识一个内核设备。当一个进程打开设备文件后，内核会将系统的读写调用转移到内核设备实现上，而不是将它们传递给文件系统。

文件名称与文件是不同的；底层文件（非磁盘上的文件）被称为 inode，一个 inode 可以有多个名称，称为链接。每个链接由目录中的一个项组成；该项包含一个文件名和对 inode的引用。inode 保存着一个文件的 **metadata**（元数据），包括它的类型（文件或目录或设备），它的长度，文件内容在磁盘上的位置，以及文件的链接数量。

**fstat** 系统调用从文件描述符引用的 inode 中检索信息。它定义在 **stat.h**的 **stat** 结构中：

```c
#define T_DIR 1 // Directory
#define T_FILE 2 // File
#define T_DEVICE 3 // Device
struct stat
{
int dev; // File system’s disk device
uint ino; // Inode number
short type; // Type of file
short nlink; // Number of links to file
uint64 size; // Size of file in bytes
};
```

**link** 系统调用创建了一个引用了同一个 inode 的文件（文件名）。下面的片段创建了引用了同一个 inode 两个文件 a 和 b。

```c
open("a", O_CREATE | O_WRONLY);
link("a", "b");
```

> 读写 a 与读写 b 是一样的，每个 inode 都有一个唯一的 inode 号来标识。经过上面的代码序列后，可以通过检查 fstat 的结果来确定 a 和 b 指的是同一个底层内容：两者将返回相同的 inode 号（**ino**），并且 nlink 计数为 2。
>
> Inode 是 linux 和类 unix 操作系统用来储存除了文件名和实际数据的数据结构，它是用来连接实际数据和文件名的。

**unlink** 系统调用会从文件系统中删除一个文件名。`只有当文件的链接数为零且没有文件描述符引用它时，文件的 inode 和存放其内容的磁盘空间才会被释放。`

```c
unlink("a");
```

上面这行代码会删除 a，此时只有 b 会引用 inode。

```c
fd = open("/tmp/xyz", O_CREATE | O_RDWR);
unlink("/tmp/xyz");
```

这段代码是创建一个临时文件的一种惯用方式，它创建了一个无名称 inode，故会在进程关闭 **fd** 或者退出时删除文件。

Unix 提供了 shell 可调用的文件操作程序，作为用户级程序，例如 **mkdir**、**ln** 和 **rm**。这种设计允许任何人通过添加新的用户级程序来扩展命令行接口。现在看来，这个设计似乎是显而易见的，但在 Unix 时期设计的其他系统通常将这类命令内置到 shell 中（并将 shell 内置到内核中）。

> 本文研究的是 xv6 如何实现其类似 Unix 的接口，但其思想和概念不仅仅适用于 Unix。任何操作系统都必须将进程复用到底层硬件上，将进程相互隔离，并提供受控进程间通信的机制。在学习了 xv6 之后，您应该能够研究其他更复杂的操作系统，并在这些系统中看到 xv6的基本概念。

### 第二章：操作系统组织

操作系统的一个关键要求是同时支持几个活动。例如，使用第 1 章中描述的系统调用接口，一个进程可以用 **fork** 创建新进程。操作系统必须在这些进程之间分时共享计算机的资源。例如，即使进程的数量多于硬件 CPU 的数量，操作系统也必须保证所有的进程都有机会执行。操作系统还必须安排进程之间的隔离。也就是说，如果一个进程出现了 bug 并发生了故障，不应该影响不依赖 bug 进程的进程。然而，隔离性太强了也不可取，因为进程间可能需要进行交互，例如管道。因此，一个操作系统必须满足三个要求：`多路复用`、`隔离`和`交互`。

==Xv6 运行在多核RISC-V 微处理器上，它的许多底层功能（例如，它的进程实现）是 RISC-V 所特有的。RISC-V 是一个 64 位的 CPU，xv6 是用 "LP64 "C 语言编写的，这意味着 C 编程语言中的 long(L)和指针(P)是 64 位的，但 int 是 32 位的。==

一台完整的计算机中的 CPU 周围都是硬件，其中大部分是 I/O 接口的形式。Xv6 编写的代码是基于通过"-machine virt "选项的 qemu。这包括 RAM、包含启动代码的 ROM、与用户键盘/屏幕的串行连接以及用于存储的磁盘。

#### 2.1抽象物理资源

遇到一个操作系统，人们可能会问的第一个问题是为什么需要它呢？答案是，我们可以把图 1.2 中的系统调用作为一个库来实现，应用程序与之连接。在这个想法中，每个应用程序可以根据自己的需要定制自己的库。应用程序可以直接与硬件资源进行交互，并以最适合应用程序的方式使用这些资源（例如，实现高性能）。一些用于嵌入式设备或实时系统的操作系统就是以这种方式组织的。

`这种系统库方式的缺点是，如果有多个应用程序在运行，这些应用程序必须正确执行。`例如，每个应用程序必须定期放弃 CPU，以便其他应用程序能够运行。如果所有的应用程序都相互信任并且没有 bug，这样的 **cooperative** 分时方案可能是 OK 的。`更典型的情况是，应用程序之间互不信任，并且有 bug，所以人们通常希望比 **cooperative** 方案提供更强的隔离性`

`为了实现强隔离，禁止应用程序直接访问敏感的硬件资源，而将资源抽象为服务是很有帮助的。`例如，Unix 应用程序只通过文件系统的 **open**、**read**、**write** 和 **close** 系统调用与文件系统进行交互，而不是直接读写磁盘。这为应用程序带来了路径名的便利，而且它允许操作系统（作为接口的实现者）管理磁盘。即使不考虑隔离问题，那些有意交互的程序（或者只是希望互不干扰）很可能会发现文件系统是一个比直接使用磁盘更方便的抽象。

`同样，Unix 在进程之间透明地切换硬件 CPU，必要时保存和恢复寄存器状态，这样应用程序就不必意识到时间共享。这种透明性允许操作系统共享 CPU，即使一些应用程序处于无限循环中。`

另一个例子是，Unix 进程使用 exec 来建立它们的内存映像，而不是直接与物理内存交互。这使得操作系统可以决定将进程放在内存的什么位置；如果内存紧张，操作系统甚至可能将进程的部分数据存储在磁盘上。Exec 还允许用户将可执行文件储存在文件系统中。

Unix 进程之间的许多形式的交互都是通过文件描述符进行的。文件描述符不仅可以抽象出许多细节（例如，管道或文件中的数据存储在哪里），而且它们的定义方式也可以简化交互。例如，如果管道中的一个应用程序崩溃了，内核就会为管道中的另一个进程产生一个文件结束信号。

> 图 1.2 中的系统调用接口经过精心设计，既为程序员提供了便利，又提供了强隔离的可能。Unix 接口并不是抽象资源的唯一方式，但事实证明它是一种非常好的方式。

#### 2.2模式变换与系统调用

强隔离要求应用程序和操作系统之间有一个分界线。如果应用程序发生错误，我们不希望操作系统崩溃，也不希望其他应用程序崩溃。相反，操作系统应该能够清理崩溃的应用程序并继续运行其他应用程序。为了实现强隔离，操作系统必须安排应用程序不能修改（甚至不能读取）操作系统的数据结构和指令，应用程序不能访问其他进程的内存。

CPU 提供了强隔离的硬件支持。例如，RISC-V 有三种模式，CPU 可以执行指令：（machine）机器模式、监督者（supervisor）模式和（user）用户模式。在机器模式下执行的指令具有完全的权限，一个 CPU 在机器模式下启动。机器模式主要用于配置计算机。Xv6 会在机器模式下执行几条指令，然后转为监督者模式。

`在监督者（supervisor）模式下，CPU 被允许执行特权指令：例如，启用和禁用中断，读写保存页表地址的寄存器等。如果用户模式下的应用程序试图执行一条特权指令，CPU 不会执行该指令，而是切换到监督者模式，这样监督者模式的代码就可以终止应用程序，因为它做了不该做的事情。`第 1 章的图 1.1 说明了这种组织方式。一个应用程序只能执行用户模式的指令（如数字相加等），被称为运行在用户空间，而处于监督者模式的软件也可以执行特权指令，被称为运行在内核空间。运行在内核空间（或监督者模式）的软件称为内核。

`一个应用程序如果要调用内核函数（如 xv6 中的读系统调用），必须过渡到内核。CPU提供了一个特殊的指令，可以将 CPU 从用户模式切换到监督模式，并在内核指定的入口处进入内核。(RISC-V 为此提供了 ecall 指令。)`一旦 CPU 切换到监督者模式，内核就可以验证系统调用的参数，决定是否允许应用程序执行请求的操作，然后拒绝或执行该操作。`内核控制监督者模式的入口点是很重要的；如果应用程序可以决定内核的入口点，那么恶意应用程序就能够进入内核，例如，通过跳过参数验证而进入内核。`

#### 2.3内核组织

==一个关键的设计问题是操作系统的哪一部分应该在监督者模式下运行。一种可能是整个操作系统驻留在内核中，这样所有系统调用的实现都在监督者模式下运行。这种组织方式称为宏内核。==

在这种组织方式中，整个操作系统以全硬件权限运行。这种组织方式很方便，因为操作系统设计者不必决定操作系统的哪一部分不需要全硬件权限。此外，操作系统的不同部分更容易合作。例如，一个操作系统可能有一个缓冲区，缓存文件系统和虚拟内存系统共享的数据。

宏内核组织方式的一个缺点是操作系统的不同部分之间的接口通常是复杂的（我们将在本文的其余部分看到），因此操作系统开发者很容易写 bug。在宏内核中，一个错误是致命的，因为监督者模式下的错误往往会导致内核崩溃。如果内核崩溃，计算机就会停止工作，因此所有的应用程序也会崩溃。计算机必须重启。

==为了降低内核出错的风险，操作系统设计者可以尽量减少在监督者模式下运行的操作系统代码量，而在用户模式下执行操作系统的大部分代码。这种内核组织方式称为微内核。==

![image-20221114141151380](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114141151380.png)

图 2.1 说明了这种微内核设计。在图中，文件系统作为一个用户级进程运行。作为进程运行的 OS 服务称为服务器。为了让应用程序与文件服务器进行交互，内核提供了一种进程间通信机制，用于从一个用户模式进程向另一个进程发送消息。例如，如果一个像 shell 这样的应用程序想要读写文件，它就会向文件服务器发送一个消息，并等待响应。

`在微内核中，内核接口由一些低级函数组成，用于启动应用程序、发送消息、访问设备硬件等。这种组织方式使得内核相对简单，因为大部分操作系统驻留在用户级服务器中。`

==xv6 和大多数 Unix 操作系统一样，是以宏内核的形式实现的。因此，xv6 内核接口与操作系统接口相对应，内核实现了完整的操作系统。由于 xv6 不提供很多服务，所以它的内核比一些微内核要小，但从概念上讲 xv6 是宏内核。==

#### 2.4xv6代码组织形式

xv6 内核源码在 **kernel/**子目录下。按照模块化的概念，源码被分成了多个文件，图 2.2列出了这些文件。模块间的接口在 **defs.h(kernel/defs.h)**中定义。

![image-20221114141413174](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114141413174.png)

2.5进程概述

xv6 中的隔离单位（和其他 Unix 操作系统一样）是一个进程。进程抽象可以防止一个进程破坏或监视另一个进程的内存、CPU、文件描述符等。它还可以防止进程破坏内核，所以进程不能破坏内核的隔离机制。内核必须小心翼翼地实现进程抽象，因为一个错误或恶意的应用程序可能会欺骗内核或硬件做一些不好的事情（例如，规避隔离）。`内核用来实现进程的机制包括：用户/监督模式标志、地址空间和线程的时间分割。`

==为了帮助实施隔离，进程抽象为程序提供了一种错觉，即它有自己的私有机器。一个进程为程序提供了一个看似私有的内存系统，或者说是地址空间，其他进程不能对其进行读写。进程还为程序提供了“私有”的 CPU，用来执行程序的指令。==

Xv6 使用`页表`（由硬件实现）给每个进程提供自己的地址空间。RISC-V 页表将`虚拟地址`(RISC-V 指令操作的地址)转换(或 "映射")为`物理地址`(CPU 芯片发送到主存储器的地址)。

![image-20221114141810611](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114141810611.png)

Xv6 为每个进程维护一个单独的页表，定义该进程的地址空间。如图 2.3 所示，进程的用户空间内存的地址空间从虚拟地址 0 开始的。指令存放在最前面，其次是全局变量，然后是栈，最后是一个堆区（用于 **malloc**），进程可以根据需要扩展。有一些因素限制了进程地址空间的最大长度：RISC-V 上的指针是 64 位宽；硬件在页表中查找虚拟地址时只使用低的39 位；xv6 只使用 39 位中的 38 位。因此，最大地址是 2^38^ -1 = 0x3fffffffff，也就是 **MAXVA**。在地址空间的顶端，xv6 保留了一页，用于 **trampoline** 和映射进程**trapframe** 的页，以便切换到内核。

xv6 内核为每个进程维护了许多状态，它将这些状态在**proc** 结构体中。一个进程最重要的内核状态是它的页表、内核栈和运行状态。我们用 **p->xxx** 来表示 proc结构的元素，例如，**p->pagetable** 是指向进程页表的指针。

每个进程都有一个执行线程（简称线程），执行进程的指令。一个线程可以被暂停，然后再恢复。为了在进程之间透明地切换，内核会暂停当前运行的线程，并恢复另一个进程的线程。线程的大部分状态（局部变量、函数调用返回地址）都存储在线程的栈中。每个进程有两个栈：用户栈和内核栈（**p->kstack**）。当进程在执行用户指令时，只有它的用户栈在使用，而它的内核栈是空的。当进程进入内核时（为了系统调用或中断），内核代码在进程的内核栈上执行；当进程在内核中时，它的用户栈仍然包含保存的数据，但不被主动使用。进程的线程在用户栈和内核栈中交替执行。内核栈是独立的（并且受到保护，不受用户代码的影响），所以即使一个进程用户栈被破坏了，内核也可以执行。

一个进程可以通过执行 RISC-V **ecall** 指令进行系统调用。该指令提高硬件权限级别，并将程序计数器改变为内核定义的入口点。入口点的代码会切换到内核栈，并执行实现系统调用的内核指令。当系统调用完成后，内核切换回用户栈，并通过调用 **sret** 指令返回用户空间，降低硬件特权级别，恢复执行系统调用前的用户指令。进程的线程可以在内核中阻塞等待 I/O，当 I/O 完成后，再从离开的地方恢复。

**p->state** 表示进程是创建、就绪、运行、等待 I/O，还是退出。

**p->pagetable** 以 RISC-V 硬件需要的格式保存进程的页表，当进程在用户空间执行时，xv6 使分页硬件使用进程的 **p->pagetable**。进程的页表也会记录分配给该进程内存的物理页地址。

#### 2.5xv6启动流程

当 RISC-V 计算机开机时，它会初始化自己，并运行一个存储在只读存储器中的 **boot loader**。**Boot loader** 将 xv6 内核加载到内存中。然后，在机器模式下，CPU 从 **_entry**开始执行 xv6。RISC-V 在禁用分页硬件的情况下启动：虚拟地址直接映射到物理地址。

loader 将 xv6 内核加载到物理地址 0x80000000 的内存中。之所以将内核放在0x80000000 而不是 0x0，是因为地址范围 0x0-0x80000000 包含 I/O 设备。

**_entry** 处 的 指 令 设 置 了 一 个 栈 ， 这 样 xv6 就 可 以 运 行 C 代 码 。Xv6 在 文 件start.c中声明了初始栈的空间，即 **stack0**。在**_entry** 处的代码加载栈指针寄存器 sp，地址为 **stack0+4096**，也就是栈的顶部，因为 RISC-V 的栈是向下扩张的。现在内核就拥有了栈，**_entry** 调用 start，并执行其 C 代码。

函数 start 执行一些只有在机器模式下才允许的配置，然后切换到监督者模式。为了进入监督者模式，RISC-V 提供了指令 **mret**。为了进入监督者模式，RISC-V 提供了指令 **mret**。这条指令最常用来从上一次的调用中返回，上一次调用从监督者模式到机器模式。**start** 并不是从这样的调用中返回，而是把事情设置得像有过这样的调用一样：它在寄存器 **mstatus**中把上一次的特权模式设置为特权者模式，它把 **main** 的地址写入寄存器 **mepc** 中，把返回地址设置为 **main 函数**的地址，在特权者模式中把 0 写入页表寄存器 **satp** 中，禁用虚拟地址转换，并把所有中断和异常委托给特权者模式。

在进入特权者模式之前，**start** 还要执行一项任务：对时钟芯片进行编程以初始化定时器中断。在完成了这些基本管理后，**start** 通过调用 **mret** "返回" 到监督者模式。这将导致程序计数器变为 main的地址。

在 **main** 初 始 化 几 个 设 备 和 子 系 统 后 ， 它 通 过 调 用**userinit**来创建第一个进程。第一个进程执行一个用 RISC-V 汇编编写的小程序 **initcode.S**，它通过调用 **exec** 系统调用重新进入内核。正如我们在第一章中所看到的，**exec** 用一个新的程序（本例中是/init）替换当前进程的内存和寄存器。一旦内核完成 **exec**，它就会在**/init** 进程中返回到用户空间。**init** 在需要时会创建一个新的控制台设备文件，然后以文件描述符 0、1 和 2 的形式打开它。然后它在控制台上启动一个 shell。这样系统就启动了。

`entry.S`

```assembly
	# qemu -kernel loads the kernel at 0x80000000
        # and causes each CPU to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
_entry:
	# set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        # stack0作为CPU栈的基地址
        la sp, stack0 
        # 把 4096 这个立即数读到 a0 寄存器中
        li a0, 1024*4
        # 把当前 CPU 的 ID 读到 a1 寄存器中
	csrr a1, mhartid
        # cpuid从0开始，注意CPU初始栈大小都是4096byte，且栈向下生长
        addi a1, a1, 1
        # 计算当前CPU栈的偏移地址
        mul a0, a0, a1
        # 算出栈顶地址（栈向下生长）并且放到 sp 寄存器中
        add sp, sp, a0
	# jump to start() in start.c
        call start
spin:
        j spin
```

`start.c`

```c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
void main();
void timerinit();
// entry.S needs one stack per CPU.
// 定义了 entry.S 中的 stack0 ，它要求 16bit 对齐
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];
// scratch area for timer interrupt, one per CPU.
// 定义了共享变量，即每个 CPU 的暂存区用于 machine-mode 定时器中断，它是和 timer 驱动之间传递数据用的。
uint64 mscratch0[NCPU * 32];
// assembly code in kernelvec.S for machine-mode timer interrupt.
// 声明了 timer 中断处理函数，在接下来的 timer 初始化函数中被用到
extern void timervec();
// entry.S jumps here in machine mode on stack0.
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  // 使 CPU 进入 supervisor mode 
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);
  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  // 设置了汇编指令 mret 后 PC 指针跳转的函数，也就是 main 函数
  w_mepc((uint64)main);
  // disable paging for now.
  // 暂时关闭了分页功能，即直接使用物理地址
  w_satp(0);
  // delegate all interrupts and exceptions to supervisor mode.
  // 将所有中断异常处理设定在给 supervisor mode 下
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  // External Interupt | Software Interupt | Timer Interupt
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
  // ask for clock interrupts.
  // 请求时钟中断，也就是 clock 的初始化
  timerinit();
  // keep each CPU's hartid in its tp register, for cpuid().
  // 将 CPU 的 ID 值保存在寄存器 tp 中
  int id = r_mhartid();
  w_tp(id);
  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}

// set up to receive timer interrupts in machine mode,
// which arrive at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  // 首先读出 CPU 的 ID
  int id = r_mhartid();
  // ask the CLINT for a timer interrupt.
  // 设置中断时间间隔，这里设置的是 0.1 秒
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;
  // prepare information in scratch[] for timervec.
  // scratch[0..3] : space for timervec to save registers.
  // scratch[4] : address of CLINT MTIMECMP register.
  // scratch[5] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &mscratch0[32 * id];
  scratch[4] = CLINT_MTIMECMP(id);
  scratch[5] = interval;
  w_mscratch((uint64)scratch);
  // set the machine-mode trap handler.
  // 设置中断处理函数
  w_mtvec((uint64)timervec);
  // enable machine-mode interrupts.
  // 打开中断
  w_mstatus(r_mstatus() | MSTATUS_MIE);
  // enable machine-mode timer interrupts.
  // 打开时钟中断
  w_mie(r_mie() | MIE_MTIE);
}
```

`types.h`

```c
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;
// 给数据类型起别名
typedef uint64 pde_t;
```

`param.h`

```c
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
```

`memlayout.h`

```c
// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio disk 
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// local interrupt controller, which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu puts programmable interrupt controller here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
// 物理内存实际只有128M
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
// 用户空间和内核空间的最高地址出都是trampoline代码（用户态和内核态切换）
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
// 内核内存trampoline页后面跟着内核栈页(每一个进程的内核栈都是两页，其中一页是守护页)
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
// 用户内存的trampoline页后面跟着trapframe页
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
```

`riscv.h`

```c
// which hart (core) is this?
static inline uint64
r_mhartid()
{
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

// Machine Status Register, mstatus
#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)    // machine-mode interrupt enable.

static inline uint64
r_mstatus()
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

static inline void 
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void 
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}

// Supervisor Status Register, sstatus
// sstatus 中的 SIE 位控制设备中断是否被启用
// SPP 位表示 trap 是来自用户模式还是监督者模式，并控制sret 返回到什么模式
#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline void 
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Pending
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

static inline void 
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r" (x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline void 
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r" (x));
}

// Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11) // external
#define MIE_MTIE (1L << 7)  // timer
#define MIE_MSIE (1L << 3)  // software
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

static inline void 
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r" (x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
// 当 trap 发生时，RISC-V 会将程序计数器保存在这里（因为 PC 会被 stvec 覆盖）。sret(从 trap 中返回)指令将 sepc 复制到 pc 中。
// 内核可以写 sepc 来控制 sret的返回到哪里。
static inline void 
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

// Machine Exception Delegation
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

static inline void 
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r" (x));
}

// Machine Interrupt Delegation
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

static inline void 
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r" (x));
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
// 内核在这里写下 trap 处理程序的地址；RISC-V 到这里来处理 trap
static inline void 
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

// Machine-mode interrupt vector
static inline void 
w_mtvec(uint64 x)
{
  asm volatile("csrw mtvec, %0" : : "r" (x));
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))
// supervisor address translation and protection;
// holds the address of the page table.
// 页表寄存器
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

// Supervisor Scratch register, for early trap handler in trampoline.S.
// 内核在这里放置了一个值，这个值会方便 trap 恢复/储存用户上下文
static inline void 
w_sscratch(uint64 x)
{
  asm volatile("csrw sscratch, %0" : : "r" (x));
}

static inline void 
w_mscratch(uint64 x)
{
  asm volatile("csrw mscratch, %0" : : "r" (x));
}

// Supervisor Trap Cause
// RISC -V 在这里放了一个数字，描述了 trap 的原因
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

// Supervisor Trap Value
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

// Machine-mode Counter-Enable
static inline void 
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}

// machine-mode cycle counter
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x) );
  return x;
}

// enable device interrupts
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int
intr_get()
{
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// read and write tp, the thread pointer, which holds
// this core's hartid (core number), the index into cpus[].
static inline uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void 
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}

// flush the TLB.
static inline void
sfence_vma()
{
  // the zero, zero means flush all TLB entries.
  asm volatile("sfence.vma zero, zero");
}

// 页大小(4k字节)
#define PGSIZE 4096 // bytes per page
// 页的偏移位(低12位)
#define PGSHIFT 12  // bits of offset within a page
// 临近的初始开始页
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
// 当前页的起始地址
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
// 有效位
#define PTE_V (1L << 0)
// 可读位 
#define PTE_R (1L << 1)
// 可写位
#define PTE_W (1L << 2)
// 可执行位
#define PTE_X (1L << 3)
// 用户模式访问位
#define PTE_U (1L << 4) // 1 -> user can access
// COW页标志
#define PTE_COW (1L << 7)

// shift a physical address to the right place for a PTE.
// 物理地址转换为PTE项(低10位需要额外填充标志位)
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
// PTE项转换为物理地址
#define PTE2PA(pte) (((pte) >> 10) << 12)
// 物理地址所在的块号
#define PA2IDX(pa) (((uint64)pa) >> 12)
// PTE项的低10位标记位
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// extract the three 9-bit page table indices from a virtual address.
// 抽取出页表项的页目录索引项(9位)
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs，一页4096字节，一个pte8字节
```

`main.c`

```c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
// 保证并发初始化正确
volatile static int started = 0;
// start() jumps here in supervisor mode on all CPUs.
// 进入main函数时系统为supervisor模式
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize(); // gcc 提供的原子操作，保证内存访问的操作都是原子操作
    started = 1; //设置初始化完成的标志
  } else {
    // 如果不是主 CPU ，首先循环等待主 CPU 初始化完成，当主 CPU 初始化完成，则初始化完成标志 started 为 1 ，跳出循环
    while(started == 0)
      ;
    // 原子屏障
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}

```

`proc.h`

```c
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
};
```

`proc.c:userinit()`

```c

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
// 建立第一个用户进程
void
userinit(void)
{
  struct proc *p;
  // 从进程表中分配一个进程作为initproc
  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  // 这个进程执行了一个initcode.S的汇编程序，这个汇编程序调用了exec这个system call来执行/init，重新进入kernel。
  uvminit(p->pagetable, initcode, sizeof(initcode));
  // init进程只有一页大小
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  // 准备好initproc被内核调度器线程选中需要restore的信息(epc==0表示从initproc进程的一开始执行，sp==PGSIZE表示initproc的用户栈从PGSIZE开始)
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  // 初始化进程的信息(名称、工作目录、进程状态)
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
  p->state = RUNNABLE;

  release(&p->lock);
}
```

`initcode.S`

```assembly
# Initial process that execs /init.
# This code runs in user space.
# 初始进程/init进程
#include "syscall.h"
# exec(init, argv)
.globl start
start:
        la a0, init
        la a1, argv
        li a7, SYS_exec
        ecall

# for(;;) exit();
exit:
        li a7, SYS_exit
        ecall
        jal exit

# char init[] = "/init\0";
init:
  .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .long init
  .long 0
```

`init.c`

```c
// init: The initial user-level program
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"
char *argv[] = { "sh", 0 };
int
main(void)
{
  int pid, wpid;
  // 打开三个文件描述符0、1、2
  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  // init进程是一个无限死循环
  for(;;){
    printf("init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      // 子进程执行sh程序
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      // 注意：如果一个父进程退出，子进程会被委托给init进程
      // 因此wait的返回不一定是因为shell进程的退出
      wpid = wait((int *) 0);
      if(wpid == pid){
        // the shell exited; restart it.
        // shell程序退出，需要重启一个shell子进程
        break;
      } else if(wpid < 0){
        // init进程肯定存在子进程，所以wait不可能返回-1
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // 委托给init进程的子进程退出，init进程继续等待
        // it was a parentless process; do nothing.
      }
    }
  }
}

```

### 第三章：页表

页表是操作系统为每个进程提供自己私有地址空间和内存的机制。页表决定了内存地址的含义，以及物理内存的哪些部分可以被访问。它们允许 xv6 隔离不同进程的地址空间，并将它们映射到物理内存上。页表还提供了一个间接层次，允许 xv6 执行一些技巧：在几个地址空间中映射同一内存(trampoline 页)，以及用一个未映射页来保护内核和用户的栈。本章其余部分将解释 RISC-V 硬件提供的页表以及 xv6 如何使用它们。

#### 3.1分页硬件

提醒一下，RISC-V 指令(包括用户和内核)操作的是虚拟地址。机器的 RAM，或者说物理内存，是用物理地址来做索引的，RISC-V 分页硬件将这两种地址联系起来，通过将每个虚拟地址映射到物理地址上。

xv6 运行在 Sv39 RISC-V 上，这意味着只使用 64 位虚拟地址的底部 39 位，顶部 25 位未被使用。在这种 Sv39 配置中，一个 RISC-V 页表在逻辑上是一个 2^27^（134,217,728）**页表项（Page Table Entry, PTE）**的数组。每个 PTE 包含一个 44 位的**物理页号(Physical Page** **Number，PPN)**和一些标志位。分页硬件通过利用 39 位中的高 27 位索引到页表中找到一个 **PTE** 来转换一个虚拟地址，并计算出一个 56 位的物理地址，它的前 44 位来自于 **PTE** 中的 **PPN**，而它的后 12 位则是从原来的虚拟地址复制过来的。图 3.1 显示了这个过程，在逻辑上可以把页表看成是一个简单的 **PTE** 数组（更完整的描述见图 3.2）。页表让操作系统控制虚拟地址到物理地址的转换，其粒度为 4096（2^12^）字节的对齐块。这样的分块称为页。

在 Sv39 RISC-V 中，虚拟地址的前 25 位不用于转换地址；将来，RISC-V 可能会使用这些位来定义更多的转换层。物理地址也有增长的空间：在 **PTE** 格式中，物理页号还有 10 位的增长空间。

![image-20221114153948359](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114153948359.png)

如图 3.2 所示，实际转换分三步进行。一个页表以三层树的形式存储在物理内存中。树的根部是一个 4096 字节的页表页，它包含 512 个 PTE，这些 PTE 包含树的下一级页表页的物理地址。每一页都包含 512 个 PTE，用于指向下一个页表或物理地址。分页硬件用 27 位中的顶 9 位选择根页表页中的 PTE，用中间 9 位选择树中下一级页表页中的 PTE，用底 9 位选择最后的 PTE。

如果转换一个地址所需的三个 PTE 中的任何一个不存在，分页硬件就会引发一个页面错误的异常(**page-fault exception**)，让内核来处理这个异常。这种三层结构的一种好处是，当有大范围的虚拟地址没有被映射时，可以省略整个页表页。

每个 PTE 包含标志位，告诉分页硬件如何允许使用相关的虚拟地址。**PTE_V** 表示 PTE 是否存在：如果没有设置，对该页的引用会引起异常（即不允许）。PTE_R 控制是否允许指令读取到页。**PTE_W** 控制是否允许指令向写该页。**PTE_X** 控制 CPU 是否可以将页面的内容解释为指令并执行。**PTE_U** 控制是否允许用户模式下的指令访问页面；如果不设置 **PTE_U**，PTE 只能在监督者模式下使用。

![image-20221114154520842](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114154520842.png)

要告诉硬件使用页表，内核必须将根页表页的物理地址写入 **satp** 寄存器中。每个 CPU都有自己的 **satp** 寄存器。一个 CPU 将使用自己的 **satp** 所指向的页表来翻译后续指令产生的所有地址。每个 CPU 都有自己的 **satp**，这样不同的 CPU 可以运行不同的进程，每个进程都有自己的页表所描述的私有地址空间。

关于术语的一些说明：物理内存指的是 **DRAM** 中的存储单元。物理存储器的一个字节有一个地址，称为物理地址。当指令操作虚拟地址时，分页硬件会将其翻译成物理地址，然后发送给 DRAM 硬件，以读取或写入存储。不像物理内存和虚拟地址，虚拟内存不是一个物理对象，而是指内核提供的管理物理内存和虚拟地址的抽象和机制的集合。

#### 3.2内核地址空间

Xv6 为每个进程维护页表，一个是进程的用户地址空间，外加一个内核地址空间的单页表。内核配置其地址空间的布局，使其能够通过可预测的虚拟地址访问物理内存和各种硬件资源 。 图 3.3 显示了这个设计是如何将内核虚拟地址映射到物理地址的。

![image-20221114155032464](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114155032464.png)

QEMU 模拟的计算机包含 RAM（物理内存），从物理地址 **0x80000000**，至少到0x86400000，xv6 称之为 **PHYSTOP**。QEMU 模拟还包括 I/O 设备，如磁盘接口。QEMU 将设备接口作为 **memory-mapped(内存映射)**控制寄存器暴露给软件，这些寄存器位于物理地址空间的 **0x80000000** 以下。内核可以通过读取/写入这些特殊的物理地址与设备进行交互；这种读取和写入与设备硬件而不是与 RAM 进行通信。

内核使用“直接映射”**RAM** 和**内存映射**设备寄存器，也就是在虚拟地址上映射硬件资源，这些地址与物理地址相等。例如，内核本身在虚拟地址空间和物理内存中的位置都是**KERNBASE=0x80000000**。直接映射简化了读/写物理内存的内核代码。例如，当 fork 为子进程分配用户内存时，分配器返回该内存的物理地址；**fork** 在将父进程的用户内存复制到子进程时，直接使用该地址作为虚拟地址。

有几个内核虚拟地址不是直接映射的:

1．trampoline 页。它被映射在虚拟地址空间的顶端；用户页表也有这个映射。我们在这里看到了页表的一个有趣的用例；一个物理页（存放 trampoline 代码）在内核的虚拟地址空间中被映射了两次：一次是在虚拟地址空间的顶部，一次是直接映射。

2．内核栈页。每个进程都有自己的内核栈，内核栈被映射到地址高处，所以在它后面xv6 可以留下一个未映射的守护页。守护页的 PTE 是无效的（设置 **PTE_V**），这样如果内核溢出内核 stack，很可能会引起异常，内核会报错。如果没有守护页，栈溢出时会覆盖其他内核内存，导致不正确的操作。报错还是比较好的。

内核为 trampoline 和 text(可执行程序的代码段)映射的页会有 **PTE_R** 和 **PTE_X** 权限。内核从这些页读取和执行指令。内核映射的其他 page 会有 **PTE_R** 和 **PTE_W** 权限，以便内核读写这些页面的内存。守护页的映射是无效的（设置 **PTE_V**）；

#### 3.3创建地址空间

大部分用于操作地址空间和页表的 xv6 代码都在 **vm.c**中。核心数据结构是 **pagetable_t**，它实际上是一个指向 RISC-V 根页表页的指针；**pagetable_t** 可以是内核页表，也可以是进程的页表。核心函数是 **walk** 和 **mappages**，前者通过虚拟地址得到 PTE，后者将虚拟地址映射到物理地址。以 **kvm** 开头的函数操作内核页表；以 uvm 开头的函数操作用户页表；其他函数用于这两种页表。**copyout** 可以将内核数据复制到用户虚拟地址，**copyin** 可以将用户虚拟地址的数据复制到内核空间地址，用户虚拟地址由系统调用的参数指定；它们在 **vm.c** 中，因为它们需要显式转换这些地址以便找到相应的物理内存。

在启动序列的前面，**main** 调用 **kvminit**来创建内核的页表。这个调用发生在 xv6 在 RISC-V 启用分页之前，所以地址直接指向物理内存。**Kvminit** 首先分配一页物理内存来存放根页表页。然后调用kvmmap将内核所需要的硬件资源映射到物理地址。这些资源包括内核的指令和数据，KERNBASE 到 PHYSTOP（0x86400000）的物理内存，以及实际上是设备的内存范围。

**kvmmap**调用 **mappages**，它将一个虚拟地址范围映射到一个物理地址范围。它将范围内地址分割成多页（忽略余数），每次映射一页的顶端地址。对于每个要映射的虚拟地址（页的顶端地址），mapages 调用 walk 找到该地址的最后一级 PTE 的地址。然后，它配置 PTE，使其持有相关的物理页号、所需的权限(**PTE_W、PTE_X**和/或 PTE_R**)，以及 **PTE_V来标记 PTE 为有效。

**walk**模仿 RISC-V 分页硬件查找虚拟地址的 PTE(见图 3.2)。walk 每次降低 3 级页表的 9 位。它使用每一级的 9 位虚拟地址来查找下一级页表或最后一级的 PTE。如果 PTE 无效，那么所需的物理页还没有被分配；如果 **alloc** 参数被设置 **true**，**walk** 会分配一个新的页表页，并把它的物理地址放在 PTE 中。它返回 PTE在树的最低层的地址。

**main** 调用 **kvminithart** 来映射内核页表。它将根页表页的物理地址写入寄存器 **satp** 中。在这之后，CPU 将使用内核页表翻译地址。由于内核使用唯一映射，所以指令的虚拟地址将映射到正确的物理内存地址。

**procinit**，它由 **main** 调用，为每个进程分配一个内核栈。它将每个栈映射在 **KSTACK** 生成的虚拟地址上，这就为栈守护页留下了空间。**Kvmmap** 栈的虚拟地址映射到申请的物理内存上，然后调用 **kvminithart** 将内核页表重新加载到 **satp** 中，这样硬件就知道新的 PTE 了。

每个 RISC-V CPU 都会在 **Translation Look-aside Buffer(TLB)**中缓存页表项，当 xv6 改变页表时，必须告诉 CPU 使相应的缓存 TLB 项无效。如果它不这样做，那么在以后的某个时刻，TLB 可能会使用一个旧的缓存映射，指向一个物理页，而这个物理页在此期间已经分配给了另一个进程，这样的话，一个进程可能会在其他进程的内存上“乱写乱画“。RISC-V 有一条指令 **sfence.vma**，可以刷新当前 CPU 的 TLB。xv6 在重新加载 satp 寄存器后，在**kvminithart** 中执行 **sfence.vma**，也会在从内核空间返回用户空间前，切换到用户页表的trampoline 代码中执行 **sfence.vma**。

`vm.c`

```c
#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
/*
 * the kernel's page table.
 * 内核页表，所有CPU共享
 */
pagetable_t kernel_pagetable;
extern char etext[];  // kernel.ld sets this to end of kernel code.
extern char trampoline[]; // trampoline.S
/*
 * create a direct-map page table for the kernel.
 * 为内核建立直接映射的页表
 */
void
kvminit()
{
  // 首先分配一页物理内存来存放根页表页
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // 调用 kvmmap 将内核所需要的硬件资源映射到物理地址,内存映射IO
  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  // 将根页表页的物理地址写入寄存器 satp 中
  w_satp(MAKE_SATP(kernel_pagetable));
  // 刷新当前 CPU 的 TLB
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  // 虚拟地址不能超出最大范围
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    //使用每一级的 9 位虚拟地址来查找下一级页表或最后一级PTE
    pte_t *pte = &pagetable[PX(level, va)];
    //PTE项有效
    if(*pte & PTE_V) {
      //当前PTE指向的下一级页表
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      //alloc为0或者新页表分配内存失败，直接退出
      if(!alloc || (pagetable = (pte_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      //填充当前的PTE
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  //最终返回最后一级PTE
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  // 校验虚拟地址范围正确性
  if(va >= MAXVA)
    return 0;
  // 获得最后一级PTE
  pte = walk(pagetable, va, 0);
  // ptr==0说明该虚拟地址没有对应的物理地址与之映射
  if(pte == 0)
    return 0;
  // 当前pte已失效
  if((*pte & PTE_V) == 0)
    return 0;
  // 允许用户模式下的指令查看页面
  if((*pte & PTE_U) == 0)
    return 0;
  //pte转为物理地址
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
// 仅在boot启动阶段使用，映射的虚拟地址和物理地址都是连续的，注意在此之前end~PHYSTOP的物理内存块都被串联成单链表供内存分配
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  // 它将一个虚拟地址范围映射到一个物理地址范围。它将范围内地址分割成多页（忽略余数），每次映射一页的顶端地址
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(kernel_pagetable, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);// 开始页地址
  last = PGROUNDDOWN(va + size - 1);// 终止页地址
  for(;;){
    //调用 walk 找到该地址的最后一级 PTE 的地址
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    //如果PTE不空且有效则说明是重新映射
    if(*pte & PTE_V)
      panic("remap");
    //PTE的0~9位是标志位，10~53位是物理页号，54~63位保留
    *pte = PA2PTE(pa) | perm | PTE_V;
    //如果map的虚拟页到达终止页，结束该次mappages
    if(a == last)
      break;
    //当前映射页后移一页
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  //虚拟地址必须页对齐
  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    // 页目录项必须存在
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    // 页目录项必须有效
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    // PTE是中间目录项(不是叶子目录项)
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    // 可选项，是否释放物理内存页
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    //清空pte表项
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
// 创建一个空用户页表
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  // 创建一个空的user页表
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
// uvminit(p->pagetable, initcode, sizeof(initcode));
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;
  // initcode.S不超过一页
  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  // 空页初始化
  memset(mem, 0, PGSIZE);
  // 初始化进程的虚拟地址0映射到实际分配的物理页
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  // 给实际的物理页填充内容
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;
  // 需要映射的下一个地址
  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    // 分配一页物理内存
    mem = kalloc();
    if(mem == 0){
      // 分配过程中失败，则页表应该恢复到原来的大小
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    // 初始化物理页内容
    memset(mem, 0, PGSIZE);
    // 扩展的虚拟地址映射到新的物理地址
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      // 映射失败则释放当前分配的内存页，页表应该恢复到原来的大小
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;
  // 解除用户页表的部分内存映射
  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    // 计算需要解除映射的页数
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    // 从PGROUNDUP(newsz)开始移除npages页，并且释放对应的物理内存(do_free=1)
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // this PTE points to a lower-level page table.
    // PTE项是中间页表目录项
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){    
      // 进入下一级页表进行页表页释放
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      // PTE对应的下级页表释放完毕，PTE项置零
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      // 所有叶子目录的映射本应该在此之前全部移除
      panic("freewalk: leaf");
    }
  }
  // 页目录项全部置零后释放该级页表
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  // 释放用户内存页
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  // 释放页表页
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
// fork创建子进程时会默认为子进程分配物理内存，并将父进程的内存复制到子进程中
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    // 页目录项必须存在
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    // 页目录项必须有效
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    // 虚拟地址从0开始的i对应的物理地址
    pa = PTE2PA(*pte);
    // 页目录项的低10位标志位
    flags = PTE_FLAGS(*pte);
    // 为子进程分配物理内存
    if((mem = kalloc()) == 0)
      goto err;
    // 把父进程的内存搬至子进程
    memmove(mem, (char*)pa, PGSIZE);
    // 完成子进程的页表地址映射
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  // 失败情况下需要释放所有已分配的页
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
// PTE项标记为用户模式无法访问，用于exec执行用户程序的用户栈后的守护页(发生栈溢出时就会报错无法访问特权内存区)
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    // 虚拟地址dstva开始页
    va0 = PGROUNDDOWN(dstva);
    // 虚拟地址对应的物理地址
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    // 需要copy的字节数，开始页比较特殊（不一定全部copy）
    n = PGSIZE - (dstva - va0);
    // 多出的部分需要截断
    if(n > len)
      n = len;
    // 把src开始处的n个字节拷贝到实际的物理地址处pa0 + (dstva - va0)
    memmove((void *)(pa0 + (dstva - va0)), src, n);
    // 待拷贝字节数减少n，src后移n，dstva跳到下一页开始处（page aligend）
    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    // 虚拟地址srcva的开始页
    va0 = PGROUNDDOWN(srcva);
    // 虚拟地址对应的物理地址
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    // 当前页实际需要拷贝的字节数
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    // 把物理地址pa0 + (srcva - va0)处的n个字节拷贝到dst处
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);
    // 待拷贝字节数减少n，dst后移n，srcva跳到下一页开始处（page aligend）
    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  // 字符串终止结束符标志位
  int got_null = 0;
  // 没有遇到字符串终止'\0'并且复制字符数还没有达到max
  while(got_null == 0 && max > 0){
    // srcva虚拟地址的起始页
    va0 = PGROUNDDOWN(srcva);
    // walkaddr模拟分页硬件确定 va0 的物理地址 pa0
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    // 需要复制的字节数
    n = PGSIZE - (srcva - va0);
    // 确保不多复制
    if(n > max)
      n = max;
    // 待复制字节的源物理地址开始处
    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      // 遇到终止符结束copy
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        // 否则一直复制
        *dst = *p;
      }
      --n;
      --max;
      p++;
      // 可以看出，目标地址(dst==buf)应该是连续一块内存
      dst++;
    }
    // 切换到下一页继续复制
    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}
```

#### 3.4物理内存分配

内核必须在运行时为页表、用户内存、内核堆栈和管道缓冲区分配和释放物理内存。xv6使用内核地址结束到 **PHYSTOP** 之间的物理内存进行运行时分配。它每次分配和释放整个4096 字节的页面。它通过保存空闲页链表，来记录哪些页是空闲的。分配包括从链表中删除一页；释放包括将释放的页面添加到空闲页链表中。

分配器在 **kalloc.c**中。分配器的数据结构是一个可供分配的物理内存页的**空闲页链表**，每个空闲页的链表元素是一个结构体 **run**。分配器从哪里获得内存来存放这个结构体呢？它把每个空闲页的 **run** 结构体存储在空闲页本身，因为那里没有其他东西存储。空闲链表由一个**自旋锁**保护。链表和锁被包裹在一个结构体中，以明确锁保护的是结构体中的字段。

**main** 函数调用 **kinit** 来初始化分配器。**kinit** 初始空闲页链表，以保存内核地址结束到 **PHYSTOP** 之间的每一页。xv6 应该通过解析硬件提供的配置信息来确定有多少物理内存可用。但是它没有做，而是假设机器有 128M 字节的 RAM。**Kinit** 通过调用**freerange** 来添加内存到空闲页链表，**freerange** 则对每一页都调用 **kfree**。PTE 只能引用按4096 字节边界对齐的物理地址(4096 的倍数)，因此 **freerange** 使用 **PGROUNDUP** 来确保它只添加对齐的物理地址到空闲链表中。分配器开始时没有内存;这些对 **kfree** 的调用给了它一些内存管理。

分配器有时把地址当作整数来处理，以便对其进行运算（如 **freerange** 遍历所有页），有时把地址当作指针来读写内存（如操作存储在每页中的 **run** 结构体）；这种对地址的双重使用是分配器代码中充满 C 类型转换的主要原因。另一个原因是，释放和分配本质上改变了内存的类型。

`kalloc.c`

```c
// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
// 物理内存分配器，分配单位是4k字节
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
void freerange(void *pa_start, void *pa_end);
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// 每个空闲页的 run 结构体存储在空闲页本身
struct run {
  struct run *next;
};

// 空闲链表由一个自旋锁保护
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // 初始空闲页链表，以保存内核地址结束end到PHYSTOP之间的每一页
  // xv6 应该通过解析硬件提供的配置信息来确定有多少物理内存可用。
  // 但是它没有做，而是假设机器有128M字节的RAM
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  // Fill with junk to catch dangling refs.
  // 这将使得释放内存后使用内存的代码(使用悬空引用)读取垃圾而不是旧的有效内容；
  // 希望这将导致这类代码更快地崩溃
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  // kalloc 移除并返回空闲链表中的第一个元素
  return (void*)r;
}
```

#### 3.5进程地址空间

每个进程都有一个单独的页表，当 xv6 在进程间切换时，也会改变页表。如图 2.3 所示，一个进程的用户内存从虚拟地址 0 开始，可以增长到 MAXVA，原则上允许一个进程寻址 256GB 的内存。

当一个进程要求 xv6 提供更多的用户内存时，xv6 首先使用 **kalloc** 来分配物理页，然后将指向新物理页的 PTE 添加到进程的页表中。然后它将指向新物理页的 PTE 添加到进程的页表中。Xv6 在这些 PTE 中设置 **PTE_W**、**PTE_X**、**PTE_R**、**PTE_U** 和 **PTE_V** 标志。大多数进程不使用整个用户地址空间；xv6 使用 **PTE_V** 来清除不使用的 PTE。

我们在这里看到了几个例子，是关于使用页表的。首先，不同的进程页表将用户地址转化为物理内存的不同页，这样每个进程都有私有的用户内存。第二，每个进程都认为自己的内存具有从零开始的连续的虚拟地址，而进程的物理内存可以是不连续的。第三，内核会映射带有 **trampoline** 代码的页，该 **trampoline** 处于用户地址空间顶端，因此，在所有地址空间中都会出现一页物理内存。

![image-20221114180046008](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221114180046008.png)

图 3.4 更详细地显示了 xv6 中执行进程的用户内存布局。栈只有一页，图中显示的是由**exec** 创建的初始内容。字符串的值，以及指向这些参数的指针数组，位于栈的最顶端。下面是允许程序在 **main** 启动的值，就像函数 **main(argc, argv)**刚刚被调用一样。

为了检测用户栈溢出分配的栈内存，xv6 会在 stack 的下方放置一个无效的保护页。如果用户栈溢出，而进程试图使用栈下面的地址，硬件会因为该映射无效而产生一个页错误异常。现实世界中的操作系统可能会在用户栈溢出时自动为其分配更多的内存。

#### 3.6sbrk解析

**Sbrk** 是 一 个 进 程 收 缩 或 增 长 内 存 的 系 统 调 用 。 该 系 统 调 用 由 函 数**growproc**实现，**growproc** 调用 **uvmalloc** 或 **uvmdealloc**，取决于 n 是正数还是负数。**uvmdealloc** 调用 **uvmunmap** ，它使用 walk 来查找 PTE，使用 **kfree** 来释放它们所引用的物理内存。

xv6 使用进程的页表不仅是为了告诉硬件如何映射用户虚拟地址，也是将其作为分配给该进程的物理地址的唯一记录。这就是为什么释放用户内存（**uvmunmap** 中）需要检查用户页表的原因。

```c
// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
// 增长或收缩用户内存，n可正可负
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}
```

#### 3.7exec解析

> 参考资料：https://paper.seebug.org/papers/Archive/refs/elf/Understanding_ELF.pdf

Exec 是创建用户地址空间的系统调用。`它读取储存在文件系统上的文件用来初始化用户地址空间。`Exec使用 namei 打开二进制文件路径，然后它读取 ELF 头。xv6 应用程序用 ELF 格式来描述可执行文件，它定义在kernel/elf.h。一个 ELF 二进制文件包括一个 ELF 头(elfhdr 结构体)，后面是一个程序节头 (program section header) 序列， 程序节头为一个结构体proghdr。每一个 proghdr 描述了一个必须加载到内存中的程序节；xv6 程序只有一个程序节头，但其他系统可能有单独的指令节和数据节需要加载到内存。

```c
// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  // 最开始处的这 16 个字节含有 ELF 文件的识别标志，并且提供了一些用于解码和解析文件内容的数据，是不依赖于具体操作系统的。ELF 文件最开始的这一部分的格式是固定并通用的，在所有平台上都一样。所有处理器都可能用固定的格式去读取这一部分的内容，从而获知这个 ELF 文件中接下来的内容应该如何读取和解析。
  // ELF 标识，即前述 ELF 文件头结构最开始的 16 个字节，作为一个数组，它的各个索引位置的字节数据有固定的含义。
  uint magic;  // must equal ELF_MAGIC
  uchar elf[12];
  // 此字段表明本目标文件属于哪种类型:可重定位文件/可执行文件/动态链接库文件
  ushort type;
  // 此字段用于指定该文件适用的处理器体系结构。
  ushort machine;
  // 此字段指明目标文件的版本。
  uint version;
  // 此字段指明程序入口的虚拟地址。即当文件被加载到进程空间里后，入口程序在进程地址空间里的地址。对于可执行程序文件来说，当 ELF 文件完成加载之后，程序将从这里开始运行；而对于其它文件来说，这个值应该是 0。
  uint64 entry;
  // 此字段指明程序头表(program header table)开始处在文件中的偏移量。如果没有程序头表，该值应设为 0。
  uint64 phoff;
  // 此字段指明节头表(section header table)开始处在文件中的偏移量。如果没有节头表，该值应设为 0。
  uint64 shoff;
  // 此字段含有处理器特定的标志位.
  uint flags;
  // 此字段表明 ELF 文件头的大小，以字节为单位。
  ushort ehsize;
  // 此字段表明在程序头表中每一个表项的大小，以字节为单位。
  ushort phentsize;
  // 此字段表明程序头表中总共有多少个表项。如果一个目标文件中没有程序头表，该值应设为 0。
  ushort phnum;
  // 此字段表明在节头表中每一个表项的大小，以字节为单位。
  ushort shentsize;
  // 此字段表明节头表中总共有多少个表项。如果一个目标文件中没有节头表，该值应设为 0。
  ushort shnum;
  // 节头表中与节名字表相对应的表项的索引。如果文件没有节名字表，此值应设置为 SHN_UNDEF。
  ushort shstrndx;
};

// Program section header
struct proghdr {
  // 此数据成员说明了本程序头所描述的段的类型，或者如何解析本程序头的信息。
  uint32 type;
  // 此数据成员给出了本段内容的属性。
  uint32 flags;
  // 此数据成员给出本段内容在文件中的位置，即段内容的开始位置相对于文件开头的偏移量。
  uint64 off;
  // 此数据成员给出本段内容的开始位置在进程空间中的虚拟地址。
  uint64 vaddr;
  // 此数据成员给出本段内容的开始位置在进程空间中的物理地址。对于目前大多数现代操作系统而言，应用程序中段的物理地址事先是不可知的，所以目前这个成员多数情况下保留不用，或者被操作系统改作它用。
  uint64 paddr;
  // 此数据成员给出本段内容在文件中的大小，单位是字节，可以是 0。
  uint64 filesz;
  // 此数据成员给出本段内容在内容镜像中的大小，单位是字节，可以是 0。
  uint64 memsz;
  // 对于可装载的段来说，其 p_vaddr 和 p_offset 的值至少要向内存页面大小对齐。此数据成员指明本段内容如何在内存和文件中对齐。如果该值为 0 或 1，表明没有对齐要求；否则，p_align 应该是一个正整数，并且是 2 的幂次数。p_vaddr 和p_offset 在对 p_align 取模后应该相等。
  uint64 align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4
```

第一步是快速检查文件是否包含一个 ELF 二进制文件。一个 ELF 二进制文件以`四个字节的”魔法数字“ 0x7F、E、L、F`或 ELF_MAGIC开始。如果 ELF 头有正确的”魔法数字“，exec 就会认为该二进制文件是正确的类型。

`Exec 使用 proc_pagetable分配一个没有使用的页表，使用 uvmalloc为每一个 ELF 段分配内存，通过 loadseg 加载每一个段到内存中。loadseg 使用 walkaddr 找到分配内存的物理地址，在该地址写入 ELF 段的每一页，页的内容通过 readi 从文件中读取。`

==Exec 在栈页的下方放置了一个不可访问页，这样程序如果试图使用多个页面，就会出现故障。这个不可访问的页允许 exec 处理过大的参数；在这种情况下，exec 用来复制参数到栈的 copyout函数会注意到目标页不可访问，并返回-1。==

在准备新的内存映像的过程中，如果 exec 检测到一个错误，比如一个无效的程序段，它就会跳转到标签 bad，释放新的映像，并返回-1。exec 必须延迟释放旧映像，直到它确定exec 系统调用会成功：如果旧映像消失了，系统调用就不能返回-1。exec 中唯一的错误情况发生在创建映像的过程中。一旦镜像完成，exec 就可以提交到新的页表并释放旧的页表。

`Exec 将 ELF 文件中的字节按 ELF 文件指定的地址加载到内存中。用户或进程可以将任何他们想要的地址放入 ELF 文件中。因此，Exec 是有风险的，因为 ELF 文件中的地址可能会意外地或故意地指向内核。`对于一个不小心的内核来说，后果可能从崩溃到恶意颠覆内核的隔离机制(即安全漏洞)。xv6 执行了一些检查来避免这些风险。例如 if(ph.vaddr + ph.memsz < ph.vaddr)检查总和是否溢出一个 64 位整数。`危险的是，用户可以用指向用户选择的地址的 ph.vaddr 和足够大的 ph.memsz 来构造一个 ELF 二进制，使总和溢出到 0x1000，这看起来像是一个有效值。`在旧版本的 xv6 中，用户地址空间也包含内核（但在用户模式下不可读/写），用户可以选择一个对应内核内存的地址，从而将 ELF 二进制中的数据复制到内核中。`在 RISC-V 版本的 xv6 中，这是不可能的，因为内核有自己独立的页表；loadseg 加载到进程的页表中，而不是内核的页表中。`

==内核开发人员很容易忽略一个关键的检查，现实中的内核有很长一段缺少检查的空档期，用户程序可以利用缺少这些检查来获得内核特权。xv6 在验证需要提供给内核的用户程序数据的时候，并没有完全验证其是否是恶意的，恶意用户程序可能利用这些数据来绕过 xv6 的隔离。==

```C
int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG+1], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Check ELF header
  // 一个 ELF 二进制文件包括一个 ELF 头，elfhdr 结构体
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  // 如果 ELF 头有正确的"魔法数字"，exec 就会认为该二进制文件是正确的类型。
  if(elf.magic != ELF_MAGIC)
    goto bad;

  // 分配一个没有使用的页表
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  // 根据程序头表把程序加载进内存中
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    sz = sz1;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;
  
  // 保存原进程的大小便于内存清理
  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.
  // 分配两页作为用户栈使用
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-2*PGSIZE);
  sp = sz;
  stackbase = sp - PGSIZE;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  // 进程镜像切换
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
```

### 第四章：陷入和系统调用

有三种事件会导致 CPU 搁置普通指令的执行，强制将控制权转移给处理该事件的特殊代码。一种情况是**系统调用**，当用户程序执行 **ecall** 指令要求内核为其做某事时。另一种情况是**异常**：一条指令（用户或内核）做了一些非法的事情，如除以零或使用无效的虚拟地址。第三种情况是设备**中断**，当一个设备发出需要注意的信号时，例如当磁盘硬件完成一个读写请求时。

本文使用 **trap** 作为这些情况的通用术语。通常，代码在执行时发生 trap，之后都会被恢复，而且不需要意识到发生了什么特殊的事情。也就是说，我们通常希望 trap 是透明的；这一点对于中断来说尤其重要，被中断的代码通常不会意识到会发生 trap。通常的顺序是：

trap 迫使控制权转移到内核；内核保存寄存器和其他状态，以便恢复执行；内核执行适当的处理程序代码（例如，系统调用实现或设备驱动程序）；内核恢复保存的状态，并从 trap 中返回；代码从原来的地方恢复。

xv6 内核会处理所有的 trap。这对于系统调用来说是很自然的。这对中断来说也是合理的，因为隔离要求用户进程不能直接使用设备，而且只有内核才有设备处理所需的状态。这对异常处理来说也是合理的，因为 xv6 响应所有来自用户空间的异常，并杀死该违规程序。

Xv6 trap 处理分为四个阶段：RISC-V CPU 采取的硬件行为，为内核 C 代码准备的汇编入口，处理 trap 的 C 处理程序，以及系统调用或设备驱动服务。虽然三种 trap 类型之间的共性表明，内核可以用单一的代码入口处理所有的 trap，但事实证明，为三种不同的情况，即来自用户空间的 trap、来自内核空间的 trap 和定时器中断，设置单独的汇编入口和 C trap处理程序是很方便的。

#### 4.1RISC-V陷入机制

每个 RISC-V CPU都有一组控制寄存器，内核写入这些寄存器来告诉CPU 如何处理 trap，内核可以通过读取这些寄存器来发现已经发生的 trap。RISC-V 文档包含了完整的叙述。riscv.h包含了 xv6 使用的定义。这里是最重要的寄存器的概述。

- **stvec**：内核在这里写下 trap 处理程序的地址；RISC-V 到这里来处理 trap。
- **sepc**：当 trap 发生时，RISC-V 会将程序计数器保存在这里（因为 **PC** 会被 **stvec** 覆盖）。**sret**(从 trap 中返回)指令将 **sepc** 复制到 pc 中。内核可以写 **sepc** 来控制 **sret**的返回到哪里。

- **scause**：RISC -V 在这里放了一个数字，描述了 trap 的原因。
- **sscratch**：内核在这里放置了一个值，这个值会方便 trap 恢复/储存用户上下文。
- **sstatus**:：**sstatus** 中的 **SIE** 位控制设备中断是否被启用，如果内核清除 **SIE**，RISC-V 将推迟设备中断，直到内核设置 **SIE**。**SPP** 位表示 trap 是来自用户模式还是监督者模式，并控制**sret** 返回到什么模式

上述寄存器与在监督者模式下处理的 trap 有关，在用户模式下不能读或写。对于机器模式下处理的 trap，有一组等效的控制寄存器；xv6 只在定时器中断的特殊情况下使用它们。

多核芯片上的每个 CPU 都有自己的一组这些寄存器，而且在任何时候都可能有多个CPU 在处理一个 trap。当需要执行 trap 时，RISC-V 硬件对所有的 trap 类型（除定时器中断外）进行以下操作：

1. 如果该 trap 是设备中断，且 **sstatus SIE** 位为 0，则不要执行以下任何操作。
2. 通过清除 SIE 来禁用中断。
3. 复制 pc 到 **sepc**
4. 将当前模式(用户或监督者)保存在 sstatus 的 **SPP** 位。
5. 在 **scause** 设置该次 trap 的原因。
6. 将模式转换为监督者。
7. 将 **stvec** 复制到 pc。
8. 执行新的 pc

注意，CPU 不会切换到内核页表，不会切换到内核中的栈，也不会保存 pc 以外的任何寄存器。内核软件必须执行这些任务。CPU 在 trap 期间做很少的工作的一个原因是为了给软件提供灵活性，例如，一些操作系统在某些情况下不需要页表切换，这可以提高性能。

你可能会想 CPU 的 trap 处理流程是否可以进一步简化。例如，假设 CPU 没有切换程序计数器PC。那么 trap 可以切换到监督者模式时，还在运行用户指令。这些用户指令可以打破用户空间/内核空间的隔离，例如通过修改 satp 寄存器指向一个允许访问所有物理内存的页表。因此，CPU 必须切换到内核指定的指令地址，即 **stvec**。

#### 4.2用户空间的陷入

在用户空间执行时，如果用户程序进行了系统调用(**ecall** 指令)，或者做了一些非法的事情 ， 或 者 设 备 中 断 ， 都 可 能 发 生 trap 。 来 自 用 户 空 间 的 trap 的 处 理 路 径 是**uservec**， 然 后usertrap； 返 回 时 是**usertrapret**，然后是 **userret**。

来自用户代码的 trap 比来自内核的 trap 更具挑战性，因为 **satp** 指向的用户页表并不映射内核，而且栈指针可能包含一个无效甚至恶意的值。

因为 RISC-V 硬件在 trap 过程中不切换页表，所以用户页表必须包含 **uservec** 的映射，即 **stvec** 指向的 trap 处理程序地址。**uservec** 必须切换 **satp**，使其指向内核页表；为了在切换后继续执行指令，**uservec** 必须被映射到内核页表与用户页表相同的地址。

Xv6 用一个包含 **uservec** 的 trampoline 页来满足这些条件。Xv6 在内核页表和每个用户页表中的同一个虚拟地址上映射了 trampoline 页。这个虚拟地址就是 TRAMPOLINE。**trampoline.S** 中包含 trampoline 的内容，（执行用户代码时）**stvec** 设置为 **uservec**。

当 **uservec** 启动时，所有 32 个寄存器都包含被中断的代码所拥有的值。但是 **uservec**需要能够修改一些寄存器，以便设置 satp 和生成保存寄存器的地址。RISC-V 通过 **sscratch**寄存器提供了帮助。**uservec** 开始时的 **csrrw** 指令将 **a0** 和 **sscratch** 的内容互换。现在用户代码的 **a0** 被保存了；**uservec** 有一个寄存器（**a0**）可以使用；**a0** 包含了内核之前放在 **sscratch**中的值。

**uservec** 的下一个任务是保存用户寄存器。在进入用户空间之前，内核先设置 **sscratch**指向该进程的 **trapframe**，这个 **trapframe** 可以保存所有用户寄存器。因为 **satp** 仍然是指用户页表，所以 **uservec** 需要将 **trapframe** 映射到用户地址空间中。当创建每个进程时，xv6 为进程的 **trapframe** 分配一页内存，并将它映射在用户虚拟地址**TRAPFRAME**，也就是 **TRAMPOLINE** 的下面。进程的 **p->trapframe** 也指向 **trapframe**，不过是指向它的物理地址，这样内核可以通过内核页表来使用它。

因此，在交换 **a0** 和 **sscratch** 后，**a0** 将指向当前进程的 **trapframe**。**uservec** 将在**trapframe** 保存全部的寄存器，包括从 **sscratch** 读取的 **a0**。

trapframe 包含指向当前进程的内核栈、当前 CPU 的 **hartid**、**usertrap** 的地址和内核页表的地址的指针，**uservec** 将这些值设置到相应的寄存器中，并将 **satp** 切换到内核页表和刷新 TLB，然后调用 **usertrap**。

**usertrap** 的作用是确定 trap 的原因，处理它，然后返回。如上所述，它首先改变 **stvec**，这样在内核中发生的 trap 将由 **kernelvec** 处理。它保存了 **sepc**(用户 PC)，这也是因为 **usertrap** 中可能会有一个进程切换，导致 **sepc** 被覆盖。如果 trap 是系统调用，**syscall** 会处理它；如果是设备中断，**devintr** 会处理；否则就是异常，内核会杀死故障进程。**usertrap** 会把用户 pc 加 4，因为 RISC-V 在执行系统调用时，会留下指向 **ecall** 指令的程序指针。在退出时，**usertrap** 检查进程是否已经被杀死或应该让出 CPU（如果这个 trap 是一个定时器中断）。

回到用户空间的第一步是调用 usertrapret。这个函数设置 RISC-V 控制寄存器，为以后用户空间 trap 做准备。这包括改变 **stvec** 来引用 **uservec**，准备 **uservec** 所依赖的 **trapframe** 字段，并将 **sepc** 设置为先前保存的用户程序计数器。最后，**usertrapret**在用户页表和内核页表中映射的 trampoline 页上调用 **userret**，因为 **userret** 中的汇编代码会切换页表。

**usertrapret** 对 **userret** 的调用传递了参数 **a0，a1， a0** 指向 TRAPFRAME，**a1** 指向用户进程页表，**userret** 将 **satp** 切换到进程的用户页表。回想一下，用户页表同时映射了 trampoline 页和 **TRAPFRAME**，但没有映射内核的其他内容。同样，事实上，在用户页表和内核页表中，trampoline 页被映射在相同的虚拟地址上，这也是允许**uservec** 在改变 **satp** 后继续执行的原因。**userret** 将 **trapframe** 中保存的用户 **a0** 复制到**sscratch** 中，为以后与 **TRAPFRAME** 交换做准备。从这时开始，**userret** 能使用的数据只有寄存器内容和 **trapframe** 的内容。接下来 **userret** 从 **trapframe** 中恢复保存的用户寄存器，对 **a0** 和 **sscratch** 做最后的交换，恢复用户 a0 并保存 TRAPFRAME，为下一次 trap 做准备，并使用 **sret** 返回用户空间。

`trampoline.S`

```assembly
	# 用户空间和内核空间切换的代码
    # tramppline.S被映射到用户空间和内核空间的相同的虚拟地址TRAMPOLINE
	# 这样以便于页表切换时这段代码能够继续完整执行
	# kernel.ld链接器程序会使得trampoline.S对齐成一页边界大小
	.section trampsec
.globl trampoline
trampoline:
.align 4
.globl uservec
uservec:    
	# trap.c设置stvec寄存器指向uservec，这样来自用户空间的各种traps先从这里开始执行
	# 此时处于监督者模式，但是还没有切换到内核页表
	# sscratch寄存器指向进程的p->trapframe（被映射到用户空间的TRAPFRAME虚拟地址处）
   
        # a0指向TRAPFRAME地址，sscratch保存a0的值
        csrrw a0, sscratch, a0

        # 保存现场，把用户寄存器的值保存在TRAPFRAME中以便后续恢复现场
        sd ra, 40(a0)# 返回地址
        sd sp, 48(a0)# 栈指针
        sd gp, 56(a0)# 全局指针
        sd tp, 64(a0)# CPU-ID
        sd t0, 72(a0)
        sd t1, 80(a0)
        sd t2, 88(a0)
        sd s0, 96(a0)# frame pointer
        sd s1, 104(a0)
        sd a1, 120(a0)
        sd a2, 128(a0)
        sd a3, 136(a0)
        sd a4, 144(a0)
        sd a5, 152(a0)
        sd a6, 160(a0)
        sd a7, 168(a0)
        sd s2, 176(a0)
        sd s3, 184(a0)
        sd s4, 192(a0)
        sd s5, 200(a0)
        sd s6, 208(a0)
        sd s7, 216(a0)
        sd s8, 224(a0)
        sd s9, 232(a0)
        sd s10, 240(a0)
        sd s11, 248(a0)
        sd t3, 256(a0)
        sd t4, 264(a0)
        sd t5, 272(a0)
        sd t6, 280(a0)

		# save the user a0 in p->trapframe->a0
        # 保存原来a0寄存器的值
        csrr t0, sscratch
        sd t0, 112(a0)

        # restore kernel stack pointer from p->trapframe->kernel_sp
        # 进入内核态之前设置sp指向p->trapframe->kernel_sp
        ld sp, 8(a0)

        # make tp hold the current hartid, from p->trapframe->kernel_hartid
        # 进入内核态之前设置tp为p->trapframe->kernel_hartid
        ld tp, 32(a0)

        # load the address of usertrap(), p->trapframe->kernel_trap
        # 加载usertrap函数入口
        ld t0, 16(a0)

        # restore kernel page table from p->trapframe->kernel_satp
        # t1寄存器保存p->trapframe->kernel_satp（内核页表）
        ld t1, 0(a0)
        # 切换页表为内核页表
        csrw satp, t1
        # 页表发生改变，需要刷新TLB
        sfence.vma zero, zero

        # a0 is no longer valid, since the kernel page
        # table does not specially map p->tf.
        # a0(p->trapframe)不再有效，因为现在切换成了内核页表，而内核页表中没有p->trapframe页映射

        # jump to usertrap(), which does not return
        # 跳转到usertrap函数内
        jr t0

.globl userret
userret:
        # userret(TRAPFRAME, pagetable)从内核空间切换回用户空间
        # a0: TRAPFRAME, in user page table.
        # a1: user page table, for satp.

        # switch to the user page table.
        # 切换到用户页表，刷新TLB
        csrw satp, a1
        sfence.vma zero, zero

        # put the saved user a0 in sscratch, so we
        # can swap it with our a0 (TRAPFRAME) in the last step.
        # 将trapframe中保存的a0复制到sscratch中，为最后a0与TRAPFRAME交换做准备
        ld t0, 112(a0)
        csrw sscratch, t0

        # restore all but a0 from TRAPFRAME
        # 恢复用户寄存器的值（除了a0）
        ld ra, 40(a0)
        ld sp, 48(a0)
        ld gp, 56(a0)
        ld tp, 64(a0)
        ld t0, 72(a0)
        ld t1, 80(a0)
        ld t2, 88(a0)
        ld s0, 96(a0)
        ld s1, 104(a0)
        ld a1, 120(a0)
        ld a2, 128(a0)
        ld a3, 136(a0)
        ld a4, 144(a0)
        ld a5, 152(a0)
        ld a6, 160(a0)
        ld a7, 168(a0)
        ld s2, 176(a0)
        ld s3, 184(a0)
        ld s4, 192(a0)
        ld s5, 200(a0)
        ld s6, 208(a0)
        ld s7, 216(a0)
        ld s8, 224(a0)
        ld s9, 232(a0)
        ld s10, 240(a0)
        ld s11, 248(a0)
        ld t3, 256(a0)
        ld t4, 264(a0)
        ld t5, 272(a0)
        ld t6, 280(a0)

		# restore user a0, and save TRAPFRAME in sscratch
        # 对a0和sscratch做最后的交换，恢复用户a0并保存TRAPFRAME，为下一次 trap 做准备
        csrrw a0, sscratch, a0
        
        # return to user mode and user pc.
        # usertrapret() set up sstatus and sepc.
        # 返回到用户模式下并且沿着发生异常的下一条指令继续执行
        sret

```

`trap.c`

```c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];
// in kernelvec.S, calls kerneltrap()
void kernelvec();
extern int devintr();
void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
// 设置内核空间的异常/陷入处理程序入口
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
// 处理来自用户空间的中断、异常、系统调用
//
void
usertrap(void)
{
  int which_dev = 0;

  // SPP 位表示 trap 是来自用户模式还是监督者模式，并控制sret 返回到什么模式
  // 只能处理来自用户空间的中断、异常、系统调用
  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  // 我们现在已经进入内核空间了，在内核中再发生的 trap 将由 kernelvec 处理
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  // 它保存了 sepc(用户 PC)，这也是因为 usertrap 中可能会有一个进程切换，导致 sepc 被覆盖
  p->trapframe->epc = r_sepc();
  
  // 系统调用交给syscall处理
  if(r_scause() == 8){
    if(p->killed)
      exit(-1);
    // 返回地址为ecall指令的下一个指令
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    // 开启设备中断
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    // 发生异常，内核杀死异常进程
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  // 时钟中断处理，让出CPU重新调度
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
// 开始回到用户空间，首先设置 RISC-V 控制寄存器，为以后用户空间 trap 做准备
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  // 返回用户空间时关闭中断
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  // 改变 stvec 指向 uservec
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  // 准备 uservec 所依赖的 trapframe 字段
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap; 
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  // 在sret前设置sstatus寄存器，确保返回用户模式
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  // 将 sepc 设置为先前保存的用户程序计数器
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  // 用户进程页表
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  // 调用 userret
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  // this is a supervisor external interrupt, via PLIC.
  // 经PLIC而来的外部设备中断
  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // irq indicates which device interrupted.
    // IRQ表明了具体哪个设备导致的中断
    int irq = plic_claim();
    if(irq == UART0_IRQ){
      // 键盘串口中断
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      // 磁盘中断
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    // PLIC只允许同一时刻一个设备至多被打断一次
    if(irq)
      plic_complete(irq);
    return 1;
  } 
  // software interrupt from a machine-mode timer interrupt,
  // forwarded by timervec in kernelvec.S
  // 来自机器模式的时钟中断的软件中断
  else if(scause == 0x8000000000000001L){
    if(cpuid() == 0){
      clockintr();
    } 
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);
    return 2;
  } 
  else {
    return 0;
  }
}
```

#### 4.3系统调用函数执行

第 2 章以 initcode.S 调用 exec 系统调用结束。让我们来看看用户调用是如何在内核中实现 exec 系统调用的。

用户代码将 **exec** 的参数放在寄存器 **a0** 和 **a1** 中，并将系统调用号放在 **a7** 中。系统调用号与函数指针表 **syscalls** 数组中的项匹配。**ecall** 指令进入内核，执行**uservec**、**usertrap**，然后执行 **syscall**，就像我们上面看到的那样。

**syscall**从 **trapframe** 中的 a7 中得到系统调用号，并其作为索引在**syscalls** 查找相应函数。对于第一个系统调用 **exec**，a7 将为 **SYS_exec**，这会让 **syscall** 调用 **exec** 的实现函数 **sys_exec**。

当系统调用函数返回时，**syscall** 将其返回值记录在 **p->trapframe->a0** 中。用户空间的 **exec()**将会返回该值，因为 RISC-V 上的 C 调用通常将返回值放在 **a0** 中。系统调用返回负数表示错误，0 或正数表示成功。如果系统调用号无效，**syscall** 会打印错误并返回 1。

内核的系统调用实现需要找到用户代码传递的参数。因为用户代码调用系统调用的包装函数，参数首先会存放在寄存器中，这是 C 语言存放参数的惯例位置。内核 trap 代码将用户寄存器保存到当前进程的 **trapframe** 中，内核代码可以在那里找到它们。函数 **argint**、**argaddr** 和 **argfd** 从 **trapframe** 中以整数、指针或文件描述符的形式检索第 n 个系统调用参数。它们都调用 **argraw** 在 **trapframe** 中检索相应的数据。

一些系统调用传递指针作为参数，而内核必须使用这些指针来读取或写入用户内存。例如，**exec** 系统调用会向内核传递一个指向用户空间中的字符串的指针数组。这些指针带来了两个挑战。首先，用户程序可能是错误的或恶意的，可能会传递给内核一个无效的指针或一个旨在欺骗内核访问内核内存而不是用户内存的指针。第二，xv6 内核页表映射与用户页表映射不一样，所以内核不能使用普通指令从用户提供的地址加载或存储。

内核实现了安全地将数据复制到用户提供的地址或从用户提供的地址复制数据的函数。例如 fetchstr。文件系统调用，如 exec，使用 **fetchstr** 从用户空间中检索字符串文件名参数，**fetchstr** 调用 **copyinstr** 来做这些困难的工作。

**copyinstr** 将用户页表 **pagetable** 中的虚拟地址 **srcva** 复制到 **dst**，需指定最大复制字节数。它使用 **walkaddr**（调用 **walk** 函数）在软件中模拟分页硬件的操作，以确定 **srcva** 的物理地址 **pa0**。**walkaddr** 检查用户提供的虚拟地址是否是进程用户地址空间的一部分，所以程序不能欺骗内核读取其他内存。类似的函数 **copyout**，可以将数据从内核复制到用户提供的地址。

`syscall.c`

```c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
// 安全地将数据复制到用户提供的地址或从用户提供的地址复制数据
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  int err = copyinstr(p->pagetable, buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}

// 在 trapframe 中检索相应的数据(第n个系统调用参数)
static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
// 以整数的形式检索第n个系统调用参数
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  // 以指针的形式检索第n个系统调用参数
  *ip = argraw(n);
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  // 以字符串的形式检索第n个系统调用参数
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  return fetchstr(addr, buf, max);
}

extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);

static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();
  
  //从 trapframe 中的 a7 中得到系统调用号，并其作为索引在 syscalls 查找相应函数
  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // 当系统调用函数返回时，syscall 将其返回值记录在 p->trapframe->a0 中
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

#### 4.4内核空间的陷入

Xv6 对异常的响应是相当固定：如果一个异常发生在用户空间，内核就会杀死故障进程。如果一个异常发生在内核中，内核就会 **panic**。真正的操作系统通常会以更有趣的方式进行响应。

举个例子，许多内核使用页面故障来实现**写时复制COW**fork。要解释写时复制 fork，可以想一 想 xv6 的 fork。fork 通过调用uvmcopy为子进程分配物理内存，并将父进程的内存复制到子程序中，使子进程拥有与父进程相同的内存内容。如果子进程和父进程能够共享父进程的物理内存，效率会更高。然而，直接实现这种方法是行不通的，因为父进程和子进程对共享栈和堆的写入会中断彼此的执行。

通过使用写时复制 fork，可以让父进程和子进程安全地共享物理内存，通过页面故障来实现。当 CPU 不能将虚拟地址翻译成物理地址时，CPU 会产生一个页面故障异常(page-fault exception)。 RISC-V 有三种不同的页故障：load 页故障（当加载指令不能翻译其虚拟地址时）、store 页故障（当存储指令不能翻译其虚拟地址时）和指令页故障（当指令的地址不能翻译时）。**scause** 寄存器中的值表示页面故障的类型，**stval** 寄存器中包含无法翻译的地址。

**COW** fork 中的基本设计是父进程和子进程最初共享所有的物理页面，但将它们映射设置为只读。因此，当子进程或父进程执行 store 指令时，RISC-V CPU 会引发一个页面故障异常。作为对这个异常的响应，内核会拷贝一份包含故障地址的页。然后将一个副本的读/写映射在子进程地址空间，另一个副本的读/写映射在父进程地址空间。更新页表后，内核在引起故障的指令处恢复故障处理。因为内核已经更新了相关的 PTE，允许写入，所以现在故障指令将正常执行。

这个 COW 设计对 fork 很有效，因为往往子程序在 fork 后立即调用 exec，用新的地址空间替换其地址空间。在这种常见的情况下，子程序只会遇到一些页面故障，而内核可以避免进行完整的复制。此外，COW fork 是透明的：不需要对应用程序进行修改，应用程序就能受益。

页表和页故障的结合，将会有更多种有趣的可能性的应用。另一个被广泛使用的特性叫做**懒分配 (lazy allocation)**，它有两个部分。首先，当一个应用程序调用 **sbrk** 时，内核会增长地址空间，但在页表中把新的地址标记为无效。第二，当这些新地址中的一个出现页面故障时，内核分配物理内存并将其映射到页表中。由于应用程序经常要求获得比他们需要的更多的内存，所以懒分配是一个胜利：内核只在应用程序实际使用时才分配内存。像 COW fork一样，内核可以对应用程序透明地实现这个功能。

另一个被广泛使用的利用页面故障的功能是从**磁盘上分页(paging from disk)**。如果应用程序需要的内存超过了可用的物理 RAM，内核可以交换出一些页：将它们写入一个存储设备，比如磁盘，并将其 PTE 标记为无效。如果一个应用程序读取或写入一个被换出到磁盘的页，CPU 将遇到一个页面故障。内核就可以检查故障地址。如果该地址属于磁盘上的页面，内核就会分配一个物理内存的页面，从磁盘上读取页面到该内存，更新 PTE 为有效并引用该内存，然后恢复应用程序。为了给该页腾出空间，内核可能要交换另一个页。这个特性不需要对应用程序进行任何修改，如果应用程序具有引用的位置性（即它们在任何时候都只使用其内存的一个子集），这个特性就能很好地发挥作用。

其他结合分页和分页错误异常的功能包括自动扩展堆栈和内存映射文件。

### 第五章：中断和设备驱动

**驱动**是操作系统中管理特定设备的代码，他有如下功能：1、配置设备相关的硬件，2、告诉设备需要怎样执行，3、处理设备产生的中断，4、与等待设备 I/O 的进程进行交互。驱动程序的代码写起来可能很棘手，因为驱动程序与它所管理的设备会同时执行。此外，驱动程序编写人员必须了解设备的硬件接口，但硬件接口可能是很复杂的，而且文档不够完善。

需要操作系统关注的设备通常可以被配置为产生中断，这是 trap 的一种类型。内核 trap 处理代码可以知道设备何时引发了中断，并调用驱动的中断处理程序；在 xv6 中，这个处理发生在 **devintr**中。

许多设备驱动程序在两个 context 中执行代码：上半部分(**top half**)在进程的内核线程中运行，下半部分(**bottom half**)在中断时执行。上半部分是通过系统调用，如希望执行 I/O 的read 和 write。这段代码可能会要求硬件开始一个操作（比如要求磁盘读取一个块）；然后代码等待操作完成。最终设备完成操作并引发一个中断。驱动程序的中断处理程序，作为**下半**部分，推算出什么操作已经完成，如果合适的话，唤醒一个等待该操作的进程，并告诉硬件执行下一个操作。

#### 5.1控制台输入

控制台驱动**(console.c)**是驱动结构的一个简单说明。控制台驱动通过连接到 RISC-V 上的 UART 串行端口硬件，接受输入的字符。控制台驱动程序每次累计一行输入，处理特殊的输入字符，如退格键和 control-u。用户进程，如 shell，使用 **read** 系统调用从控制台获取输入行。当你在 QEMU 中向 xv6 输入时，你的按键会通过 QEMU 的模拟 UART 硬件传递给xv6。

与驱动交互的 UART 硬件是由 QEMU 仿真的 16550 芯片。在真实的计算机上，16550将管理一个连接到终端或其他计算机的 RS232 串行链接。当运行 QEMU 时，它连接到你的键盘和显示器上。

UART 硬件在软件看来是一组**内存映射**的控制寄存器。也就是说，有一些 RISC-V 硬件的物理内存地址会连接到 UART 设备，因此加载和存储与设备硬件而不是 RAM 交互。UART的内存映射地址0x10000000 开始，即 **UART0**。这里有一些 UART控制寄存器，每个寄存器的宽度是一个字节。例如，**LSR** 寄存器中一些位表示是否有输入字符在等待软件读取。这些字符（如果有的话）可以从 **RHR** 寄存器中读取。每次读取一个字符，UART 硬件就会将其从内部等待字符的 FIFO中删除，并在 FIFO 为空时清除 **LSR** 中的就绪位。UART 传输硬件在很大程度上是独立于接收硬件的，如果软件向 **THR** 写入一个字节，UART 就会发送该字节。

Xv6 的 **main** 调用 **consoleinit**来初始化 UART 硬件。这段代码配置了 UART，当 UART 接收到一个字节的输入时，就产生一个接收中断，当 UART 每次完成发送一个字节的输出时，产生一个**传输完成(transmit complete)**中断。

`console.c的consoleinit`

```c
void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
```

`uart.c的uartinit`

```c
void
uartinit(void)
{
  // disable interrupts.
  // 初始化UART硬件过程中需要先关闭UART硬件中断
  WriteReg(IER, 0x00);
  // special mode to set baud rate.
  // 开启LCR_BAUD_LATCH模式设置波特率
  WriteReg(LCR, LCR_BAUD_LATCH);
  // LSB for baud rate of 38.4K.
  WriteReg(0, 0x03);
  // MSB for baud rate of 38.4K.
  WriteReg(1, 0x00);
  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  // 设置为LCR_EIGHT_BITS模式
  WriteReg(LCR, LCR_EIGHT_BITS);
  // reset and enable FIFOs.
  // 重置并开启字符FIFO队列
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
  // enable transmit and receive interrupts.
  // 初始化UART硬件结束后开启对应的中断（包括接受和发送中断）
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);
  initlock(&uart_tx_lock, "uart");
}
```

`printf.c`

```c
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
// printf输出的内容都是字符数据，即使是整数也被拆分为个位、十位、百位等字符一个个输出(注意负数要额外输出一个负号)
// 每次调用printf真正打印字符之前都需要先acquire(&pr.lock),输出完毕再释放锁，避免交叉打印
// 交互流程：printf函数通过consputc与控制台驱动console.c交互-->console.c通过uartputc_sync与UART硬件交互-->屏幕显示字符
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
}
```

`console.c的consputc`

```c
//
// send one character to the uart.
// called by printf, and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}
```

`uart.c的uartputc_sync`

```c
// UART 硬件在软件看来是一组内存映射的控制寄存器。
// 也就是说，有一些 RISC-V 硬件的物理内存地址会连接到 UART 设备，因此加载和存储与设备硬件而不是 RAM 交互.
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))
#define LSR 5                 // line status register
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send

// alternate version of uartputc() that doesn't 
// use interrupts, for use by kernel printf() and
// to echo characters. it spins waiting for the uart's
// output register to be empty.
void
uartputc_sync(int c)
{
  push_off();
  if(panicked){
    for(;;)
      ;
  }
  // wait for Transmit Holding Empty to be set in LSR.
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);
  pop_off();
}
```

xv6 shell 通过 **init.c**打开的文件描述符从控制台读取。**consoleread** 等待输入的到来(通过中断)，输入会被缓冲在 **cons.buf**，然后将输入复制到用户空间，再然后(在一整行到达后)返回到用户进程。如果用户还没有输入完整的行，任何 **read** 进程将在 **sleep**调用中等待。

当用户键入一个字符时，UART 硬件向 RISC-V 抛出一个中断，从而激活 xv6 的 **trap**处理程序。trap 处理程序调用 devintr，它查看 RISC-V 的 **scause** 寄存器，发现中断来自一个外部设备。然后它向一个叫做 PLIC的硬件单元询问哪个设备中断了。如果是 UART，**devintr** 调用 **uartintr**。

**uartintr**从 **UART** 硬件中读取在等待的输入字符，并将它们交给**consoleintr**；它不会等待输入字符，因为以后的输入会引发一个新的中断。**consoleintr** 的工作是将中输入字符积累 **cons.buf** 中，直到有一行字符。 **consoleintr**会特别处理退格键和其他一些字符。当一个新行到达时，**consoleintr** 会唤醒一个等待的**consoleread**（如果有的话）。

一旦被唤醒，**consoleread** 将会注意到 **cons.buf** 中的完整行，并将其将其复制到用户空间，并返回（通过系统调用）到用户空间。

`trap.c的设备中断devintr`

```c
// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  // this is a supervisor external interrupt, via PLIC.
  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // irq indicates which device interrupted.
    int irq = plic_claim();
    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);
    return 1;
  } 
  // software interrupt from a machine-mode timer interrupt,
  // forwarded by timervec in kernelvec.S
  else if(scause == 0x8000000000000001L){
    if(cpuid() == 0){
      clockintr();
    } 
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);
    return 2;
  } 
  else {
    return 0;
  }
}
```

`uart.c的uartintr`

```c
#define LSR 5                 // line status register
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send

// read one input character from the UART.
// return -1 if none is waiting.
int
uartgetc(void)
{
  if(ReadReg(LSR) & 0x01){
    // input data is ready.
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from trap.c.
void
uartintr(void)
{
  // read and process incoming characters.
  while(1){
    // 读取来自UART的一个输入字符
    int c = uartgetc();
    if(c == -1)
      break;
    // 字符追加到cons.buffer并唤醒可能休眠的consoleread()
    consoleintr(c);
  }

  // send buffered characters.
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}
```

`console.c的consoleintr`

```c
struct {
  // 串行化访问console硬件，避免乱序的输出
  struct spinlock lock;  
  #define INPUT_BUF 128
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;

//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // Print process list.
    procdump();
    break;
  case C('U'):  // Kill line.
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // Backspace
  case '\x7f':
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF){
      c = (c == '\r') ? '\n' : c;
      // echo back to the user.
      // 字符回显到屏幕！！
      consputc(c);
      // store for consumption by consoleread().
      // 字符添加到cons.buffer中！！
      cons.buf[cons.e++ % INPUT_BUF] = c;
      if(c == '\n' || c == C('D') || cons.e == cons.r+INPUT_BUF){
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        // 一整行或者文件结束符到达后唤醒休眠的consoleread()
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
```

`console.c的consoleread`

```c
//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
// 读取键盘的数据并拷贝至目标地址（实际上数据最终被放入shell的buf中）
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while(cons.r == cons.w){
      if(myproc()->killed){
        release(&cons.lock);
        return -1;
      }
      // cons.buf为空，则休眠等待中断处理函数把数据放入cons.buffer
      sleep(&cons.r, &cons.lock);
    }

    c = cons.buf[cons.r++ % INPUT_BUF];

    if(c == C('D')){  // end-of-file
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      // 读取到一整行的末尾则结束
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}
```

#### 5.2控制台输出

向控制台写数据的 **write** 系统调用最终会到达 **uartputc**。设备驱动维护了一个输出缓冲uart_tx_buf，这样写入过程就不需要等待 UART 完成发送；相反，**uartputc** 将每个字符追加到缓冲区，调用uartstart来启动设备发送(如果还没有的话)，然后返回。**uartputc** 只有在缓冲区满的时候才会等待。

每次 UART 发送完成一个字节，它都会产生一个中断。**uartintr** 调用 **uartstart**，**uartintr**检查设备是否真的发送完毕，并将下一个缓冲输出字符交给设备，每当 UART 发送完一个字节，就会产生一个中断。因此，如果一个进程向控制台写入多个字节，通常第一个字节将由uartputc调用 uartstart 发送，其余的缓冲字节将由 uartintr 调用 uartstart 发送，因为发送完成中断到来。

有一个通用模式需要注意，`设备活动和进程活动需要解耦`，这将通过`缓冲和中断`来实现。==控制台驱动程序可以处理输入，即使没有进程等待读取它，随后的读取将看到输入。同样，进程可以发送输出字节，而不必等待设备==。`这种解耦可以通过允许进程与设备 I/O 并发执行来提高性能`，当设备速度很慢如 UART或需要立即关注（如打印键入的字节）时，这种解耦尤为重要。这个 idea 有时被称为 **I/O 并发**。

5.1的代码完成了字符从键盘的中断输入并回显，那么用户空间的printf函数如何打印字符的呢？

`user/printf.c`

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include <stdarg.h>
static char digits[] = "0123456789ABCDEF";

static void
putc(int fd, char c)
{
  // 重点在这里，系统调用write
  write(fd, &c, 1);
}

static void
printint(int fd, int xx, int base, int sgn)
{
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(fd, buf[i]);
}

static void
printptr(int fd, uint64 x) {
  int i;
  putc(fd, '0');
  putc(fd, 'x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    putc(fd, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  int c, i, state;

  state = 0;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(fd, va_arg(ap, int), 10, 1);
      } else if(c == 'l') {
        printint(fd, va_arg(ap, uint64), 10, 0);
      } else if(c == 'x') {
        printint(fd, va_arg(ap, int), 16, 0);
      } else if(c == 'p') {
        printptr(fd, va_arg(ap, uint64));
      } else if(c == 's'){
        s = va_arg(ap, char*);
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        putc(fd, va_arg(ap, uint));
      } else if(c == '%'){
        putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(fd, '%');
        putc(fd, c);
      }
      state = 0;
    }
  }
}

void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
}
```

`sysfile.c的sys_write`

```c
uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;
  //解析系统调用参数
  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}
```

`file.c的filewrite`

```c
// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  // 检查文件读写权限
  if(f->writable == 0)
    return -1;

  // 根据文件类型选择不同的写入函数
  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    // Linux中一切皆文件包括设备，这里的思想也一样：对于文件的读写深入底层其实是不同设备的读写接口
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}
```

`console.c的consolewrite`

```c
//
// user write()s to the console go here.
// user_src=1表示src是用户空间的虚拟地址，src表示字符数据的地址，n表示要写入的字符数
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;
  acquire(&cons.lock);
  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    // 把一个字符写入输出缓冲区
    uartputc(c);
  }
  release(&cons.lock);
  return i;
}
```

`proc.c的either_copyin`

```c
// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    // 用户空间的数据需要通过copyin使用用户页表拷贝数据
    return copyin(p->pagetable, dst, src, len);
  } else {
    // 内核空间的数据可以直接通过memmove复制数据(因为此时系统就处于内核态)
    memmove(dst, (char*)src, len);
    return 0;
  }
}
```

`uart.c的uartputc`

```c
// the transmit output buffer.
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
int uart_tx_w; // write next to uart_tx_buf[uart_tx_w++]
int uart_tx_r; // read next from uart_tx_buf[uar_tx_r++]

// add a character to the output buffer and tell the
// UART to start sending if it isn't already.
// blocks if the output buffer is full.
// because it may block, it can't be called
// from interrupts; it's only suitable for use
// by write().
void
uartputc(int c)
{
  acquire(&uart_tx_lock);

  if(panicked){
    for(;;)
      ;
  }
  // 外层的while循环防止假唤醒
  while(1){
    if(((uart_tx_w + 1) % UART_TX_BUF_SIZE) == uart_tx_r){
      // buffer is full.
      // wait for uartstart() to open up space in the buffer.
      // 循环缓冲区满了，则休眠在最近等待发送的字符位置uart_tx_r处等待uartstart的唤醒
      sleep(&uart_tx_r, &uart_tx_lock);
    } else {
      // 正常情况下输出缓冲区有空间，字符直接追加写入即可
      uart_tx_buf[uart_tx_w] = c;
      uart_tx_w = (uart_tx_w + 1) % UART_TX_BUF_SIZE;
      // 准备好数据就可以发送
      uartstart();
      release(&uart_tx_lock);
      return;
    }
  }
}
```

`uart.c的uartstart`

```c
// if the UART is idle, and a character is waiting
// in the transmit buffer, send it.
// caller must hold uart_tx_lock.
// called from both the top- and bottom-half.
void
uartstart()
{
  while(1){
    if(uart_tx_w == uart_tx_r){
      // transmit buffer is empty.
      // 发送缓冲区为空直接退出
      return;
    }
    if((ReadReg(LSR) & LSR_TX_IDLE) == 0){
      // the UART transmit holding register is full,
      // so we cannot give it another byte.
      // it will interrupt when it's ready for a new byte.
      // UART传输寄存器有内容，我们不能覆盖，这个函数会在UART硬件传输完上一个字符准备新字符输出时被uartintr中断调用
      return;
    } 
    int c = uart_tx_buf[uart_tx_r];
    uart_tx_r = (uart_tx_r + 1) % UART_TX_BUF_SIZE;
    // maybe uartputc() is waiting for space in the buffer.
    // 发送一个字符后，唤醒可能休眠的uartputc()
    wakeup(&uart_tx_r);
    
    WriteReg(THR, c);
  }
}
```

`发送的关键：uartintr`

```c
// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from trap.c
// uartintr不仅处理键盘的字符输入中断，还需要处理UART硬件的字符输出完毕中断
void
uartintr(void)
{
  // read and process incoming characters.
  while(1){
    // 读取来自UART的一个输入字符
    int c = uartgetc();
    if(c == -1)
      break;
    // 字符追加到cons.buffer并唤醒可能休眠的consoleread()
    consoleintr(c);
  }

  // send buffered characters.
  // UART传输完毕中断，尝试传输新的uart_tx_buf字符
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}
```

#### 5.3设备并发

你可能已经注意到在 **consoleread** 和 **consoleintr** 中会调用 **acquire**。**acquire** 调用会获取一个锁，保护控制台驱动的数据结构不被并发访问。这里有三个并发风险：不同 CPU 上的两个进程可能会同时调用 **consoleread**；硬件可能会在一个 CPU 正在执行 consoleread 时，向该 CPU 抛出一个控制台（实际上是 UART）中断；硬件可能会在 consoleread 执行时向另一个 CPU 抛出一个控制台中断。第 6 章探讨锁如何在这些情况下提供帮助。

需要关注驱动并发安全的另一个原因是，一个进程可能正在等待来自设备的输入，但是此时该进程已经没有在运行（被切换）。因此，中断处理程序不允许知道被中断的进程或代码。例如，一个中断处理程序不能安全地用当前进程的页表调用 **copyout**。中断处理程序通常只做相对较少的工作（例如，只是将输入数据复制到缓冲区），并唤醒**上半部分**代码来做剩下的工作。

#### 5.4时钟中断

Xv6 使用定时器中断来维护它的时钟，并使它能够切换正在运行的进程；**usertrap** 和**kerneltrap** 中的 **yield** 调用会导致这种切换。每个 RISC-V CPU 的时钟硬件都会抛出时钟中断。Xv6 对这个时钟硬件进行编程，使其定期中断相应的 CPU。

RISC-V 要求在机器模式下处理定时器中断，而不是监督者模式。RISCV 机器模式执行时没有分页，并且有一套单独的控制寄存器，因此在机器模式下运行普通的 xv6 内核代码是不实用的。因此，xv6 对定时器中断的处理与上面谈到的 trap 机制完全分离了。

在 main 执行之前的 **start.c**，是在机器模式下执行的，设置了接收定时器中断。一部分工作是对 **CLINT** 硬件（**core-local interruptor**）进行编程，使其每隔一定时间产生一次中断。另一部分是设置一个类似于 **trapframe** 的 **scratch** 区域，帮助定时器中断处理程序保存寄存器和 **CLINT** 寄存器的地址。最后，**start** 将 **mtvec** 设置为**timervec**，启用定时器中断。

`start.c的timerinit`

```c
// set up to receive timer interrupts in machine mode,
// which arrive at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  // 首先读出 CPU 的 ID
  int id = r_mhartid();
  // ask the CLINT for a timer interrupt.
  // 设置中断时间间隔，这里设置的是 0.1 秒
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;
  // prepare information in scratch[] for timervec.
  // scratch[0..3] : space for timervec to save registers.
  // scratch[4] : address of CLINT MTIMECMP register.
  // scratch[5] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &mscratch0[32 * id];
  scratch[4] = CLINT_MTIMECMP(id);
  scratch[5] = interval;
  w_mscratch((uint64)scratch);
  // set the machine-mode trap handler.
  // 设置中断处理函数
  w_mtvec((uint64)timervec);
  // enable machine-mode interrupts.
  // 打开中断
  w_mstatus(r_mstatus() | MSTATUS_MIE);
  // enable machine-mode timer interrupts.
  // 打开时钟中断
  w_mie(r_mie() | MIE_MTIE);
}
```

定时器中断可能发生在用户或内核代码执行的任何时候；内核没有办法在关键操作中禁用定时器中断。因此，定时器中断处理程序必须以保证不干扰被中断的内核代码的方式进行工作。基本策略是处理程序要求 RISC-V 引发一个软件中断并立即返回。RISC-V 用普通的trap 机制将软件中断传递给内核，并允许内核禁用它们。处理定时器中断产生的软件中断的代码可以在 **devintr**中看到。

`trap.c的devintr`

```c
struct spinlock tickslock;
uint ticks;

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();
  // this is a supervisor external interrupt, via PLIC.
  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // irq indicates which device interrupted.
    int irq = plic_claim();
    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }
    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);
    return 1;
  } 
  // software interrupt from a machine-mode timer interrupt,
  // forwarded by timervec in kernelvec.S
  else if(scause == 0x8000000000000001L){
    if(cpuid() == 0){
      clockintr();
    } 
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);
    return 2;
  } 
  else {
    return 0;
  }
}
```

机器模式的定时器中断向量是 **timervec**。它在 **start** 准备的**scratch** 区域保存一些寄存器，告诉 **CLINT** 何时产生下一个定时器中断，使 RISC-V 产生一个软件中断，恢复寄存器，然后返回。在定时器中断处理程序中没有 C 代码。

```assembly
.globl timervec
.align 4
timervec:
        # start.c has set up the memory that mscratch points to:
        # scratch[0,8,16] : register save area.
        # scratch[32] : address of CLINT's MTIMECMP register.
        # scratch[40] : desired interval between interrupts.
        csrrw a0, mscratch, a0
        # 恢复寄存器
        sd a1, 0(a0)
        sd a2, 8(a0)
        sd a3, 16(a0)
        # schedule the next timer interrupt
        # by adding interval to mtimecmp.
        # 告诉 CLINT 何时产生下一个定时器中断
        ld a1, 32(a0) # CLINT_MTIMECMP(hart)
        ld a2, 40(a0) # interval
        ld a3, 0(a1)
        add a3, a3, a2
        sd a3, 0(a1)
        # raise a supervisor software interrupt.
        # 使 RISC-V 产生一个软件中断
		li a1, 2
        csrw sip, a1
		# 保存寄存器
        ld a3, 16(a0)
        ld a2, 8(a0)
        ld a1, 0(a0)
        csrrw a0, mscratch, a0
        mret
```

usertrap()的最后几行代码：

```c
// give up the CPU if this is a timer interrupt.
// 时钟中断处理，让出CPU重新调度 
if(which_dev == 2)
  yield();
usertrapret();
```

`proc.c中有关进程切换的代码`

```c
struct cpu cpus[NCPU];
struct proc proc[NPROC];
int nextpid = 1;
struct spinlock pid_lock;

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  // 必须持有p->lock
  if(!holding(&p->lock))
    panic("sched p->lock");
  // 禁止进程切换前持有其它锁（避免死锁）
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");
  // 保存mycpu()->intena
  intena = mycpu()->intena;
  // 保存进程的寄存器组到p->context中，下次再回到当前进程会紧接着运行
  // 切换到CPU的scheduler()进行进程调度
  swtch(&p->context, &mycpu()->context);
  // 恢复mycpu()->intena
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  // 拿到p->lock后不能在这里立刻释放，因为此时可能有其他核心的调度线程会发现该线程可运行，此时多个核心运行一个线程，线程栈瞬间崩溃
  // 只有在scheduler内核调度线程中才能释放p->lock，因为此时线程使用的是scheduler的内核栈，用户线程已经完全放弃CPU使用
  sched();
  release(&p->lock);
}
```

`swtch.S`

```assembly
# 上下文切换
# void swtch(struct context *old, struct context *new);
# Save current registers in old. Load from new.	
.globl swtch
swtch:
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)   
        ret
```

`proc.c的scheduler`

```c
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    // CPU核心通过设置SSTATUS寄存器（SSTATUS_SIE）开启设备中断
    intr_on();
    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        // 切换到选中的进程执行
        swtch(&c->context, &p->context);
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      // 这一步很重要，每次通过shced()-->scheduler()-->swtch()执行，到达这里会释放之前保	  持的p->lock
      // 一个具体的例子就是sleep()沿着函数调用到达这里后释放了p->lock，wakeup()可以获得p-	   >lock并唤醒p
      release(&p->lock);
    }
    if(found == 0) {
      intr_on();
      asm volatile("wfi");
    }
  }
}
```

#### 5.5补充

UART 驱动器通过读取 UART 控制寄存器，一次读取一个字节的数据；这种模式被称为编程 I/O，因为软件在控制数据移动。程序化 I/O 简单，但速度太慢，无法在高数据速率下使用。需要高速移动大量数据的设备通常使用**直接内存访问(direct memory access, DMA)**。DMA 设备硬件直接将传入数据写入 RAM，并从 RAM 中读取传出数据。现代磁盘和网络设备都使用 DMA。DMA 设备的驱动程序会在 RAM 中准备数据，然后使用对控制寄存器的一次写入来告诉设备处理准备好的数据。

当设备在不可预知的时间需要关注时，中断是很有用的，而且不会太频繁。但中断对 CPU的开销很大。因此，高速设备，如网络和磁盘控制器，使用了减少对中断需求的技巧。其中一个技巧是对整批传入或传出的请求提出一个单一的中断。另一个技巧是让驱动程序完全禁用中断，并定期检查设备是否需要关注。这种技术称为**轮询（polling）**。如果设备执行操作的速度非常快，轮询是有意义的，但如果设备大部分时间处于空闲状态，则会浪费 CPU 时间。一些驱动程序会根据当前设备的负载情况，在轮询和中断之间动态切换。

UART 驱动首先将输入的数据复制到内核的缓冲区，然后再复制到用户空间。这在低数据速率下是有意义的，但对于那些快速生成或消耗数据的设备来说，这样的双重拷贝会大大降低性能。一些操作系统能够直接在用户空间缓冲区和设备硬件之间移动数据，通常使用DMA。

### 第六章：锁

大多数内核，包括 xv6，都会交错执行多个任务。多处理器硬件可以交错执行任务：具有多个 CPU 独立执行的计算机，如 xv6 的 RISC-V。这些多个 CPU 共享物理 RAM，xv6 利用共享来维护所有 CPU 读写的数据结构。这种共享带来了一种可能性，即一个 CPU 读取一个数据结构，而另一个 CPU 正在中途更新它，甚至多个 CPU 同时更新同一个数据；如果不仔细设计，这种并行访问很可能产生不正确的结果或破坏数据结构。即使在单处理器上，内核也可能在多个线程之间切换 CPU，导致它们的执行交错。最后，如果中断发生的时间不对，一个设备中断处理程序可能会修改与一些可中断代码相同的数据，从而破坏数据。并发一词指的是由于多处理器并行、线程切换或中断而导致多个指令流交错的情况。

内核中充满了并发访问的数据。例如，两个 CPU 可以同时调用 **kalloc**，从而并发地从空闲内存链表的头部 push。内核设计者喜欢允许大量的并发，因为它可以通过并行来提高性能，提高响应速度。然而，结果是内核设计者花了很多精力说服自己，尽管存在并发，但仍然是正确的。有很多方法可以写出正确的代码，有些方法比其他方法更简单。以并发下的正确性为目标的策略，以及支持这些策略的抽象，被称为并发控制技术。

Xv6 根据不同的情况，使用了很多并发控制技术，还有更多的可能。本章重点介绍一种广泛使用的技术：锁。锁提供了相互排斥的功能，确保一次只有一个 CPU 可以持有锁。如果程序员为每个共享数据项关联一个锁，并且代码在使用某项时总是持有关联的锁，那么该项每次只能由一个 CPU 使用。在这种情况下，我们说锁保护了数据项。虽然锁是一种简单易懂的并发控制机制，但锁的缺点是会扼杀性能，因为锁将并发操作串行化了。

#### 6.1竞争情况

作为我们为什么需要锁的一个例子，考虑两个进程在两个不同的 CPU 上调用 **wait**，**wait**释放子进程的内存。因此，在每个 CPU 上，内核都会调用 **kfree** 来释放子进程的内存页。内核分配器维护了一个链表：**kalloc() **从空闲页链表中 pop 一页内存，**kfree()**将一页 push 空闲链表中。为了达到最好的性能，我们可能希望两个父进程的 **kfrees** 能够并行执行，而不需要任何一个进程等待另一个进程，但是考虑到xv6 的 **kfree** 实现，这是不正确的。

![image-20221116160144854](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221116160144854.png)

图 6.1 更详细地说明了这种设置：链表在两个 CPU 共享的内存中，CPU 使用加载和存储指令操作链表。(在现实中，处理器有缓存，但在概念上，多处理器系统的行为就像有一个单一的共享内存一样)。如果没有并发请求，你可能会实现如下的链表 push 操作：

```c
struct element{
  int data;
    struct element *next; 
};
struct element *list = 0;
void
push(int data)
{
  struct element *l;
  l = malloc(sizeof *l);
  l->data = data;
  l->next = list;
  list = l;
}
```

如果单独执行，这个实现是正确的。但是，如果多个副本同时执行，代码就不正确。如果两个 CPU 同时执行 push，那么两个 CPU 可能都会执行图 6.1 所示的第 12 行，然后其中一个才执行第 13行，这就会产生一个不正确的结果。这样就会出现两个 list元素，将 next 设为 list 的前值。当对 list 的两次赋值发生在第 13 行时，第二次赋值将覆盖第一次赋值；第一次赋值中涉及的元素将丢失。

第 13 行的丢失更新是**竞争条件(race condition）**的一个例子。竞争条件是指同时访问一个内存位置，并且至少有一次访问是写的情况。竞争通常是一个错误的标志，要么是丢失更新（如果访问是写），要么是读取一个不完全更新的数据结构。竞争的结果取决于所涉及的两个 CPU 的确切时间，以及它们的内存操作如何被内存系统排序，这可能会使竞争引起的错误难以重现和调试。例如，在调试 **push** 时加入 **print** 语句可能会改变执行的时机，足以使竞争消失。

避免竞争的通常方法是使用锁。锁确保了相互排斥，因此一次只能有一个CPU执行 **push**的哪一行；这就使得上面的情况不可能发生。上面代码的正确 **lock** 版本只增加了几行代码。

```c
struct element *list = 0;
struct lock listlock;

void
push(int data)
{
struct element *l;
l = malloc(sizeof *l);
l->data = data;
acquire(&listlock);
l->next = list;
list = l;
release(&listlock);
}
```

**acquire** 和 **release** 之间的指令序列通常被称为临界区。这里的锁保护 **list**。

当我们说锁保护数据时，我们真正的意思是锁保护了一些适用于数据的不变式集合。invariant 是数据结构的属性，这些属性在不同的操作中都得到了维护。通常情况下，一个操作的正确行为取决于操作开始时的 invariant 是否为真。操作可能会暂时违反 invariant，但必须在结束前重新建立 invariant。例如，在链表的情况下，invariant 是 **list**指向列表中的第一个元素，并且每个元素的下一个字段指向下一个元素。**push** 的实现暂时违反了这个 invariant：在第11行，**l** 指向下一个 **list** 元素，但 **list** 还没有指向 **l**（在第 12 行重新建立）。我们上面所研究的竞争条件之所以发生，是因为第二个 CPU 执行了依赖于链表invariant 的代码，而它们被暂时违反了。正确地使用锁可以保证一次只能有一个 CPU 对关键部分的数据结构进行操作，所以当数据结构的 invariant 不成立时，没有 CPU 会执行数据结构操作。

你可以把锁看成是把并发的临界区**串行化(serializing)**的一种工具，使它们同时只运行一个，从而保护 invariant（假设临界区是独立的）。你也可以认为由同一个锁保护的临界区，相互之间是原子的，这样每个临界区都只能看到来自之前临界区的完整变化，而永远不会看到部分完成的更新。

虽然正确使用锁可以使不正确的代码变得正确，但锁会限制性能。例如，如果两个进程同时调用 **kfree**，锁会将两个调用串行化，我们在不同的 CPU 上运行它们不会获得任何好处。我们说，如果多个进程同时想要同一个锁，就会发生冲突，或者说锁经历了争夺。内核设计的一个主要挑战是避免锁的争用。Xv6 在这方面做得很少，但是复杂的内核会专门组织数据结构和算法来避免锁争用。例如链表，一个内核可以为每个 CPU 维护一个空闲页链表，只有当自己的链表为空时，并且它必须从另一个 CPU 获取内存时，才会接触另一个 CPU 的空闲链表。其他用例可能需要更复杂的设计。

锁的位置对性能也很重要。例如，在 **push** 中把 **acquire** 移到较早的位置是正确的：把**acquire** 的调用移到第 8 行之前是可以的。这可能会降低性能，因为这样对 **malloc** 的调用也会被锁住。

#### 6.2自旋锁

Xv6 有两种类型的锁：自旋锁和睡眠锁。我们先说说自旋锁。Xv6 将自旋锁表示为一个结构体 **spinlock**。该结构中重要的字段是 **locked**，当锁可获得时，**locked** 为零，当锁被持有时，**locked** 为非零。从逻辑上讲，xv6 获取锁的的代码类似于：

```c
void
acquire(struct spinlock *lk) // does not work!
{
	for (;;){
		if (lk->locked == 0){
			lk->locked = 1;
			break;
		}
	}
}
```

不幸的是，这种实现并不能保证多处理器上的相互排斥。可能会出现这样的情况：两个CPU 同时到达 if 语句，看到 **lk->locked** 为零，然后都通过设置 **lk->locked = 1** 来抢夺锁。此时，两个不同的 CPU 持有锁，这就违反了互斥属性。我们需要的是让第 5 行和第 6 行作为一个原子即不可分割步骤来执行。

由于锁被广泛使用，多核处理器通常提供了一些原子版的指令。在 RISC-V 上，这条指令是 **amoswap r, a**。**amoswap** 读取内存地址 **a** 处的值，将寄存器 **r** 的内容写入该地址，并将其读取的值放入 **r** 中，也就是说，它将寄存器的内容和内存地址进行了交换。它原子地执行这个序列，使用特殊的硬件来防止任何其他 CPU 使用读和写之间的内存地址。

Xv6 的 acquire使 用 了 可 移 植 的 C 库调用__sync_lock_test_and_set，它本质上为 **amoswap** 指令；返回值是 **lk->locked** 的旧（交换）内容。**acquire** 函数循环交换，重试（旋转）直到获取了锁。每一次迭代都会将 1 交换到**lk->locked** 中，并检查之前的值；如果之前的值为 0，那么我们已经获得了锁，并且交换将**lk->locked** 设置为 1。如果之前的值是 1，那么其他 CPU 持有该锁，而我们原子地将 1 换成 **lk->locked** 并没有改变它的值。

一旦锁被获取，**acquire** 就会记录获取该锁的 CPU，这方便调试。**lk->cpu** 字段受到锁的保护，只有在持有锁的时候才能改变。

函数 **release**与 **acquire** 相反：它清除 **lk->cpu** 字段，然后释放锁。从概念上讲，释放只需要给 **lk->locked** 赋值为 0。C 标准允许编译器用多条存储指令来实现赋值，所以 C 赋值对于并发代码来说可能是非原子性的。相反，release 使用 C 库函数__sync_lock_release 执行原子赋值。这个函数也是使用了 RISC-V 的 **amoswap** 指令。

`spinlock.h`

```c
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

```

`spinlock.c`

```c
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
```

#### 6.3锁的使用

Xv6 在很多地方使用锁来避免竞争条件。如上所述，**kalloc**和 **kfree**就是一个很好的例子。xv6 也会有一些竞争。使用锁的一个难点是决定使用多少个锁，以及每个锁应该保护哪些数据和 invariant。有几个基本原则。首先，任何时候，当一个 CPU 在另一个 CPU 读写数据的同时，写入变量，都应该使用锁来防止这两个操作重叠。其次，记住锁保护的是 invariant：如果一个 invariant涉及到多个内存位置，通常需要用一个锁保护所有的位置，以确保 invariant 得到维护。

上面的规则说了什么时候需要锁，但没说什么时候不需要锁，为了效率，不要太多锁，因为锁会降低并行性。如果并行性不重要，那么可以只安排一个线程，而不用担心锁的问题。一个简单的内核可以在多处理器上像这样做，通过一个单一的锁，这个锁必须在进入内核时获得，并在退出内核时释放（尽管系统调用，如管道读取或等待会带来一个问题）。许多单处理器操作系统已经被改造成使用这种方法在多处理器上运行，有时被称为“大内核锁“，但这种方法牺牲了并行性：内核中一次只能执行一个 CPU。如果内核做任何繁重的计算，那么使用一组更大的更精细的锁，这样内核可以同时在多个 CPU 上执行，效率会更高。

作为粗粒度锁的一个例子，xv6 的 kalloc.c 分配器有一个单一的空闲页链表，由一个锁保护。如果不同 CPU 上的多个进程试图同时分配内存页，每个进程都必须通过在 **acquire** 中自旋来等待获取锁。自旋会降低性能，因为这是无意义的。如果对锁的争夺浪费了很大一部分 CPU 时间，也许可以通过改变分配器的设计来提高性能，使其拥有多个空闲页链表，每个链表都有自己的锁，从而实现真正的并行分配。

作为细粒度锁的一个例子，xv6 对每个文件都有一个单独的锁，这样操作不同文件的进程往往可以不等待对方的锁就可以进行。如果想让进程同时写入同一文件的不同区域，文件锁方案可以做得更细。最后，锁粒度的决定需要考虑性能以及复杂性。

![image-20221116191701662](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221116191701662.png)

#### 6.4死锁和锁排序

如果一个穿过内核的代码路径必须同时持有多个锁，那么所有的代码路径以相同的顺序获取这些锁是很重要的。如果他们不这样做，就会有死锁的风险。假设线程 T1执行代码 path1并获取锁 A，线程 T2 执行代码 path2 并获取锁 B，接下来 T1 会尝试获取锁 B，T2 会尝试获取锁 A，这两次获取都会无限期地阻塞，因为在这两种情况下，另一个线程都持有所需的锁，并且不会释放它，直到它的获取返回。为了避免这样的死锁，所有的代码路径必须以相同的顺序获取锁。对全局锁获取顺序的需求意味着锁实际上是每个函数规范的一部分：调用者调用函数的方式必须使锁按照约定的顺序被获取。

由于 **sleep** 的工作方式，Xv6 有许多长度为 2 的锁序链，涉及到进程锁（**struct proc** 中的锁）。例如，consoleintr是处理类型化字符的中断例程。当一个新数据到达时，任何正在等待控制台输入的进程都应该被唤醒。为此，**consoleintr** 在调用 **wakeup** 时持有 **cons.lock**，以获取进程锁来唤醒它。因此，全局避免死锁的锁顺序包括了 **cons.lock** 必须在任何进程锁之前获取的规则。文件系统代码包含 xv6 最长的锁链。例如，创建一个文件需要同时持有目录的锁、新文件的 inode 的锁、磁盘块缓冲区的锁、磁盘驱动器的 **vdisk_lock** 和调用进程的 **p->lock**。为了避免死锁，文件系统代码总是按照上一句提到的顺序获取锁。

遵守全局避免死锁的顺序可能会非常困难。有时锁的顺序与逻辑程序结构相冲突，例如，也许代码模块 M1 调用模块 M2，但锁的顺序要求 M2 中的锁在 M1 中的锁之前被获取。有时锁的身份并不是事先知道的，也许是因为必须持有一个锁才能发现接下来要获取的锁的身份。这种情况出现在文件系统中，因为它在路径名中查找连续的组件，也出现在wait 和 exit 的代码中，因为它们搜索进程表寻找子进程。最后，死锁的危险往往制约着人们对锁方案的细化程度，因为更多的锁往往意味着更多的死锁机会。避免死锁是内核实现的重要需求。

#### 6.5锁和中断处理程序

一些 xv6 自旋锁保护的数据会被线程和中断处理程序两者使用。例如，**clockintr** 定时器中断处理程序可能会在内核线程读取 **sys_sleep**中的 **ticks** 的同时，递增 **ticks** 。锁 **tickslock** 将保护两次临界区。

自旋锁和中断的相互作用带来了一个潜在的危险。假设 **sys_sleep** 持有 **tickslock**，而它的 CPU 被一个定时器中断。 **clockintr** 会尝试获取 **tickslock**，看到它被持有，并等待它被释放。在这种情况下，**tickslock** 永远不会被释放：只有 **sys_sleep** 可以释放它，但 **sys_sleep**不会继续运行，直到 **clockintr** 返回。所以 CPU 会死锁，任何需要其他锁的代码也会冻结。

为了避免这种情况，如果一个中断处理程序使用了自旋锁，CPU 决不能在启用中断的情况下持有该锁。Xv6 比较保守：当一个 CPU 获取任何锁时，xv6 总是禁用该 CPU 上的中断。中断仍然可能发生在其他 CPU 上，所以一个中断程序获取锁会等待一个线程释放自旋锁；它们不在同一个 CPU 上。

xv6 在 CPU 没有持有自旋锁时重新启用中断；它必须做一点记录来应对嵌套的临界区。**acquire**调用**push_off**和**release**调用**pop_off**来跟踪当前 CPU 上锁的嵌套级别。当该计数达到零时，**pop_off** 会恢复最外层临界区开始时的中断启用状态。**intr_off** 和 **intr_on** 函数分别执行 RISC-V 指令来禁用和启用中断。

在设置 **lk->locked** 之前，严格调用 **push_off** 是很重要的。如果两者反过来，那么在启用中断的情况下，锁会有一个窗口（未锁到的位置），在未禁止中断时持有锁，不幸的是，一个定时的中断会使系统死锁。同样，释放锁后才调用 **pop_off** 也很重要。

#### 6.6指令和内存排序

人们很自然地认为程序是按照源代码语句出现的顺序来执行的。然而，许多编译器和CPU 为了获得更高的性能，会不按顺序执行代码。如果一条指令需要很多周期才能完成，CPU 可能会提前发出该指令，以便与其他指令重叠，避免 CPU 停顿。例如，CPU 可能会注意到在一个串行序列中，指令 A 和 B 互不依赖。CPU 可能先启动指令 B，这是因为它的输入在 A 的输入之前已经准备好了，或者是为了使 A 和 B 的执行重叠。 编译器可以执行类似的重新排序，在一条语句的指令之前发出另一条语句的指令，由于它们原来的顺序。

编译器和 CPU 在 re-order 时遵循相应规则，以确保它们不会改变正确编写的串行代码的结果。然而，这些规则确实允许 re-order，从而改变并发代码的结果，并且很容易导致多处理器上的不正确行为。CPU 的 ordering 规则称为内存模型。

例如，在这段 push 的代码中，如果编译器或 CPU 将第 4 行对应的存储移到第 6 行释放后的某个点，那将是一场灾难。

```c
l = malloc(sizeof *l);
l->data = data;
acquire(&listlock);
l->next = list;
list = l;
release(&listlock);
```

如果发生这样的 re-order，就会有一个窗口，在这个窗口中，另一个 CPU 可以获取锁并观察更新的链表，但看到的是一个未初始化的 **list->next**。

为了告诉硬件和编译器不要执行这样的 re-ordering，xv6 在获取和释放中都使用sync_synchronize()。__sync_synchronize() 是一个**内存屏障**(**memory barrier)**：它告诉编译器和 CPU 不要在屏障上 re-order 加载或存储。xv6 中的屏障几乎在所有重要的情况下都会 **acquire** 和 **release** 强制顺序，因为 xv6 在访问共享数据的周围使用锁。

#### 6.7睡眠锁

有时 xv6 需要长时间保持一个锁。例如，文件系统在磁盘上读写文件内容时，会保持一个文件的锁定，这些磁盘操作可能需要几十毫秒。如果另一个进程想获取一个自旋锁，那么保持那么长的时间会导致浪费，因为获取进程在自旋的同时会浪费 CPU 很长时间。自旋锁的另一个缺点是，一个进程在保留自旋锁的同时不能让出 CPU；我们希望做到这一点，这样其他进程可以在拥有锁的进程等待磁盘的时候使用 CPU。在保留自旋锁的同时让出是非法的，因为如果第二个线程试图获取自旋锁，可能会导致死锁；因为获取自旋锁不会让出 CPU，第二个线程的自旋可能会阻止第一个线程运行并释放锁。在持有锁的同时让出会违反在持有自旋锁时中断必须关闭的要求。因此，我们希望有一种锁，在等待获取的过程中产生 CPU，并在锁被持有时允许让出 cpu（和中断）。

Xv6以**睡眠锁(sleep-locks)**的形式提供了这样的锁。**acquiresleep**在等待的过程中让出 CPU，使用的技术将在第 7 章解释。在高层次上，**sleep-lock** 有一个由**spinlock** 保护的锁定字段，而 **acquiresleep** 对 **sleep** 的调用会原子性地让出 CPU 并释放**spinlock**。其结果是，在 **acquiresleep** 等待时，其他线程可以执行。

因为睡眠锁会使中断处于启用状态，所以不能在中断处理程序中使用睡眠锁。因为acquiresleep 可能会让出 CPU，所以睡眠锁不能在 spinlock 临界区内使用（虽然 spinlocks 可以在睡眠锁临界区内使用）。

自旋锁最适合短的临界区，因为等待它们会浪费 CPU 时间；睡眠锁对长时间的操作很有效。

`sleeplock.h`

```c
// Long-term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?
  // 必须使用自旋锁保护，保证并发下的正确性
  struct spinlock lk; // spinlock protecting this sleep lock
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};
```

`sleeplock.c`

```c
// Sleeping locks
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  // while循环消除了无效唤醒
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  // 获得睡眠锁
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

void
releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  // 释放睡眠锁，唤醒可能睡眠的进程
  wakeup(lk);
  release(&lk->lk);
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;  
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}
```

### 第七章：调度

任何操作系统运行的进程数量都可能超过计算机的 CPU 数量，因此需要制定一个方案，在各进程之间分时共享 CPU。理想情况下，这种共享对用户进程是透明的。一种常见的方法是通过将进程复用到硬件 CPU 上，给每个进程提供它有自己的虚拟 CPU 的假象。本章解释xv6 如何实现这种复用。

#### 7.1关于复用

xv6 通过在两种情况下将 CPU 从一个进程切换到另一个进程来实现复用。首先，xv6 的sleep 和wakeup机制会进行切换，这会发生在进程等待设备或管道 I/O，或等待子进程退出，或在 **sleep** 系统调用中等待。其次，xv6 周期性地强制切换，以应对长时间的计算进程。这种复用造成了每个进程都有自己的 CPU 的假象，就像 xv6 使用内存分配器和硬件页表造成每个进程都有自己的内存的假象一样。

实现复用会有一些挑战。首先，如何从一个进程切换到另一个进程？虽然上下文切换的想法很简单，但实现起来却很难。第二，如何对用户进程透明的强制切换？xv6 采用一般的方式，用定时器中断来驱动上下文切换。第三，许多 CPU 可能会在进程间并发切换，需要设计一个锁来避免竞争。第四，当进程退出时，必须释放进程的内存和其他资源，但它自己不能做到这一切，因为它不能释放自己的内核栈，同时又在使用内核栈。第五，多核机器的每个内核必须记住它正在执行的进程，这样系统调用就会修改相应进程的内核状态。最后，**sleep** 和 **wakeup** 允许一个进程放弃 CPU，并睡眠等待事件，并允许另一个进程唤醒第一个进程。需要注意一些竞争可能会使唤醒丢失。Xv6 试图尽可能简单地解决这些问题，但尽管如此，写出来代码还是很棘手。

#### 7.2上下文切换

![image-20221116200142968](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221116200142968.png)

图 7.1 概述了从一个用户进程切换到另一个用户进程所涉及的步骤：用户内核转换（系统调用或中断）到旧进程的内核线程，context 切换到当前 CPU 的调度器线程，context 切换到新进程的内核线程，以及 trap 返回到用户级进程。xv6 调度器在每个 CPU 上有一个专门的线程(保存的寄存器和栈)，因为调度器在旧进程的内核栈上执行是不安全的：其他核心可能会唤醒进程并运行它，而且在两个不同的核心上使用相同的栈将是一场灾难。在本节中，我们将研究在内核线程和调度线程之间切换的机制。

从一个线程切换到另一个线程，需要保存旧线程的 CPU 寄存器，并恢复新线程之前保存的寄存器；栈指针和 pc 被保存和恢复，意味着 CPU 将切换栈和正在执行的代码。

函数 **swtch** 执行内核线程切换的保存和恢复。**swtch** 并不直接知道线程，它只是保存和恢复寄存器组，称为**上下文(context)**。当一个进程要放弃 CPU 的时候，进程的内核线程会调用 **swtch** 保存自己的上下文并返回到调度器上下文。每个上下文都包含在一个结构体**context**中，它本身包含在进程的结构体 **proc** 或 CPU 的结构体 **cpu** 中。**Swtch** 有两个参数：**struct context \*old** 和 **struct context \*new**。它将当前的寄存器保存在old 中，从 new 中加载寄存器，然后返回。

让我们跟随一个进程通过 **swtch** 进入 **scheduler**。我们在第 4 章看到，在中断结束时，有一种情况是 **usertrap** 调用 **yield**。**yield** 又调用 **sched**，**sched** 调用 **swtch** 将当前上下文保存在 **p->context** 中 ， 并 切 换 到 之 前 保 存 在 **cpu->scheduler** 中 的 调 度 器 上 下 文。

**Swtch**只保存 callee-saved 寄存器，caller-saved 寄存器由调用的 C代码保存在堆栈上。**Swtch** 知道 **struct context** 中每个寄存器字段的偏移量。它不保存 pc。相反，**swtch** 保存了 ra 寄存器，它保存了 **swtch** 应该返回的地址。现在，**swtch**从新的上下文中恢复寄存器，新的上下文中保存着前一次 **swtch** 所保存的寄存器值。当**swtch** 返回时，它返回到被恢复的 ra 寄存器所指向的指令，也就是新线程之前调用 **swtch**的指令。此外，它还会在新线程的栈上返回。

在我们的例子中，**sched** 调用 **swtch** 切换到 **cpu->scheduler**，即 CPU 的调度上下文。这个上下文已经被调度器对 **swtch** 的调用所保存。当我们跟踪的 **swtch**返回时，它不是返回到 **sched** 而是返回到 **scheduler**，它的栈指针指向当前 CPU 的调度器栈。

#### 7.3调度核心代码

```c
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    // CPU核心通过设置SSTATUS寄存器（SSTATUS_SIE）开启设备中断
    intr_on();
    
    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;

        // A kernel page table per process 实验开始
        w_satp(MAKE_SATP(p->kernelpt));
        sfence_vma();
        // A kernel page table per process 实验结束

        // 切换到选中的进程执行
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        // A kernel page table per process 实验开始
        w_satp(MAKE_SATP(kernel_pagetable));
      	sfence_vma();
        // A kernel page table per process 实验结束
        found = 1;
      }
      // 这一步很重要，每次通过shced()-->scheduler()-->swtch()执行，到达这里会释放之前保持的p->lock
      // 一个具体的例子就是sleep()沿着函数调用到达这里后释放了p->lock，wakeup()可以获得p->lock并唤醒p
      release(&p->lock);
    }
    if(found == 0) {
      intr_on();
      asm volatile("wfi");
    }
  }
}
```

#### 7.4mycpu和myproc

Xv6 经常需要一个指向当前进程 **proc** 的指针。在单核处理器上，可以用一个全局变量指向当前的 **proc**。这在多核机器上是行不通的，因为每个核都执行不同的进程。解决这个问题的方法是利用每个核都有自己的一组寄存器的事实；我们可以使用其中的一个寄存器来帮助查找每个核的信息。

Xv6 为每个 CPU 维护了一个 **cpu** 结构体，它记录了当前在该 CPU 上运行的进程(如果有的话)，为 CPU 的调度线程保存的寄存器，以及管理中断禁用所需的嵌套自旋锁的计数。函数 **mycpu**返回一个指向当前 CPU 结构体 **cpu** 的指针。RISC-V 对其 CPU 进行编号，给每个 CPU 一个 hartid。Xv6 确保每个 CPU 的 hartid 在内核中被存储在该 CPU 的 **tp** 寄存器中。这使得 mycpu 可以使用 **tp** 对 **cpu** 结构体的数组进行索引，从而找到正确的 cpu。

确保一个 CPU 的 **tp** 始终保持 CPU 的 hartid 是有一点复杂的。**mstart** 在 CPU 启动的早期设置 **tp** 寄存器，此时 CPU 处于机器模式。**Usertrapret** 将 **tp** 寄存器保存在 trampoline 页中，因为用户进程可能会修改 tp 寄存器。最后，当从用户空间进入内核时，**uservec** 会恢复保存的 **tp**。编译器保证永远不使用 **tp** 寄存器。如果 RISC-V 允许 xv6 直接读取当前的 hartid 会更方便，但机器模式只有机器模式下能够读取，不允许在监督者模式下使用。

**cpuid** 和 **mycpu** 的返回值很容易错：如果定时器中断，导致线程让出 CPU，然后转移到不同的 CPU 上，之前返回的值将不再正确。为了避免这个问题，xv6 要求调用者禁用中断，只有在使用完返回的 cpu 结构后才启用中断。

**myproc**函数返回当前 CPU 上运行的进程的 **proc** 指针。**myproc** 禁用中断，调用 **mycpu**，从 **cpu** 中获取当前进程指针(**c->proc**)，然后启用中断。即使启用了中断，**myproc** 的返回值也可以安全使用：如果定时器中断将调用进程移到了不同的 CPU 上，它的 **proc** 结构指针将保持不变。

#### 7.5sleep和wakeup机制

调度和锁有助于让一个进程对另一个进程的不可见，但到目前为止，我们还没有任何抽象来帮助进程进行交互。人们发明了许多机制来解决这个问题。Xv6 使用了一种叫做睡眠和唤醒的机制，它允许一个进程睡眠并等待事件，而另一个进程在事件发生后将其唤醒。睡眠和唤醒通常被称为 **序 列 协 调 (sequence coordination)** 或 **条 件 同 步 (conditional** **synchronization)** 机制。

为了说明这一点，让我们考虑一个叫做**信号量(semaphore**)的同步机制，它协调生产者和消费者。信号量维护一个计数并提供两个操作。V 操作（针对生产者）增加计数。P 操作（针对消费者）等待，直到计数非零，然后将其递减并返回。如果只有一个生产者线程和一个消费者线程，而且它们在不同的 CPU 上执行，编译器也没有太过激进的优化，那么这个实现是正确的。

```c
struct semaphore
{
    struct spinlock lock;
    int count;
};
void V(struct semaphore *s)
{
    acquire(&s->lock);
    s->count += 1;
    release(&s->lock);
}
void P(struct semaphore *s)
{
    while (s->count == 0)
    ;
    acquire(&s->lock);
    s->count -= 1;
    release(&s->lock);
}
```

上面的实现是代价很大。如果生产者很少生产，消费者将把大部分时间花在 while 循环中，希望得到一个非零的计数。消费者的 CPU 可以通过反复**轮询(polling)** **s->count** 来找到比**忙碌等待(busy waiting)**更有效的工作。避免**忙碌等待**需要一种方法，让消费者让出 CPU，只有在 **V** 增加计数后才恢复。

这里是朝着这个方向迈出的一步，虽然他不能完全解决这个问题。让我们想象一对调用，**sleep** 和 **wakeup**，其工作原理如下。**Sleep(chan)**睡眠 **chan** 上，**chan** 可以为任意值，称为**等待通道(wait chan)**。**Sleep** 使调用进程进入睡眠状态，释放 CPU 进行其他工作。**Wakeup(chan)**唤醒所有在 **chan** 上 **sleep** 的进程（如果有的话），使它们的 **sleep** 调用返回。如果没有进程在 **chan** 上等待，则 **wakeup** 不做任何事情。我们更改信号量实现，以使用**sleep** 和 **wakeup**。

```c
void V(struct semaphore *s)
{
    acquire(&s->lock);
    s->count += 1;
    wakeup(s);
    release(&s->lock);
}
void P(struct semaphore *s)
{
    while (s->count == 0)
    	sleep(s);
    acquire(&s->lock);
    s->count -= 1;
    release(&s->lock);
}
```

P 现在放弃 CPU 而不是自旋，这是一个不错的改进。然而，事实证明，像这样设计 **sleep**和 **wakeup** 并不明确，因为存在可能丢失唤醒的问题。假设执行 P 的 **s->count == 0** 这一行时。当 P 在 **sleep** 之前，V 在另一个 CPU 上运行：它将 **s->count** 改为非零，并调用**wakeup**，**wakeup** 发现没有进程在睡眠，因此什么也不做。现在 P 继续执行：它调用 **sleep**并进入睡眠状态。这就造成了一个问题：P 正在 **sleep**，等待一个已经发生的 V 调用。除非我们运气好，生产者再次调用 V，否则消费者将永远等待，即使计数是非零。

这个问题的根源在于，P 只有在 **s->count == 0** 时才会 **sleep** 的不变式被违反，当 V 运行在错误的时刻时。保护这个不变式的一个不正确的方法是移动 P 中的锁的获取，这样它对计数的检查和对 sleep 的调用是原子的：

```c
void V(struct semaphore *s)
{
    acquire(&s->lock);
    s->count += 1;
    wakeup(s);
    release(&s->lock);
}
void P(struct semaphore *s)
{
    acquire(&s->lock);
    while (s->count == 0)
    	sleep(s);
    s->count -= 1;
    release(&s->lock);
}
```

人们可能希望这个版本的 P 能够避免丢失的唤醒，因为锁会阻止 V 在 **s->count == 0**和 sleep 之间执行。它做到了这一点，但它也会死锁。P 在 **sleep** 时保持着锁，所以 V 将永远阻塞在等待锁的过程中。

我们将通过改变 **sleep** 的接口来修正前面的方案：调用者必须将**条件锁(condition lock)**传递给 **sleep**，这样在调用进程被标记为 **SLEEPING** 并在 chan 上等待后，它就可以释放锁。锁将强制并发的 V 等待到 P 完成使自己进入 **SLEEPING** 状态，这样 **wakeup** 就会发现**SLEEPING** 的消费者并将其唤醒。一旦消费者再次被唤醒，**sleep** 就会重新获得锁，然后再返回。我们新的正确的睡眠/唤醒方案可以使用如下(**sleep** 函数改变了)。

```c
void V(struct semaphore *s)
{
    acquire(&s->lock);
    s->count += 1;
    wakeup(s);
    release(&s->lock);
}
void P(struct semaphore *s)
{
    acquire(&s->lock);
    while (s->count == 0)
    	sleep(s, &s->lock);
    s->count -= 1;
    release(&s->lock);
}
```

P 持有 **s->lock** 会阻止了 V 在 P 检查 **s->count** 和调用 **sleep** 之间试图唤醒它。但是，请注意，我们需要 **sleep** 来原子地释放 **s->lock** 并使消费者进程进入 **SLEEPING** 状态。

#### 7.6sleep和wakeup代码

让我们看看 sleep 和 wakeup的实现。其基本思想是让 sleep 将当前进程标记为 SLEEPING，然后调用 sched 让出 CPU；wakeup 则寻找给定的等待通道上睡眠的进程，并将其标记为RUNNABLE。sleep 和 wakeup 的调用者可以使用任何方便的数字作为 chan。Xv6 经常使用参与等待的内核数据结构地址。

`sleep`

```c
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  // 修改进程状态和调用sched函数前，必须拿到p->lock
  if(lk != &p->lock){  //DOC: sleeplock0
    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  // 进程休眠
  p->chan = chan;
  p->state = SLEEPING;
    
  // 转去调度线程
  sched();

  // Tidy up.
  // 进程被唤醒紧接着执行，重置chan休眠等待链
  p->chan = 0;

  // Reacquire original lock.
  // 唤醒后需要重新获得原来的lk
  if(lk != &p->lock){
    release(&p->lock);
    acquire(lk);
  }
}
```

`wakeup`

```c
// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}
```

Sleep 首先获取 p->lock。现在进入睡眠状态的进程同时持有p->lock 和 lk。在调用者(在本例中为 P)中，持有 lk 是必要的：它保证了没有其他进程(在本例中，运行 V 的进程)可以调用 wakeup(chan)。现在 sleep 持有 p->lock，释放 lk 是安全的：其他进程可能会调用 wakeup(chan)，但 wakeup 会等待获得 p->lock，因此会等到 sleep将进程状态设置为 SLEEPING，使 wakeup 不会错过 sleep 的进程。

有一个复杂情况：如果 lk 和 p->lock 是同一个锁，如果 sleep 仍试图获取 p->lock，就会和自己死锁。但是如果调用 sleep 的进程已经持有 p->lock，那么它就不需要再做任何事情来避免错过一个并发的 wakeup。这样的情况发生在，wait调用sleep 并持有 p->lock 时。

现在 sleep 持有 p->lock，而没有其他的锁，它可以通过记录它睡眠的 chan，将进程状态设置为SLEEPING，并调用 sched来使进程进入睡眠状态。稍后我们就会明白为什么在进程被标记为 SLEEPING 之前，p->lock 不会被释放（由调度器）。

在某些时候，一个进程将获取条件锁，设置睡眠等待的条件，并调用 wakeup(chan)。重要的是，wakeup 是在持有条件锁的情况下被调用的。Wakeup 循环浏览进程表。它获取每个被检查的进程的 p->lock，因为它可能会修改该进程的状态，也因为 p->lock 确保 sleep 和 wakeup 不会相互错过。当 wakeup 发现一个进程处于状态为 SLEEPING 并有一个匹配的 chan 时，它就会将该进程的状态改为 RUNNABLE。下一次调度器运行时，就会看到这个进程已经准备好运行了。

为什么 sleep 和 wakeup 的锁规则能保证睡眠的进程不会错过 wakeup？sleep 进程从检查条件之前到标记为 SLEEPING 之后的这段时间里，持有条件锁或它自己的 p->lock 或两者都持有。调用 wakeup 的进程在 wakeup 的循环中持有这两个锁。因此，唤醒者要么在消费者检查条件之前使条件为真；要么唤醒者的 wakeup 在消费者被标记为 SLEEPING 之后检查它。 无论怎样，wakeup 就会看到这个睡眠的进程，并将其唤醒（除非有其他事情先将其唤醒）。

有时会出现多个进程在同一个 chan 上睡眠的情况；例如，有多个进程从管道中读取数据。调用一次 wakeup 就会把它们全部唤醒。其中一个进程将首先运行，并获得 sleep 参数传递的锁，就管道而言读取数据都会在管道中等待。其他进程会发现，尽管被唤醒了，但没有数据可读。从他们的角度来看，唤醒是“虚假的“，他们必须再次睡眠。出于这个原因，sleep 总是在一个检查条件的循环中被调用。

如果两次使用 sleep/wakeup 不小心选择了同一个通道，也不会有害：它们会看到虚假的唤醒，上面提到的循环允许发生这种情况。sleep/wakeup 的魅力很大程度上在于它既是轻量级的（不需要创建特殊的数据结构来充当睡眠通道），又提供了一层间接性（调用者不需要知道他们正在与哪个具体的进程交互）。

#### 7.7管道代码

一个使用 sleep 和 wakeup 来同步生产者和消费者的更复杂的例子是 xv6 的管道实现。我们在第 1 章看到了管道的接口：写入管道一端的字节被复制到内核缓冲区，然后可以从管道的另一端读取。未来的章节将研究管道如何支持文件描述符，但我们现在来看一下pipewrite 和 piperead 的实现吧。

每个管道由一个结构体 pipe 表示，它包含一个锁和一个数据缓冲区。nread 和 nwrite两个字段统计从缓冲区读取和写入的字节总数。缓冲区呈环形：buf[PIPESIZE-1]之后写入的下一个字节是 buf[0]。计数不呈环形。这个约定使得实现可以区分满缓冲区(nwrite == nread+PIPESIZE)和空缓冲区(nwrite == nread)，但这意味着对缓冲区的索引必须使用buf[nread % PIPESIZE]，而不是使用 buf[nread]。

假设对 piperead 和 pipewrite 的调用同时发生在两个不同的 CPU 上。Pipewrite首先获取管道的锁，它保护了计数、数据和相关的不变式。然后，Piperead也试图获取这个锁，但是不会获取成功。它在 acquire中循环，等待锁的到来。当 piperead 等待时，pipewrite 会循环写，依次将每个字节添加到管道中。在这个循环中，可能会发生缓冲区被填满的情况。在这种情况下，pipewrite 调用 wakeup 来唤醒所有睡眠中的 reader 有数据在缓冲区中等待，然后在&pi->nwrite 上 sleep，等待 reader 从缓冲区中取出一些字节。Sleep 函数内会释放 pi->lock，然后 pipwrite 进程睡眠。

`pipe结构体`

```c
#define PIPESIZE 512
struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};
```

`pipewrite`

```c
int
pipewrite(struct pipe *pi, uint64 addr, int n)
{
  int i;
  char ch;
  struct proc *pr = myproc();

  acquire(&pi->lock);
  for(i = 0; i < n; i++){
    // 管道数据区满了，唤醒piperead进程并陷入休眠
    while(pi->nwrite == pi->nread + PIPESIZE){  //DOC: pipewrite-full
      if(pi->readopen == 0 || pr->killed){
        release(&pi->lock);
        return -1;
      }
      wakeup(&pi->nread);
      sleep(&pi->nwrite, &pi->lock);
    }
    // 数据写入到内核数据区中
    if(copyin(pr->pagetable, &ch, addr + i, 1) == -1)
      break;
    pi->data[pi->nwrite++ % PIPESIZE] = ch;
  }
  // 已全部写入，唤醒读进程即可
  wakeup(&pi->nread);
  release(&pi->lock);
  return i;
}
```

现在 pi->lock 可用了，piperead 设法获取它并进入它的临界区：它发现 pi->nread != pi->nwrite (pipewrite 进 入 睡 眠 状 态 是 由 于 pi->nwrite == pi->nread+PIPESIZE )，所以它进入 for 循环，将数据从管道中复制出来，并按复制的字节数增加 nread。现在又可写了，所以 piperead 在返回之前调用 wakeup来唤醒在睡眠的 writer。Wakeup 找到一个在&pi->nwrite 上睡眠的进程，这个进程正在运行 pipewrite，但在缓冲区填满时停止了。它将该进程标记为 RUNNABLE。

`piperead`

```c
int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  // 管道数据区为空，进程休眠等待write进程的唤醒
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(pr->killed){
      release(&pi->lock);
      return -1;
    }
    sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
  }
  // 数据从管道数据区拷贝到用户数据区
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread++ % PIPESIZE];
    if(copyout(pr->pagetable, addr + i, &ch, 1) == -1)
      break;
  }
  // 读取完毕，唤醒写进程继续写入即可
  wakeup(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}
```

管道代码对 reader 和 writer 使用单独的睡眠 chan（pi->nread 和 pi->nwrite）；这可能会使系统在有多个 reader 和 writer 等待同一个管道的情况下更有效率。管道代码在循环内 sleep，检查 sleep 条件；如果有多个 reader 和 writer，除了第一个被唤醒的进程外，其他进程都会看到条件仍然是假的，然后再次睡眠。

#### 7.8wait、exit和kill代码

sleep 和 wakeup 可以用于许多种需要等待的情况。在第 1 章中介绍的一个有趣的例子是，一个子进程的 exit 和其父进程的 wait 之间的交互。在子进程退出的时候，父进程可能已经在 wait 中睡眠了，也可能在做别的事情；在后一种情况下，后续的 wait 调用必须观察子进程的退出，也许是在它调用 exit 之后很久。`xv6 在 wait 观察到子进程退出之前，记录子进程退出的方式是让 exit 将调用进程设置为 ZOMBIE 状态，在那里停留，直到父进程的wait 注意到它，将子进程的状态改为 UNUSED，复制子进程的退出状态，并将子进程的进程 ID 返回给父进程。`==如果父进程比子进程先退出，父进程就把子进程交给 init 进程，而 init进程则循环的调用 wait；这样每个子进程都有一个“父进程”来清理。==主要的实现挑战是父进程和子进程的 **wait** 和 **exit**，以及 **exit** 和 **exit** 之间可能出现竞争和死锁的情况。

`Wait 使用调用进程 的 p->lock 作为条件锁，以避免唤醒丢失，它在开始时获取该锁。然后它扫描进程表。如果它发现一个处于 ZOMBIE 状态的子进程，它释放这个子进程的资源和它的 proc 结构，将子进程的退出状态复制到提供给 wait 的地址(如果它不是 0)，并返回子进程的 ID。`==如果 wait 找到了子进程但没有一个退出，它调用 sleep等待其中一个子进程退出，然后再次扫描。这里，在 sleep 中释放的条件锁是等待进程的 p->lock，也就是上面提到的特殊情况。==请注意，wait 经常持有两个锁；`它在试图获取任何子锁之前，会先获取自己的锁；因此 xv6 的所有锁都必须遵守相同的锁顺序（父进程的锁，然后是子进程的锁），以避免死锁。`

Wait 会查看每个进程的 np->parent 来寻找它的子进程。它使用 np->parent 而不持有 np->lock，这违反了共享变量必须受锁保护的通常规则。但是 np 有可能是当前进程的祖先，在这种情况下，获取 np->lock 可能会导致死锁，因为这违反了上面提到的顺序。在这种情况下，在没有锁的情况下检查 np->parent 似乎是安全的；一个进程的父进程字段只有“父亲“改变，所以如果 np->parent==p 为真，除非当前进程改变它，否则该值就不会改变。

`wait`

```c
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  // hold p->lock for the whole time to avoid lost
  // wakeups from a child's exit().
  acquire(&p->lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      // np可能是p的祖先进程，注意需要遵循“先父进程锁后子进程锁”的原则
      if(np->parent == p){
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acquire(&np->lock);
        havekids = 1;
        // 对于僵死的子进程需要释放其内存
        if(np->state == ZOMBIE){
          // Found one.
          // 子进程的进程ID
          pid = np->pid;
          // 子进程的退出状态复制到addr地址处
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          // 释放子进程内存
          freeproc(np);
          release(&np->lock);
          release(&p->lock);
          // 返回子进程ID
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    // 没有子进程直接返回-1
    if(!havekids || p->killed){
      release(&p->lock);
      return -1;
    }
    
    // Wait for a child to exit.
    // 等待子进程调用exit退出唤醒
    sleep(p, &p->lock);  //DOC: wait-sleep
  }
}
```

`Exit记录退出状态，释放一些资源，将所有子进程交给 init 进程，在父进程处于等待状态时唤醒它，将调用进程标记为 zombie，并永久放弃 CPU。`==退出的进程必须持有父进程的锁，同时将自己状态设置为 ZOMBIE 并唤醒父进程，因为父进程的锁是条件锁，可以防止在等待中丢失 wakeup。子进程也必须持有自己的 p->lock，否则父进程可能会看到它的状态为 ZOMBIE，并在它还在运行时释放它。==

> 锁的获取顺序对避免死锁很重要：因为 wait 在子锁之前获取父锁，所以 exit 必须使用相同的顺序。

Exit 调用了一个专门的唤醒函数 wakeup1，它只唤醒父进程，而且只有父进程在 wait中睡眠的情况下才会去唤醒它。`在将自己的状态设置为 ZOMBIE 之前，唤醒父进程可能看起来并不正确，但这是安全的：尽管 wakeup1 可能会导致父进程运行，但 wait 中的循环不能检查子进程，直到子进程的 p->lock 被调度器释放为止，所以 wait 不能查看退出的进程，直到 exit 将其状态设置为 ZOMBIE 之后。`

`exit`

```c
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();
  if(p == initproc)
    panic("init exiting");
  // 关闭所有打开的文件
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }
  // 减少对内存中inode的引用
  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;
  // we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);
  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);
  
  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);
  acquire(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup1(original_parent);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&original_parent->lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}
```

`reparent`

```c
// Pass p's abandoned children to init.
// Caller must hold p->lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    // this code uses pp->parent without holding pp->lock.
    // acquiring the lock first could cause a deadlock
    // if pp or a child of pp were also in exit()
    // and about to try to lock p.
    if(pp->parent == p){
      // pp->parent can't change between the check and the acquire()
      // because only the parent changes it, and we're the parent.
      acquire(&pp->lock);
      pp->parent = initproc;
      // we should wake up init here, but that would require
      // initproc->lock, which would be a deadlock, since we hold
      // the lock on one of init's children (pp). this is why
      // exit() always wakes init (before acquiring any locks).
      release(&pp->lock);
    }
  }
}
```

Exit 允许一个进程自行终止，而 kill则允许一个进程请求另一个进程终止。`如果让 kill 直接摧毁进程，那就太复杂了，因为相应进程可能在另一个 CPU 上执行，也许正处于更新内核数据结构的敏感序列中`。因此，kill 的作用很小：它只是设置进程的 p->killed，如果它在 sleep，则 wakeup 它。最终，进程会进入或离开内核，这时如果p->killed 被设置，usertrap 中的代码会调用 exit。如果进程在用户空间运行，它将很快通过进行系统调用或因为定时器（或其他设备）中断而进入内核。

`如果进程处于睡眠状态，kill 调用 wakeup 会使进程从睡眠中返回。这是潜在的危险，因为正在等待的条件可能不为真。然而，xv6 对 sleep 的调用总是被包裹在一个 while 循环中，在 sleep 返回后重新检测条件。`一些对 sleep 的调用也会在循环中检测 p->killed，如果设置了 p->killed，则离开当前活动。只有当这种离开是正确的时候才会这样做。例如，管道读写代码如果设置了 killed 标志就会返回；最终代码会返回到 trap，trap 会再次检查标志并退出。

`一些 xv6 sleep 循环没有检查 p->killed，因为代码处于多步骤系统调用的中间，而这个调用应该是原子的。`==virtio 驱动就是一个例子：它没有检查 p->killed，因为磁盘操作可能是一系列写操作中的一个，而这些写操作都是为了让文件系统处于一个正确的状态而需要的。==一个在等待磁盘 I/O 时被kill的进程不会退出，直到它完成当前的系统调用和 usertrap 看到 killed 的标志。

`kill`

```c
// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}
```

### 第八章：文件系统

文件系统的目的是`组织和存储数据`。文件系统通常支持用户和应用程序之间的数据共享，以及支持持久性，以便数据在重启后仍然可用。

xv6 文件系统提供了类 Unix 的文件、目录和路径名，并将其数据存储在virtio 磁盘上以实现持久化。该文件系统解决了几个挑战：

- 文件系统需要磁盘上的数据结构来表示命名目录和文件的树，记录保存每个文件内容的块的身份，并记录磁盘上哪些区域是空闲的。
- 文件系统必须支持崩溃恢复。也就是说，如果发生崩溃如电源故障，文件系统必须在重新启动后仍能正常工作。风险在于，崩溃可能会中断更新序列，并在磁盘上留下不一致的数据结构（例如，一个块既在文件中使用，又被标记为空闲）。
- 不同的进程可能并发在文件系统上运行，所以文件系统代码必须协调维护每一个临界区。
- 访问磁盘的速度比访问内存的速度要慢几个数量级，所以文件系统必须维护缓存，用于缓存常用块。

#### 8.1概述

![image-20221117091925084](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221117091925084.png)

==xv6 文件系统的实现分为七层，如图 8.1 所示。磁盘层在 virtio 磁盘上读写块。Buffer缓存层缓存磁盘块，并同步访问它们，确保一个块只能同时被内核中的一个进程访问。日志层允许上层通过事务更新多个磁盘块，并确保在崩溃时，磁盘块是原子更新的（即全部更新或不更新）。inode 层将一个文件都表示为一个 inode，每个文件包含一个唯一的 i-number 和一些存放文件数据的块。目录层将实现了一种特殊的 inode，被称为目录，其包含一个目录项序列，每个目录项由文件名称和 i-number 组成。路径名层提供了层次化的路径名，如/usr/rtm/xv6/fs.c，可以用递归查找解析他们。文件描述符层用文件系统接口抽象了许多 Unix 资源（如管道、设备、文件等），使程序员的生产力得到大大的提高。==

![image-20221117092252966](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221117092252966.png)

文件系统必须安排好磁盘存储 inode 和内容块的位置。为此，xv6 将磁盘分为几个部分，如图8.2所示。文件系统不使用块0（它存放boot sector）。第1块称为superblock，它包含了文件系统的元数据（以块为单位的文件系统大小、数据块的数量、inode 的数量和日志中的块数）。从块 2 开始存放着日志。日志之后是 inodes，每个块会包含多个inode。在这些块之后是位图块(bitmap)，记录哪些数据块在使用。其余的块是数据块，每个数据块要么在位图块中标记为空闲，要么持有文件或目录的内容。超级块由一个单独的程序 mkfs 写入，它建立了一个初始文件系统。

```c
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
```

本章的其余部分将讨论每一层，从 buffer 缓存开始。低层使用选择合适抽象，可以方便更高层的设计。

#### 8.2Buffer缓存层

`buffer缓存有两项工作。(1)同步访问磁盘块，以确保磁盘块在内存中只有一个buffer缓存，并且一次只有一个内核线程能使用该 buffer 缓存；(2)缓存使用较多的块，这样它们就不需要从慢速磁盘中重新读取。`

buffer 缓存的主要接口包括 **bread** 和 **bwrite**，bread 返回一个在内存中可以读取和修改的块副本 **buf**，**bwrite** 将修改后的 buffer 写到磁盘上相应的块。内核线程在使用完一个 buffer 后，必须通过调用 **brelse** 释放它。buffer 缓存为每个 buffer 都设有 sleeplock，以确保每次只有一个线程使用 buffer（从而使用相应的磁盘块）；**bread** 返回的buffer 会被锁定，而 **brelse** 释放锁。

我们再来看看 buffer 缓存。buffer 缓存有固定数量的 buffer 来存放磁盘块，这意味着如果文件系统需要一个尚未被缓存的块，buffer 缓存必须回收一个当前存放其他块的buffer。buffer 缓存为新块寻找最近使用最少的 buffer（lru 机制）。因为最近使用最少的buffer 是最不可能被再次使用的 buffer。

`buf.h`

```c
struct buf {
  int valid;   // has data been read from disk? 是否从磁盘读取了数据
  int disk;    // does disk "own" buf? 缓冲区的内容已经被修改需要被重新写入磁盘
  uint dev;    // 设备号
  uint blockno;// 块号
  struct sleeplock lock; // 保证进程同步访问buf
  uint refcnt; // 引用计数
  struct buf *prev; // LRU cache list
  struct buf *next; // LRU cache list
  uchar data[BSIZE];
};
```

buffer 缓存是一个由 buffer 组成的双端链表。由函数 binit 用静态数组 buf 初始化这个链表， binit 在启动时由 main调用。访问 buffer 缓存是通过链表，而不是buf 数组。

`binit`

```c
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;// bcache的缓存更新策略是LRU
} bcache;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.lock, "bcache");
  // 创建buffer的双端链表
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}
```

buffer 有两个与之相关的状态字段。字段 valid 表示是否包含该块的副本（是否从磁盘读取了数据）。字段 disk 表示缓冲区的内容已经被修改需要被重新写入磁盘。

bget扫描 buffer 链表，寻找给定设备号和扇区号来查找缓冲区。如果存在，bget 就会获取该 buffer 的 sleeplock。然后 bget 返回被锁定的 buffer。

如果给定的扇区没有缓存的 buffer，bget 必须生成一个，可能会使用一个存放不同扇区的 buffer，它再次扫描 buffer 链表，寻找没有被使用的 buffer(b->refcnt = 0)；任何这样的 buffer 都可以使用。任何这样的 buffer 都可以使用。bget 修改 buffer 元数据，记录新的设备号和扇区号，并获得其 sleeplock。请注意，b->valid = 0 可以确保 bread 从磁盘读取块数据，而不是错误地使用 buffer 之前的内容。

请注意，每个磁盘扇区最多只能有一个 buffer，以确保写操作对读取者可见，也因为文件系统需要使用 buffer 上的锁来进行同步。Bget 通过从第一次循环检查块是否被缓存，第二次循环来生成一个相应的 buffer（通过设置 dev、blockno 和 refcnt），在进行这两步操作时，需要一直持有 bcache.lock 。持有 bcache.lock 会保证上面两个循环在整体上是原子的。

bget 在 bcache.lock 保护的临界区之外获取 buffer 的 sleeplock 是安全的，因为非零的 b->refcnt 可以防止缓冲区被重新用于不同的磁盘块。sleeplock 保护的是块的缓冲内容的读写，而 bcache.lock 保护被缓存块的信息。

如果所有 buffer 都在使用，那么太多的进程同时在执行文件相关的系统调用，bget 就会 panic。一个更好的处理方式可能是睡眠，直到有 buffer 空闲，尽管这时有可能出现死锁。

```c
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  // 查找的块之前已被缓存，从前往后
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 利用LRU（链表尾部是最少使用的）依次查找直到找到一个未被使用的buf，从后往前
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      // b->valid = 0 可以确保 bread 从磁盘读取块数据，而不是错误地使用 buffer 之前的内容
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}
```

一旦 bread 读取了磁盘内容（如果需要的话）并将缓冲区返回给它的调用者，调用者就独占该buffer，可以读取或写入数据。如果调用者修改了 buffer，它必须在释放 buffer 之前调用 bwrite 将修改后的数据写入磁盘。bwrite调用 virtio_disk_rw 与磁盘硬件交互。

`bread`

```c
// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  // 寻找目标缓存块
  b = bget(dev, blockno);
  // 如果是新分配的buf(b->valid==0)，需要从磁盘上重新读取
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}
```

`bwrite`

```c
// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  // 如果调用者修改了 buffer，它必须在释放 buffer 之前调用 bwrite 将修改后的数据写入磁盘
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}
```

当调用者处理完一个buffer后，必须调用brelse来释放它。(brelse这个名字是b-release的缩写，虽然很神秘，但值得学习，它起源于 Unix，在 BSD、Linux 和 Solaris 中也有使用。) brelse释放 sleeplock，并将该 buffer 移动到链表的头部。移动 buffer 会使链表按照 buffer 最近使用的时间（最近释放）排序，链表中的第一个buffer 是最近使用的，最后一个是最早使用的。bget 中的两个循环利用了这一点，在最坏的情况下，获取已缓存 buffer 的扫描必须处理整个链表，由于数据局部性，先检查最近使用的缓冲区（从 bcache.head 开始，通过 next 指针）将减少扫描时间。扫描选取可使用 buffer的方法是通过从后向前扫描（通过 prev 指针）选取最近使用最少的缓冲区。

`brelse`

```c
// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  // buf使用完毕可以释放持有的buf的sleep锁了
  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  // buf没有进程使用，则把buf移至LRU链表表头（最近使用，可能后面很快就用到）
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  release(&bcache.lock);
}
```

> 有时候为了保证块缓存不会立即失效回收，使用bpin手动增加buf的引用计数，等到不需要再使用buf的时候使用bunpin减小buf的引用计数。

```c
void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}
```

#### 8.3Logging日志层

文件系统设计中最有趣的问题之一是崩溃恢复。这个问题的出现是因为许多文件系统操作涉及到对磁盘的多次写入，如果只执行了部分写操作，然后发生崩溃可能会使磁盘上的文件系统处于不一致的状态。例如，假设在文件截断（将文件的长度设置为零并释放其内容块）时发生崩溃。根据磁盘写入的顺序，可能会留下一个引用空闲内容块的 inode，也可能会留下一个已分配但没有被引用的内容块。

后面的这种情况相对来说好一点，但是如果一个 inode 指向被释放的块，很可能在重启后造成严重的问题。重启后，内核可能会将该块分配给另一个文件，现在我们有两个不同的文件无意中指向了同一个块。如果 xv6 支持多用户，这种情况可能是一个安全问题，因为旧文件的所有者能够读写新文件，即使该文件被另一个用户所拥有。

`Xv6 通过简单的日志系统来解决文件系统操作过程中崩溃带来的问题。xv6 的系统调用不直接写磁盘上的文件系统数据结构。相反，它将写入的数据记录在磁盘上的日志中。一旦系统调用记录了全部的写入数据，它就会在磁盘上写一个特殊的提交记录，表明该日志包含了一个完整的操作。这时，系统调用就会将日志中的写入数据写到磁盘上相应的位置。在执行完成后，系统调用将磁盘上的日志清除。`

如果系统崩溃并重启，文件系统会在启动过程中恢复自己。如果日志被标记为包含一个完整的操作，那么恢复代码就会将写入的内容复制到它们在磁盘文件系统中的相应位置。如果日志未被标记为包含完整的操作，则恢复代码将忽略并清除该日志。

为什么 xv6 的日志系统可以解决文件系统操作过程中的崩溃问题？==如果崩溃发生在操作提交之前，那么磁盘上的日志将不会被标记为完成，恢复代码将忽略它，磁盘的状态就像操作根本没有开始一样。如果崩溃发生在操作提交之后，那么恢复代码会重新执行写操作，可能会重复执行之前的写操作。不管是哪种情况，日志都会使写与崩溃为原子的，即恢复后，所有操作的写入内容，要么都在磁盘上，要么都不在。==

:rabbit:日志的设计方案

日志贮存在一个固定位置，由 **superblock** 指定。它由一个 header 块组成，后面是一连串的更新块副本（日志块）。header 块包含一个扇区号数组，其中的每个扇区号都对应一个日志块，header 还包含日志块的数量。磁盘上 header 块中的数量要么为零，表示日志中没有事务，要么为非零，表示日志中包含一个完整的提交事务，并有指定数量的日志块。Xv6在事务提交时会修改 header 块，将日志块复制到文件系统后，会将数量设为零。==因此，一个事务中途的崩溃将导致日志 header 块中的计数为零；提交后的崩溃的计数为非零。==

```c
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE (MAXOPBLOCKS*3) // max data blocks in on-disk log磁盘日志块最大数量

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
  int outstanding; // how many FS sys calls are executing.活跃的文件系统调用数，操作的磁盘日志块数默认最大是MAXOPBLOCKS，事务结束时实际写入日志块数就是确定的
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;
```

为了应对崩溃，每个系统调用都包含一个原子写序列。为了允许不同进程并发执行文件系统操作，日志系统可以将多个系统调用的写操作累积到一个事务中。`因此，一次提交可能涉及多个完整系统调用的写入。为了避免一个系统调用被分裂到不同的事务中，只有在没有文件系统相关的系统调用正在进行时，日志系统才会提交。`

将几个事务一起提交的方法被称为组提交（group commit）。组提交可以减少磁盘操作的次数，因为它将提交的固定成本分摊在了多个操作上。组提交可以让文件系统同时执行更多的并发写，也可以让磁盘在一次磁盘轮转中把它们全部写入。Xv6 的 virtio 驱动不支持这种批处理，但 xv6 的文件系统实现了这种方式。

==Xv6 在磁盘上划出固定的空间来存放日志。在一个事务中，系统调用所写的块总数必须适应这个空间的大小。==这将导致两个后果：

1、`系统调用写入的日志大小必须小于日志空间的大小。`这对大多数系统调用来说都不是问题，但有两个系统调用可能会写很多块，**write** 和 **unlink**。大文件的 write 可能会写很多数据块和 bitmap 块，以及一个 inode 块；取消链接一个大文件可能会写很多 bitmap 块和一个 inode。Xv6 的 **write** 系统调用将大的写操作分解成多个小的写操作，以适应在日志空间的大小，而 **unlink** 不会引起问题，因为 xv6 文件系统只使用一个位图块。

2、`日志空间有限的另一个后果是，日志系统只会在确定了系统调用的写操作可以适应剩余日志空间之后，才会开始执行该系统调用。`

系统调用中一般用法如下：

```c
begin_op();
...
bp = bread(...);
bp->data[...] = ...;
log_write(bp);
...
end_op();
```

`begin_op会一直等到日志系统没有 commiting，并且有足够的日志空间来容纳这次调用的写。`log.outstanding统计当前系统调用的数量，可以通过log.outstanding 乘以 MAXOPBLOCKS 来计算已使用的日志空间。==自增 log.outstanding 既能预留空间，又能防止该系统调用期间进行提交。该代码假设每次系统调用最多写入MAXOPBLOCKS 个块。==

`begin_op`

```c
// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    // 会一直等到日志系统没有 commiting
    if(log.committing){
      sleep(&log, &log.lock);
    // 并且有足够的日志空间来容纳这次调用的写，活跃中的文件系统使用的日志空间默认为最大值MAXOPBLOCKS，比较悲观和宽泛，实际可能比这要小得多
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
```

`log_write是 bwrite 的代理。它将扇区号记录在内存中，在磁盘上的日志中使用一个槽，并自增buffer.refcnt 防止该 buffer 被重用。`在提交之前，块必须留在缓存中，即该缓存的副本是修改的唯一记录；在提交之后才能将其写入磁盘上的位置；该次修改必须对其他读可见。 注意，当一个块在一个事务中被多次写入时，他们在日志中的槽是相同的。这种优化通常被称为 absorption(吸收)。例如，在一个事务中，包含多个文件的多个 inode 的磁盘块被写多次，这是常见的情况。`通过将几次磁盘写吸收为一次，文件系统可以节省日志空间，并且可以获得更好的性能，因为只有一份磁盘块的副本必须写入磁盘。`

```c
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
```

end_op首先递减 log.outstanding。如果计数为零，则通过调用commit()来提交当前事务。

> commit 分为四个阶段：
>
> 1、write_log()将事务中修改的每个块从 buffer 缓存中复制到磁盘上的日志槽中。
>
> 2、write_head()将 header 块写到磁盘上，就表明已提交，为提交点，写完日志后的崩溃，会导致在重启后重新执行日志。
>
> 3、install_trans从日志中读取每个块，并将其写到文件系统中对应的位置。
>
> 4、最后修改日志块计数为 0，并写入日志空间的 header 部分。这必须在下一个事务开始之前修改，这样崩溃就不会导致重启后的恢复使用这次的 header 和下次的日志块。

`end_op`

```c
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
```

`commit四部曲`

```c
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
```

`write_log`

```c
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
```

> 需要注意一点：磁盘的日志区第一块是logheader使用的，后面才供事务的数据缓存使用。

`write_head`

```c
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
```

`install_trans`

```c
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
    // log_write()时曾经调用bpin(buf)保证缓冲区不会被重用，现在使用完buf可以回收释放了
    bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}
```

recover_from_log 是 在 initlog 中 调 用 的 ， 而initlog 是在第一个用户进程运行之前, 由 fsinit 调用的。它读取日志头，如果日志头显示日志中包含一个已提交的事务，则会像 end_op 那样执行日志。

一个使用了日志的例子是 filewrite。这个事务看起来像这样：

```c
begin_op();
ilock(f->ip);
r = writei(f->ip, ...);
iunlock(f->ip);
end_op();
```

这段代码被包裹在一个循环中，它将大的写分解成每次只有几个扇区的单独事务，以避免溢出日志空间。调用 writei 写入许多块作为这个事务的一部分：文件的 inode，一个或多个 bitmap 块，以及一些数据块。

`initlog`

```c
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
```

`recover_from_log`

```c
// 它读取日志头，如果日志头显示日志中包含一个已提交的事务，则会像 end_op 那样执行日志。
static void
recover_from_log(void)
{
  read_head();
  install_trans(); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}
```

`read_head`

```c
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
```

#### 8.4块分配器

`文件和目录存储在磁盘块中，必须从空闲池中分配，xv6 的块分配器在磁盘上维护一个bitmap，每个块对应一个位。0 表示对应的块是空闲的，1 表示正在使用中。程序 mkfs 设置引导扇区、超级块、日志块、inode 块和位图块对应的位。`

块分配器提供了两个函数：balloc 申请一个新的磁盘块，bfree 释放一个块。balloc会有一个循环遍历每一个块，从块 0 开始，直到 sb.size，即文件系统中的块数。它寻找一个位为 0 的空闲块。如果 balloc 找到了这样一个块，它就会更新 bitmap 并返回该块。为了提高效率，这个循环被分成两部分。外循环读取 bitmap 的一个块，内循环检查块中的所有 BPB 位。如果两个进程同时试图分配一个块，可能会发生竞争，但 buffer 缓存只允许块同时被一个进程访问，这就避免了这种情况的发生。

Bfree找到相应的 bitmap 块并清除相应的位。bread 和 brelse 暗含的独占性避免了显式锁定。

`文件系统的初始化`

```c
// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb; 
// Read the super block.
static void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;
  // 块1是超级块
  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Init fs
void
fsinit(int dev) {
  readsb(dev, &sb);
  if(sb.magic != FSMAGIC)
    panic("invalid file system");
  initlog(dev, &sb);
}
```

`balloc`

```c
// Bitmap bits per block 每块的bmap比特数
#define BPB           (BSIZE*8)
// Block of free map containing bit for block b 块b所在的bmap块号
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for(b = 0; b < sb.size; b += BPB){
    // 外层循环遍历每一个bmap块
    bp = bread(dev, BBLOCK(b, sb));
    // Bitmap上的每一个比特位表示一个数据块的状态
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      // Bitmap中对应的bit位是0表示块空闲
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        // 块标记为已使用
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}
```

`bfree`

```c
// Free a disk block.
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}
```

`bzero`

```c
// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}
```

#### 8.5Inode层

术语 inode 有两种相关的含义。1、它可能指的是磁盘上的数据结构，其中包含了文件的大小和数据块号的列表；2、inode 可能指的是内存中的 inode，它包含了磁盘上 inode 的副本以及内核中需要的其他信息。

磁盘上的 inode 被放置磁盘的一个连续区域。每一个 inode 的大小都是一样的，所以，给定一个数字 n，很容易找到磁盘上的第 n 个 inode。事实上，这个数字 n，被称为 inode 号或 i-number，在实现中就是通过这个识别 inode 的。

```c
#define BSIZE 1024  // block size
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
```

结构体 dinode定义了磁盘上的 inode。type 字段区分了文件、目录和特殊文件（设备）。type 为 0 表示该 inode 是空闲的。nlink 字段统计引用这个 inode 的目录项的数量，当引用数为 0 时就释放磁盘上的 inode 及其数据块。size 字段记录了文件中内容的字节数。addrs 数组记录了持有文件内容的磁盘块的块号。

```c
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
```

内核将在使用的 inode 保存在内存中；结构体 inode 是磁盘 dinode 的拷贝。内核只在有指针指向inode 才会储存。ref 字段为指向 inode 的指针的数量，如果ref减少到零，内核就会从内存中丢弃这个 inode。iget 和 iput 函数引用和释放 inode，并修改ref。指向 inode 的指针可以来自文件描述符，当前工作目录，以及短暂的内核代码，如 exec。

在 xv6 的 inode 代码中，有四种锁或类似锁的机制。==icache.lock 保证了一个 inode 在缓存只有一个副本，以及缓存 inode 的 ref 字段计数正确。每个内存中的 inode 都有一个包含sleeplock 的锁字段，它保证了可以独占访问 inode 的其他字段（如文件长度）以及 inode的文件或目录内容块的。一个 inode 的 ref 如果大于 0，则会使系统将该 inode 保留在缓存中，而不会重用该 inode。最后，每个 inode 都包含一个 nlink 字段(在磁盘上，缓存时会复制到内存中)，该字段统计链接该 inode 的目录项的数量；如果一个 inode 的链接数大于零，xv6 不会释放它。==

iget()返回的 inode 指针在调用 iput()之前都是有效的；inode 不会被删除，指针所引用的内存也不会被另一个 inode 重新使用。`iget()提供了对 inode 的非独占性访问，因此可以有许多指针指向同一个inode。文件系统代码中的许多部分都依赖于 iget()的这种行为，既是为了保持对 inode 的长期引用(如打开的文件和当前目录)，也是为了防止竞争，同时避免在操作多个 inode 的代码中出现死锁(如路径名查找)。`

Iget在 inode 缓存中寻找一个带有所需设备号和 inode 号的active 条目 (ip->ref > 0)。如果它找到了，它就返回一个新的对该 inode 的 引用。当 iget 扫描时，它会记录第一个空槽的位置 ，当它需要分配一个缓存条目时，它会使用这个空槽。

```c
// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;
  // 获得整个icache锁
  acquire(&icache.lock);
  // Is the inode already cached?
  // 如果inode已被缓存则直接返回，否则记录下icache中空闲项
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }
  // 回收一个icache项
  if(empty == 0)
    panic("iget: no inodes");
  // 使用icache的空闲项
  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  // 空inode项必须从磁盘上读取内容
  ip->valid = 0;
  release(&icache.lock);
  return ip;
}
```

`inode 缓存只缓存被指针指向的 inode。它的主要工作其实是同步多个进程的访问，缓存是次要的。`如果一个 inode 被频繁使用，如果不被 inode 缓存保存，buffer 缓存可能会把它保存在内存中。`inode 缓存是 write-through 的，这意味着缓存的 inode 被修改，就必须立即用 iupdate 把它写入磁盘。`

```c
struct {
  // icache.lock 保证了一个 inode 在缓存只有一个副本，以及缓存 inode 的 ref 字段计数正确
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void
iinit()
{
  int i = 0;
  
  initlock(&icache.lock, "icache");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }
}
```

要创建一个新的 inode(例如，当创建一个文件时)，xv6 会调用 ialloc。Ialloc 类似于 balloc：它遍历磁盘上的 inode ，寻找一个被标记为空闲的 inode。当它找到后，它会修改该 inode 的 type 字段来使用它，最后调用 iget  来从 inode 缓存中返回一个条目。==由于一次只能有一个进程持有对 bp 的引用，所以 ialloc 可以正确执行。ialloc 可以确保其他进程不会同时看到 inode 是可用的并使用它。==

```c
// Inodes per block. 一个块的inode数量
#define IPB           (BSIZE / sizeof(struct dinode))

// Allocate an inode on device dev.
// Mark it as allocated by giving it type type.
// Returns an unlocked but allocated and referenced inode.
// 分配设备(磁盘)上的一个dinode，类型标记为type
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    // 由于一次只能有一个进程持有对 bp 的引用，所以 ialloc 可以正确执行。
    // ialloc 可以确保其他进程不会同时看到 inode 是可用的并使用它。
    // 第一次需要磁盘读，后面都是直接从块缓存中拿
    bp = bread(dev, IBLOCK(inum, sb));
    // 具体的dinode
    dip = (struct dinode*)bp->data + inum%IPB;
    // 空闲的dinode分配使用
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}
```

`在读写 inode 的元数据或内容之前，代码必须使用 ilock 锁定它。`Ilock使用 sleeplock 内部有一个睡眠锁来锁定。`一旦 ilock 锁定了 inode，它就会根据自己的需要从磁盘（更有可能是Inode缓存）读取 inode。`函数 iunlock释放睡眠锁，这会唤醒正在等待该睡眠锁的进程。

```c
// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;
  // 合法性检查
  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  // inode加睡眠锁
  acquiresleep(&ip->lock);
  // inode无效需要从磁盘上重新读取
  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    // 磁盘上的相关数据结构是dinode
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    // valid有效位置一
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
// 给指定的inode解锁
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}
```

`Iput通过递减引用次数释放指向 inode 的指针。如果递减后的引用数为 0，inode 缓存中就会释放掉该 inode 在 inode 缓存中的槽位，该槽位就可以被其他 inode 使用。`

==如果 iput 发现没有指针指向该 inode，并且没有任何目录项链接该 inode（不在任何目录中出现），那么该 inode 和它的数据块必须被释放。Iput 调用 itrunc 将文件截断为零字节，释放数据块；将 inode 类型设置为 0（未分配）；并将 inode 写入磁盘。==

> iput 在释放 inode 的锁定协议是值得我们仔细研究。一个危险是，一个并发线程可能会在 ilock 中等待使用这个 inode(例如，读取一个文件或列出一个目录)，但它没有意识到该inode 可能被释放掉了。这种情况是不会发生，因为该 inode 的没有被目录项链接且 ip->ref为 1，那么系统调用是没有这个指针的（如果有，ip->ref 应该为 2）。这一个引用是调用 iput 的线程所拥有的。的确，iput 会在其 icache.lock 锁定的临界区之外检查引用数是否为 1，但此时已知链接数为 0，所以没有线程会尝试获取新的引用。另一个主要的危险是，并发调用 ialloc 可能会使 iput 返回一个正在被释放的 inode。这种情况发生在 iupdate 写磁盘时ip->type=0。这种竞争是正常的，分配 inode 的线程会等待获取 inode 的睡眠锁，然后再读取或写入 inode，但此时 iput 就结束了。

`iput`

```c
// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
// 撤销一个inode的ref引用
void
iput(struct inode *ip)
{
  acquire(&icache.lock);

  // inode has no links and no other references: truncate and free.
  if(ip->ref == 1 && ip->valid && ip->nlink == 0){
    // ip->ref == 1 means no other process can have ip locked,
    // so this acquiresleep() won't block (or deadlock).
    acquiresleep(&ip->lock);
    release(&icache.lock);
    // 调用 itrunc 将文件截断为零字节，释放数据块
    itrunc(ip);
    // 将 inode 类型设置为 0（未分配），并将 inode 写入磁盘
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);
    acquire(&icache.lock);
  }
  ip->ref--;
  release(&icache.lock);
}
```

> itrunc 释放文件的块，将 inode 的大小重置为零。Itrunc首先释放直接块，然后释放间接块中指向的块，最后释放间接块本身。

`itrunc`

```c
// Truncate inode (discard contents).
// Caller must hold ip->lock.
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  // 释放文件的直接关联块
  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  // 释放文件的间接关联块
  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      // 释放文件的间接关联块
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    // 释放存放间接块号的块
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  // 文件截断后更新信息完毕，写回磁盘
  ip->size = 0;
  iupdate(ip);
}
```

`iupdate`

```c
// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
// Inode缓存是直写的，修改了inode后需要写回磁盘上的dinode
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}
```

> iput()会写磁盘。这意味着任何使用文件系统的系统调用都会写磁盘，因为系统调用可能是最后一个对文件有引用的调用。甚至像 read()这样看似只读的调用，最终也可能会调用iput()。这又意味着，即使是只读的系统调用，如果使用了文件系统，也必须用事务来包装。

崩溃发生在 iput()中是相当棘手的。`当文件的链接数降到零时，iput()不会立即截断一个文件，因为一些进程可能仍然在内存中持有对 inode 的引用：一个进程可能仍然在对文件进行读写，因为它成功地打开了 inode。但是，如果崩溃发生在该文件的最后一个文件描述符释放时，那么该文件将被标记为已在磁盘上分配，但没有目录项指向它。`

文件系统处理这种情况的方法有两种。简单的解决方法是，是在重启后的恢复时，文件系统会扫描整个文件系统，寻找那些被标记为已分配的文件，但没有指向它们的目录项。如果有这样的文件存在，那么就可以释放这些文件。

第二种解决方案不需要扫描文件系统。在这个解决方案中，文件系统在磁盘上（例如，在 superblock 中）记录链接数为 0 但引用数不为 0 的文件的 inode 的 inumber，文件系统在其引用计数达到 0 时删除该文件 ，同时它更新磁盘上的列表，从列表中删除该 inode。恢复时，文件系统会释放列表中的任何文件。

==Xv6 没有实现这两种解决方案，这意味着 inode 可能会在磁盘上被标记分配，即使它们不再使用。这意味着随着时间的推移，xv6 可能会面临磁盘空间耗尽的风险。==

![image-20221117163303777](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221117163303777.png)

磁盘上的 inode，即 dinode 结构体，包含一个 size 和一个块号数组（见图 8.3）。inode数据可以在 dinode 的 addrs 数组中找到。开始的 NDIRECT 个数据块放置在数组中的前NDIRECT 个条目中，这些块被称为直接块。接下来的 NINDIRECT 个数据块并没有放置在inode 中，而是被存放在叫做间接块的数据块中。addrs 数组中的最后一个条目给出了放置间接块的地址。因此，一个文件的前 12 kB ( NDIRECT x BSIZE)字节可以从 inode 中列出的块中加载，而接下来的 256 kB ( NINDIRECT x BSIZE)字节只能在查阅间接块后才能取出。对于磁盘这是一种不错的表示方式，但对客户机就有点复杂了。函数 bmap 是包装了这种表示方式的高层次函数，以便于 readi 和 writei 可以更好的使用。Bmap 返回 inode ip 的第 bn 个数据块的磁盘块号。如果 ip 没有第 bn 个的数据块，bmap 就会分配一个。

函数 bmap从简单的情况开始：最前面的 NDIRECT 个块储存在ip->addrs[0]~ip->addrs[NDIRECT-1]中，接下来的 NINDIRECT 个块放置在 ip->addrs[NDIRECT]指向的的间接块中。Bmap 读取间接块，然后从块内的正确的位置读取一个块号如果块号超过了 NDIRECT+NINDIRECT，bmap 就会 panic；writei 会检查并防止这种情况。

`bmap`

```c
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}
```

Bmap 使 得 readi 和 writei 可以很容易地获取一个 inode 的数据。 Readi 首先要确定偏移量和计数没有超过文件末端。从文件超出末尾开始的读会返回一个错误，而从文件末尾开始或读取过程中超出末尾的读会不会返回错误，只是返回的字节数会少于请求的字节数。

`readi`

```c
// Read data from inode.
// Caller must hold ip->lock.
// If user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
int
readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  // 文件偏移量不能超过文件大小或者为负
  if(off > ip->size || off + n < off)
    return 0;
  // 不能读取超出文件大小的数据
  if(off + n > ip->size)
    n = ip->size - off;

  // 读取n字节的数据到dst地址处
  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      brelse(bp);
      break;
    }
    brelse(bp);
  }
  return tot;
}
```

主循环会把文件中的每一个块的数据复制到 dst 中。writei与 readi 相同，但有2个不同：

- 从文件末尾开始或越过文件末尾的写入会使文件增长，但不会超过文件的最大长度；
- 如果写使文件增长了，writi 必须更新它的大小。

`writei`

```c
// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
int
writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  // 文件偏移量不能超过文件大小或者为负
  if(off > ip->size || off + n < off)
    return -1;
  // 写入后文件大小不能超过允许的最大文件大小
  if(off + n > MAXFILE*BSIZE)
    return -1;

  // 写入n字节的数据到off后面
  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
      brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  if(n > 0){
    if(off > ip->size)
      ip->size = off;
    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap() and added a new
    // block to ip->addrs[].
    // 更新后的inode信息写回磁盘
    iupdate(ip);
  }
  return n;
}
```

> readi 和 writei 开始都会检查 ip->type == T_DEV。这种情况处理的是数据不在文件系统中的特殊设备；我们将在文件描述符层中再讨论这种情况。

函数 stati将 inode 元数据复制到 stat 结构体中，通过 stat 系统调用暴露给用户程序。

`stat结构体`

```c
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};
```

`stati`

```c
// Copy stat information from inode.
// Caller must hold ip->lock.
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}
```

#### 8.6目录层

`目录的实现机制和文件很类似。它的 inode 类型是 T_DIR，它的数据是一个目录项的序列。每个条目是一个结构体 dirent，它包含一个名称和一个 inode 号。名称最多包含 DIRSIZ(14)个字符，较短的名称以 NULL(0)结束。inode 号为 0 的目录项是空闲的。`

```c
// Directory is a file containing a sequence of dirent structures.
// 目录名称的最大长度
#define DIRSIZ 14

// directory entry 目录项（inode编号+对应的文件名）
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};
```

`函数 dirlookup 在一个目录中搜索一个带有给定名称的条目。`如果找到了，它返回一个指向相应 inode 的指针，解锁该 inode，并将* poff 设置为目录中条目的字节偏移量，以便调用者编辑它。如果dirlookup 找到一个对应名称的条目，则更新*poff，并返回一个通过 iget 获得的未被锁定的 inode。`dirlookup 是 iget 返回未锁定的 inode 的原因。调用者已经锁定了 dp，所以如果查找的是 “.” ，当前目录的别名，在返回之前试图锁定 inode，就会试图重新锁定 dp 而死锁。还有更复杂的死锁情况，涉及到多个进程和”..”父目录的别名,”.”不是唯一的问题。`

```c
// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  // inode必须是目录类型
  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  // 逐个查找比较目录的dirent序列
  for(off = 0; off < dp->size; off += sizeof(de)){
    // 读取目录inode中的dirent
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    // 目录项为空，跳过
    if(de.inum == 0)
      continue;
    // 找到符合的目录项
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      // 将*poff 设置为目录中条目的字节偏移量
      if(poff)
        *poff = off;
      inum = de.inum;
      // 返回相应的inode
      return iget(dp->dev, inum);
    }
  }

  return 0;
}
```

`函数 dirlink会在当前目录 dp 中创建一个新的目录项，通过给定的名称和 inode 号。`如果名称已经存在，dirlink 将返回一个错误。主循环读取目录项，寻找一个未使用的条目。当它找到一个时，它会提前跳出循环，并将 off 设置为该可用条目的偏移量。否则，循环结束时，将 off 设置为 dp->size。不管是哪种方式，dirlink 都会在偏移量 off 的位置添加一个新的条目到目录中。

```c
// Write a new directory entry (name, inum) into the directory dp.
// 给目录增加一项dirent(name,inum)
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  // 新增的一项dirent的name必须是未存在的
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  // 寻找一个空闲的dirent
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  // (name,inum)信息加入dirent
  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  // dirent写回inode
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}
```

#### 8.7路径命名层

查找路径名会对每一个节点调用一次 dirlookup。Namei 解析路径并返回相应的 inode。函数 nameiparent 是 namei 的一个变种：它返回相应 inode 的父目录inode，并将最后一个元素复制到 name 中。这两个函数都通过调用 namex 来实现。

Namex首先确定路径解析从哪里开始。如果路径以斜线开头，则从根目录开始解析；否则，从当前目录开始解析。然后它使用 skipelem 来遍历路径中的每个元素。循环的每次迭代都必须在当前 inode ip 中查找name。迭代的开始是锁定 ip 并检查它是否是一个目录。如果不是，查找就会失败。(锁定 ip 是必要的，不是因为 ip->type 可能会改变，而是因为在 ilock运行之前，不能保证 ip->type 已经从磁盘载入)。如果调用的是 nameiparent，而且这是最后一个路径元素，按照之前 nameiparent 的定义，循环应该提前停止，最后一个路径元素已经被复制到 name 中，所以 namex 只需要返回解锁的ip。最后，循环使用 dirlookup 查找路径元素，并通过设置 ip = next 为下一次迭代做准备。当循环遍历完路径元素时，它返回 ip。

`namex`

```c
// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  // 路径以斜杠开头，则从根目录开始
  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  // 否则，从当前目录开始
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    // 上级目录必须是T_DIR类型
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    // 到达路径最后一项，直接返回上级目录ip
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    // 在上级目录中查找名字为name的inode节点
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}
```

`idup`

```c
// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
// 增加inode的引用计数
struct inode*
idup(struct inode *ip)
{
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}
```

`skipelem`

```c
// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}
```

`namei`

```c
struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}
```

`nameiparent`

```c
struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
```

namex 可能需要很长的时间来完成：它可能会涉及几个磁盘操作，通过遍历路径名得到的目录的 inode 和目录块（如果它们不在 buffer 缓存中）。==Xv6 经过精心设计，如果一个内核线程对 namex 的调用在阻塞在磁盘 I/O 上，另一个内核线程查找不同的路径名可以同时进行。Namex 分别锁定路径中的每个目录，这样不同目录的查找就可以并行进行。==

这种并发性带来了一些挑战。例如，当一个内核线程在查找一个路径名时，另一个内核线程可能正在取消链接一个目录，这会改变目录数。一个潜在的风险是，可能一个查找线程正在搜索的目录可能已经被另一个内核线程删除了，而它的块已经被另一个目录或文件重用了。

`Xv6 避免了这种竞争。例如，在 namex 中执行 dirlookup 时，查找线程会持有目录的锁，dirlookup 返回一个使用 iget 获得的 inode。iget 会增加 inode 的引用次数。只有从dirlookup 收到 inode 后，namex 才会释放目录上的锁。现在另一个线程可能会从目录中取消链接 inode，但 xv6 还不会删除 inode，因为 inode 的引用数仍然大于零。`

==另一个风险是死锁。例如，当查找". "时，next 指向的 inode 与 ip 相同。在释放对 ip 的锁之前锁定 next 会导致死锁。为了避免这种死锁，namex 在获得对 next 的锁之前就会解锁目录。这里我们再次看到为什么 iget 和 ilock 之间的分离是很重要的。==

#### 8.8文件描述符层

Unix 接口很酷的一点是：`Unix 中的大部分资源都是以文件的形式来表示的，包括控制台、管道等设备，当然还有真实的文件。`文件描述符层就是实现这种统一性的一层。

Xv6 给每个进程提供了自己的打开文件表，或者说文件描述符表，就像我们在第一章中看到的那样。每个打开的文件由一个结构体 file表示，它包装 inode 或管道，也包含一个 I/O 偏移量。`每次调用 open 都会创建一个新的打开文件（一个新的结构体 file），如果多个进程独立打开同一个文件，那么不同的 file 实例会有不同的 I/O 偏移量。`另一方面，一个打开的文件（同一个结构文件）可以在一个进程的文件表中出现多次，也可以在多个进程的文件表中出现。如果一个进程使用 open 打开文件，然后使用 dup 创建别名，或者使用fork 与子进程共享文件，就会出现这种情况。引用计数可以跟踪特定打开文件的引用数量。一个文件的打开方式可以为读，写，或者读写。通过 readable 和 writable 来指明。

```c
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

// map major device number to device functions.
// 通过设备号映射设备读写函数
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
```

系统中所有打开的文件都保存在一个全局文件表中，即 ftable。文件表的功能有: 分配文件(filealloc)、创建重复引用(fileup)、释放引用(fileclose)、读写数据(fileeread和filewrite)。

```c
// 设备读写函数结构(通过主设备号映射)
struct devsw devsw[NDEV];
// 文件表
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;
void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}
```

前三个函数应该比较熟悉了,就不过多的讨论。==filealloc扫描文件表，寻找一个未引用的文件 (f->ref == 0)，并返回一个新的引用；fileup增加引用计数；fileclose减少引用计数。当一个文件的引用数达到 0 时，fileclose会根据类型释放底层的管道或 inode。==

`filealloc`

```c
// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}
```

`filedup`

```c
// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}
```

`fileclose`

```c
/ Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  // ref==0表示文件空闲，type==FR_NONE
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);
  // 文件关闭inode信息写回磁盘
  if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, ff.writable);
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    begin_op();
    // 文件关闭后需要撤销一个inode的ref引用
    iput(ff.ip);
    end_op();
  }
}
```

`filestat`

```c
// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}
```

函 数 filestat 、 fileread 和 filewrite实现了对文件的统计 、读和写操作 。Filestat只允许对 inodes 进行操作，并调用 stati。`Fileread 和 filewrite 首先检查打开模式是否允许该操作，然后再调用管道或 inode 的相关实现。如果文件代表一个inode，fileread 和 filewrite 使用 I/O 偏移量作为本次操作的偏移量，然后前移偏移量。Pipes 没有偏移量的概念`。回想一下 inode的函数需要调用者处理锁的相关操作。inode 加锁附带了一个不错的作用，那就是读写偏移量是原子式更新的，这样多个进程写一个文件时，自己写的数据就不会被其他进程所覆盖，尽管他们的写入可能最终会交错进行。

`fileread`

```c
// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  // 检查文件读写权限
  if(f->readable == 0)
    return -1;
  // 根据文件类型选择不同的读取函数
  if(f->type == FD_PIPE){
    r = piperead(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}
```

`filewrite`

```c
// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  // 检查文件读写权限
  if(f->writable == 0)
    return -1;

  // 根据文件类型选择不同的写入函数
  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}
```

> 注意：fork的子进程与父进程共享file结构体，也就是说对于文件的修改导致的偏移量的变化，彼此都是可见的。

```c
// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  // Allocate process.
  // 分配一个进程
  if((np = allocproc()) == 0){
    return -1;
  }
  // Copy user memory from parent to child.
  // 父进程的内存完整拷贝到子进程中
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  // 保存父子关系
  np->parent = p;
  // copy saved user registers.
  // 复制保存的用户寄存器信息（trapframe）
  *(np->trapframe) = *(p->trapframe);
  // Cause fork to return 0 in the child.
  // a0寄存器是子进程的fork返回值
  np->trapframe->a0 = 0;
  // increment reference counts on open file descriptors.
  // 增加打开的文件描述符的引用计数
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);
  safestrcpy(np->name, p->name, sizeof(p->name));
  // 子进程的进程ID
  pid = np->pid;
  // 子进程可以调度执行
  np->state = RUNNABLE;
  release(&np->lock);
  return pid;
}
```

`allocproc分配空闲进程`

```c
// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  // 从进程表中查找UNUSED状态的进程
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  // 新创建的子进程需要分配新的pid
  p->pid = allocpid();

  // Allocate a trapframe page.
  // 分配一个trapframe页
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  // 得到一个空闲用户页表
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  // 建立新的上下文信息CTX，当内核调度线程选择该进程执行用户线程时从forkret开始执行
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}
```

`forkret`

```c
// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;
  // Still holding p->lock from scheduler.
  release(&myproc()->lock);
  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }
  usertrapret();
}
```

#### 8.9补充

实际操作系统中的 buffer 缓存要比 xv6 的复杂得多，但它有同样的两个目的：`缓存和同步访问磁盘`。`xv6 的 buffer 缓存和 V6 是一样的，使用简单的最近最少使用（LRU）抛弃策略`；可以实现许多更复杂的策略，每种策略都对某些情况有好处，而对其它情况没有好处。`更高效的 LRU 缓存不使用链表，而使用哈希表进行查找，使用堆进行 LRU 抛弃。现代的buffer 缓存通常与虚拟内存系统集成在一起，以支持内存映射的文件。`

Xv6 的日志系统效率低下。提交不能与文件系统系统调用同时发生。系统会记录整个块，即使一个块中只有几个字节被改变。它执行同步的日志写入，一次写一个块，每一个块都可能需要整个磁盘旋转时间。真正的日志系统可以解决所有这些问题。

`日志不是提供崩溃恢复的唯一方法。早期的文件系统在重启期间使用 scavenger（例如UNIX fsck 程序）来检查每个文件和目录以及块和 inode 空闲列表，寻找并解决不一致的地方。`对于大型文件系统来说，清扫可能需要几个小时的时间，而且在某些情况下，这种方式要想获得的数据一致性，其系统调用必须是一致性的。从日志中恢复要快得多，而且在面对崩溃时，会导致系统调用是原子的。

Xv6 使用了与早期 UNIX 相同的 inodes 和目录的基本磁盘布局；这个方案多年来任还在使用。BSD 的 UFS/FFS 和 Linux 的 ext2/ext3 使用基本相同的数据结构。`文件系统布局中最低效的部分是目录，在每次查找过程中需要对所有磁盘块进行线性扫描。`当目录只有几个磁盘块时，这是合理的，但对于持有许多文件的目录来说是昂贵的。微软 Windows 的 NTFS，Mac OS X 的 HFS，以及 Solaris 的 ZFS，`将一个目录在磁盘上实现了平衡树，用于查找块`。这很复杂，但可以保证目录查找的时间复杂度为 O（logn）。

Xv6 对磁盘故障的处理很简单：如果磁盘操作失败，xv6 就会 panic。这是否合理取决于硬件：`如果一个操作系统位于特殊的硬件之上，这种硬件会使用冗余来掩盖故障，也许操作系统看到故障的频率很低，以至于直接 panic 是可以的。另一方面，使用普通磁盘的操作系统应该使用更加优雅的方式来处理异常，这样一个文件中一个块的丢失就不会影响文件系统其他部分的使用。`

Xv6 要求文件系统固定在磁盘设备上，而且大小不能改变。随着大型数据库和多媒体文件对存储要求越来越高，操作系统正在开发消除每个文件系统一个磁盘瓶颈的方法。`基本的方法是将许多磁盘组合成一个逻辑磁盘。硬件解决方案（如 RAID）仍然是最流行的，但目前的趋势是尽可能地在软件中实现这种逻辑。`这些软件实现通常允许丰富的功能，如通过快速添加或删除磁盘来增长或缩小逻辑设备。当然，一个能够快速增长或收缩的存储层需要一个能够做到同样的文件系统：xv6 使用的固定大小的 inode 块阵列在这样的环境中不能很好地工作。将磁盘管理与文件系统分离可能是最简洁的设计，但也有文件系统将两者通过复杂的接口将他们耦合在一起，如 Sun 公司的 ZFS，将两者结合起来。

Xv6 的文件系统缺乏现代文件系统的许多其他功能，例如，它缺乏对快照和增量备份的支持。

`现代 Unix 系统允许用与磁盘存储相同的系统调用来访问许多种类的资源：命名管道、网络连接、远程访问的网络文件系统以及监视和控制接口，如/proc。`这些系统没有 xv6 在fileread 和 filewrite 中的 if 语句，而是通常给每个打开的文件一个函数指针表，每个代表一个操作，调用函数指针来调用该inode 的实现调用。网络文件系统和用户级文件系统提供了将这些调用变成网络 RPC 的函数，并在返回前等待响应。

## 第二部分：XV6操作系统实验

经过第一部分的学习我们已经基本掌握XV6的设计大致机制和思想，本部分的内容仅仅只是为了巩固所学以及扩展知识点，具体可以详见 https://blog.miigon.net/categories/mit6-s081/

### Lab 1: Unix utilities

`实现几个 unix 实用工具，熟悉 xv6 的开发环境以及系统调用。`

#### 1.1Boot xv6 (easy)

准备环境，编译编译器、QEMU，克隆仓库，略过。

```shell
$ git clone git://g.csail.mit.edu/xv6-labs-2020
$ cd xv6-labs-2020
$ git checkout util
$ make qemu
```

#### 1.2sleep (easy)

> Implement the UNIX program sleep for xv6; your sleep should pause for a user-specified number of ticks. A tick is a notion of time defined by the xv6 kernel, namely the time between two interrupts from the timer chip. Your solution should be in the file user/sleep.c.

练手题，记得在 Makefile 中将 sleep 加入构建目标里。

```c
// sleep.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h" // 必须以这个顺序 include，由于三个头文件有依赖关系

int main(int argc, char **argv) {
	if(argc < 2) {
		printf("usage: sleep <ticks>\n");
	}
	sleep(atoi(argv[1]));
	exit(0);
}
```

```makefile
UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
	$U/_sleep\ .   # here !!!
```

#### 1.3pingpong (easy)

> Write a program that uses UNIX system calls to “ping-pong” a byte between two processes over a pair of pipes, one for each direction. The parent should send a byte to the child; the child should print “: received ping", where is its process ID, write the byte on the pipe to the parent, and exit; the parent should read the byte from the child, print ": received pong", and exit. Your solution should be in the file user/pingpong.c.

管道练手题，使用 fork() 复制本进程创建子进程，创建两个管道，分别用于父子之间两个方向的数据传输。

```c
// pingpong.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char **argv) {
	// 创建管道会得到一个长度为 2 的 int 数组
	// 其中 0 为用于从管道读取数据的文件描述符，1 为用于向管道写入数据的文件描述符
	int pp2c[2], pc2p[2];
	pipe(pp2c); // 创建用于 父进程 -> 子进程 的管道
	pipe(pc2p); // 创建用于 子进程 -> 父进程 的管道
	
	if(fork() != 0) { // parent process
		write(pp2c[1], "!", 1); // 1. 父进程首先向发出该字节
		char buf;
		read(pc2p[0], &buf, 1); // 2. 父进程发送完成后，开始等待子进程的回复
		printf("%d: received pong\n", getpid()); // 5. 子进程收到数据，read 返回，输出 pong
		wait(0);
	} else { // child process
		char buf;
		read(pp2c[0], &buf, 1); // 3. 子进程读取管道，收到父进程发送的字节数据
		printf("%d: received ping\n", getpid());
		write(pc2p[1], &buf, 1); // 4. 子进程通过 子->父管道，将字节送回父进程
	}
	exit(0);
}
```

注：序号只为方便理解，实际执行顺序由于两进程具体调度情况不定，不一定严格按照该顺序执行，但是结果相同。

```tex
$ pingpong
4: received ping
3: received pong
$
```

#### 1.4primes (moderate)

> Write a concurrent version of prime sieve using pipes. This idea is due to Doug McIlroy, inventor of Unix pipes. The picture halfway down [this page](http://swtch.com/~rsc/thread/) and the surrounding text explain how to do it. Your solution should be in the file user/primes.c.

十分好玩的一道题hhhhh，使用多进程和管道，每一个进程作为一个 stage，筛掉某个素数的所有倍数。很巧妙的形式实现了多线程的筛法求素数。

```tex
主进程：生成 n ∈ [2,35] -> 子进程1：筛掉所有 2 的倍数 -> 子进程2：筛掉所有 3 的倍数 -> 子进程3：筛掉所有 5 的倍数 -> .....
```

每一个 stage 以当前数集中最小的数字作为素数输出（每个 stage 中数集中最小的数一定是一个素数，因为它没有被任何比它小的数筛掉），并筛掉输入中该素数的所有倍数（必然不是素数），然后将剩下的数传递给下一 stage。最后会形成一条子进程链，而由于每一个进程都调用了 `wait(0);` 等待其子进程，所以会在最末端也就是最后一个 stage 完成的时候，沿着链条向上依次退出各个进程。

> 素数筛法：将一组数feed到一个进程里，先print出最小的一个数，这是一个素数，然后用其他剩下的数依次尝试整除这个素数，如果可以整除，则将其drop，不能整除则将其feed到下一个进程中，直到最后打印出所有的素数。

![image-20221118151758126](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221118151758126.png)

注意最开始的父进程要等待所有子进程exit才能exit

解决思路：采用递归，每次先尝试从左pipe中读取一个数，如果读不到说明已经到达终点，exit，否则再创建一个右pipe并fork一个子进程，将筛选后的数feed进这个右pipe。

```c
// primes.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 一次 sieve 调用是一个筛子阶段，会从 pleft 获取并输出一个素数 p，筛除 p 的所有倍数
// 同时创建下一 stage 的进程以及相应输入管道 pright，然后将剩下的数传到下一 stage 处理
void sieve(int pleft[2]) { // pleft 是来自该 stage 左端进程的输入管道
	int p;
	read(pleft[0], &p, sizeof(p)); // 读第一个数，必然是素数
	if(p == -1) { // 如果是哨兵 -1，则代表所有数字处理完毕，退出程序
		exit(0);
	}
	printf("prime %d\n", p);

	int pright[2];
	pipe(pright); // 创建用于输出到下一 stage 的进程的输出管道 pright

	if(fork() == 0) {
		// 子进程 （下一个 stage）
		close(pright[1]); // 子进程只需要对输入管道 pright 进行读，而不需要写，所以关掉子进程的输入管道写文件描述符，降低进程打开的文件描述符数量
		close(pleft[0]); // 这里的 pleft 是*父进程*的输入管道，子进程用不到，关掉
		sieve(pright); // 子进程以父进程的输出管道作为输入，开始进行下一个 stage 的处理。

	} else {
		// 父进程 （当前 stage）
		close(pright[0]); // 同上，父进程只需要对子进程的输入管道进行写而不需要读，所以关掉父进程的读文件描述符
		int buf;
		while(read(pleft[0], &buf, sizeof(buf)) && buf != -1) { // 从左端的进程读入数字
			if(buf % p != 0) { // 筛掉能被该进程筛掉的数字
				write(pright[1], &buf, sizeof(buf)); // 将剩余的数字写到右端进程
			}
		}
		buf = -1;
		write(pright[1], &buf, sizeof(buf)); // 补写最后的 -1，标示输入完成。
		wait(0); // 等待该进程的子进程完成，也就是下一 stage
		exit(0);
	}
}

int main(int argc, char **argv) {
	// 主进程
	int input_pipe[2];
	pipe(input_pipe); // 准备好输入管道，输入 2 到 35 之间的所有整数。

	if(fork() == 0) {
		// 第一个 stage 的子进程
		close(input_pipe[1]); // 子进程只需要读输入管道，而不需要写，关掉子进程的管道写文件描述符
		sieve(input_pipe);
		exit(0);
	} else {
		// 主进程
		close(input_pipe[0]); // 同上
		int i;
		for(i=2;i<=35;i++){ // 生成 [2, 35]，输入管道链最左端
			write(input_pipe[1], &i, sizeof(i));
		}
		i = -1;
		write(input_pipe[1], &i, sizeof(i)); // 末尾输入 -1，用于标识输入完成
	}
	wait(0); // 等待第一个 stage 完成。注意：这里无法等待子进程的子进程，只能等待直接子进程，无法等待间接子进程。在 sieve() 中会为每个 stage 再各自执行 wait(0)，形成等待链。
	exit(0);
}
```

这一道的主要坑就是，stage 之间的管道 pleft 和 pright，要注意关闭不需要用到的文件描述符，否则跑到 n = 13 的时候就会爆掉，出现读到全是 0 的情况。

这里的理由是，fork 会将父进程的所有文件描述符都复制到子进程里，而 xv6 每个进程能打开的文件描述符总数只有 16 个 （见 `defs.h` 中的 `NOFILE` 和 `proc.h` 中的 `struct file *ofile[NOFILE]; // Open files`）。

由于一个管道会同时打开一个输入文件和一个输出文件，所以一个管道就占用了 2 个文件描述符，并且复制的子进程还会复制父进程的描述符，于是跑到第六七层后，就会由于最末端的子进程出现 16 个文件描述符都被占满的情况，导致新管道创建失败。

解决方法有两部分：

- 关闭管道的两个方向中不需要用到的方向的文件描述符（在具体进程中将管道变成只读/只写）

> 原理：每个进程从左侧的读入管道中**只需要读数据**，并且**只需要写数据**到右侧的输出管道，所以可以把左侧管道的写描述符，以及右侧管道的读描述符关闭，而不会影响程序运行 这里注意文件描述符是进程独立的，在某个进程内关闭文件描述符，不会影响到其他进程!

- 子进程创建后，关闭父进程与祖父进程之间的文件描述符（因为子进程并不需要用到之前 stage 的管道）

`具体的操作在上面代码中有体现。（fork 后、执行操作前，close 掉不需要用掉的文件描述符）`

```tex
$ primes
prime 2
prime 3
prime 5
prime 7
prime 11
prime 13
prime 17
prime 19
prime 23
prime 29
prime 31
$
```

#### 1.5find (moderate)

> Write a simple version of the UNIX find program: find all the files in a directory tree with a specific name. Your solution should be in the file user/find.c.

这里基本原理与 ls 相同，基本上可以从 ls.c 改造得到：

```c
// find.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *target) {
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;

	if((fd = open(path, 0)) < 0){
		fprintf(2, "find: cannot open %s\n", path);
		return;
	}

	if(fstat(fd, &st) < 0){
		fprintf(2, "find: cannot stat %s\n", path);
		close(fd);
		return;
	}

	switch(st.type){
	case T_FILE:
		// 如果文件名结尾匹配 `/target`，则视为匹配
		if(strcmp(path+strlen(path)-strlen(target), target) == 0) {
			printf("%s\n", path);
		}
		break;
	case T_DIR:
		if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
			printf("find: path too long\n");
			break;
		}
		strcpy(buf, path);
		p = buf+strlen(buf);
		*p++ = '/';
		while(read(fd, &de, sizeof(de)) == sizeof(de)){
			if(de.inum == 0)
				continue;
			memmove(p, de.name, DIRSIZ);
			p[DIRSIZ] = 0;
			if(stat(buf, &st) < 0){
				printf("find: cannot stat %s\n", buf);
				continue;
			}
			// 不要进入 `.` 和 `..`
			if(strcmp(buf+strlen(buf)-2, "/.") != 0 && strcmp(buf+strlen(buf)-3, "/..") != 0) {
				find(buf, target); // 递归查找
			}
		}
		break;
	}
	close(fd);
}

int main(int argc, char *argv[])
{
	if(argc < 3){
		exit(0);
	}
	char target[512];
	target[0] = '/'; // 为查找的文件名添加 / 在开头
	strcpy(target+1, argv[2]);
	find(argv[1], target);
	exit(0);
}
```

```tex
$ find . b
    ./b
    ./a/b
```

#### 1.6xargs (moderate)

> Write a simple version of the UNIX xargs program: read lines from the standard input and run a command for each line, supplying the line as arguments to the command. Your solution should be in the file user/xargs.c.

编写 xargs 工具，从标准输入读入数据，将每一行当作参数，加入到传给 xargs 的程序名和参数后面作为额外参数，然后执行。

```shell
$ echo hello too | xargs echo bye
bye hello too
```

```c
// xargs.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 带参数列表，执行某个程序
void run(char *program, char **args) {
	if(fork() == 0) { // child exec
		exec(program, args);
		exit(0);
	}
	return; // parent return
}

int main(int argc, char *argv[]){
	char buf[2048]; // 读入时使用的内存池
	char *p = buf, *last_p = buf; // 当前参数的结束、开始指针
	char *argsbuf[128]; // 全部参数列表，字符串指针数组，包含 argv 传进来的参数和 stdin 读入的参数
	char **args = argsbuf; // 指向 argsbuf 中第一个从 stdin 读入的参数
	for(int i=1;i<argc;i++) {
		// 将 argv 提供的参数加入到最终的参数列表中
		*args = argv[i];
		args++;
	}
	char **pa = args; // 开始读入参数
	while(read(0, p, 1) != 0) {
		if(*p == ' ' || *p == '\n') {
			// 读入一个参数完成（以空格分隔，如 `echo hello world`，则 hello 和 world 各为一个参数）
			*p = '\0';	// 将空格替换为 \0 分割开各个参数，这样可以直接使用内存池中的字符串作为参数字符串
						// 而不用额外开辟空间
			*(pa++) = last_p;
			last_p = p+1;

			if(*p == '\n') {
				// 读入一行完成
				*pa = 0; // 参数列表末尾用 null 标识列表结束
				run(argv[1], argsbuf); // 执行最后一行指令
				pa = args; // 重置读入参数指针，准备读入下一行
			}
		}
		p++;
	}
	if(pa != args) { // 如果最后一行不是空行
		// 收尾最后一个参数
		*p = '\0';
		*(pa++) = last_p;
		// 收尾最后一行
		*pa = 0; // 参数列表末尾用 null 标识列表结束
		// 执行最后一行指令
		run(argv[1], argsbuf);
	}
	while(wait(0) != -1) {}; // 循环等待所有子进程完成，每一次 wait(0) 等待一个
	exit(0);
}
```

### Lab 2: System calls

> In this lab you will add some new system calls to xv6, which will help you understand how they work and will expose you to some of the internals of the xv6 kernel. You will add more system calls in later labs.

对 xv6 添加一些新的系统调用，帮助加深对 xv6 内核的理解。

#### 2.1Tracing (moderate)

准备环境，编译编译器、QEMU，克隆仓库，略过。

> In this assignment you will add a system call tracing feature that may help you when debugging later labs. You’ll create a new trace system call that will control tracing. It should take one argument, an integer “mask”, whose bits specify which system calls to trace. For example, to trace the fork system call, a program calls trace(1 « SYS_fork), where SYS_fork is a syscall number from kernel/syscall.h. You have to modify the xv6 kernel to print out a line when each system call is about to return, if the system call’s number is set in the mask. The line should contain the process id, the name of the system call and the return value; you don’t need to print the system call arguments. The trace system call should enable tracing for the process that calls it and any children that it subsequently forks, but should not affect other processes.

添加一个系统调用 trace 的功能，为每个进程设定一个位 mask，用 mask 中设定的位来指定要为哪些系统调用输出调试信息。

:pager:如何创建新系统调用

1. 首先在内核中合适的位置（取决于要实现的功能属于什么模块，理论上随便放都可以，只是主要起归类作用），实现我们的内核调用（在这里是 trace 调用）：

```c
 // kernel/sysproc.c
 // 这里着重理解如何添加系统调用，对于这个调用的具体代码细节在后面的部分分析
 uint64
 sys_trace(void)
 {
 int mask;

 if(argint(0, &mask) < 0)
     return -1;
	
 myproc()->syscall_trace = mask;
 return 0;
 }
```

这里因为我们的系统调用会对进程进行操作，所以放在 sysproc.c 较为合适。

2. 在 syscall.h 中加入新 system call 的序号：

```c
 // kernel/syscall.h
 // System call numbers
 #define SYS_fork    1
 #define SYS_exit    2
 #define SYS_wait    3
 #define SYS_pipe    4
 #define SYS_read    5
 #define SYS_kill    6
 #define SYS_exec    7
 #define SYS_fstat   8
 #define SYS_chdir   9
 #define SYS_dup    10
 #define SYS_getpid 11
 #define SYS_sbrk   12
 #define SYS_sleep  13
 #define SYS_uptime 14
 #define SYS_open   15
 #define SYS_write  16
 #define SYS_mknod  17
 #define SYS_unlink 18
 #define SYS_link   19
 #define SYS_mkdir  20
 #define SYS_close  21
 #define SYS_trace  22 // here!!!!!
```

3. 用 extern 全局声明新的内核调用函数，并且在 syscalls 映射表中，加入从前面定义的编号到系统调用函数指针的映射

```c
 // kernel/syscall.c 
 extern uint64 sys_chdir(void);
 extern uint64 sys_close(void);
 extern uint64 sys_dup(void);
 extern uint64 sys_exec(void);
 extern uint64 sys_exit(void);
 extern uint64 sys_fork(void);
 extern uint64 sys_fstat(void);
 extern uint64 sys_getpid(void);
 extern uint64 sys_kill(void);
 extern uint64 sys_link(void);
 extern uint64 sys_mkdir(void);
 extern uint64 sys_mknod(void);
 extern uint64 sys_open(void);
 extern uint64 sys_pipe(void);
 extern uint64 sys_read(void);
 extern uint64 sys_sbrk(void);
 extern uint64 sys_sleep(void);
 extern uint64 sys_unlink(void);
 extern uint64 sys_wait(void);
 extern uint64 sys_write(void);
 extern uint64 sys_uptime(void);
 extern uint64 sys_trace(void);   // HERE

 static uint64 (*syscalls[])(void) = {
 [SYS_fork]    sys_fork,
 [SYS_exit]    sys_exit,
 [SYS_wait]    sys_wait,
 [SYS_pipe]    sys_pipe,
 [SYS_read]    sys_read,
 [SYS_kill]    sys_kill,
 [SYS_exec]    sys_exec,
 [SYS_fstat]   sys_fstat,
 [SYS_chdir]   sys_chdir,
 [SYS_dup]     sys_dup,
 [SYS_getpid]  sys_getpid,
 [SYS_sbrk]    sys_sbrk,
 [SYS_sleep]   sys_sleep,
 [SYS_uptime]  sys_uptime,
 [SYS_open]    sys_open,
 [SYS_write]   sys_write,
 [SYS_mknod]   sys_mknod,
 [SYS_unlink]  sys_unlink,
 [SYS_link]    sys_link,
 [SYS_mkdir]   sys_mkdir,
 [SYS_close]   sys_close,
 [SYS_trace]   sys_trace,  // AND HERE
 };
```

这里 `[SYS_trace] sys_trace` 是 C 语言数组的一个语法，表示以方括号内的值作为元素下标。比如 `int arr[] = {[3] 2333, [6] 6666}` 代表 arr 的下标 3 的元素为 2333，下标 6 的元素为 6666，其他元素填充 0 的数组。（该语法在 C++ 中已不可用）

4. 在 usys.pl 中，加入用户态到内核态的跳板函数。

```perl
 # user/usys.pl

 entry("fork");
 entry("exit");
 entry("wait");
 entry("pipe");
 entry("read");
 entry("write");
 entry("close");
 entry("kill");
 entry("exec");
 entry("open");
 entry("mknod");
 entry("unlink");
 entry("fstat");
 entry("link");
 entry("mkdir");
 entry("chdir");
 entry("dup");
 entry("getpid");
 entry("sbrk");
 entry("sleep");
 entry("uptime");
 entry("trace");  # HERE
```

这个脚本在运行后会生成 usys.S 汇编文件，里面定义了每个 system call 的用户态跳板函数：

```assembly
 trace:		# 定义用户态跳板函数
 li a7, SYS_trace	# 将系统调用 id 存入 a7 寄存器
 ecall				# ecall，调用 system call ，跳到内核态的统一系统调用处理函数 syscall()  (syscall.c)
 ret
```

5. 在用户态的头文件加入定义，使得用户态程序可以找到这个跳板入口函数。

```c
 // user/user.h
 // system calls
 int fork(void);
 int exit(int) __attribute__((noreturn));
 int wait(int*);
 int pipe(int*);
 int write(int, const void*, int);
 int read(int, void*, int);
 int close(int);
 int kill(int);
 int exec(char*, char**);
 int open(const char*, int);
 int mknod(const char*, short, short);
 int unlink(const char*);
 int fstat(int fd, struct stat*);
 int link(const char*, const char*);
 int mkdir(const char*);
 int chdir(const char*);
 int dup(int);
 int getpid(void);
 char* sbrk(int);
 int sleep(int);
 int uptime(void);
 int trace(int);		// HERE
```

##### 2.1.1系统调用全流程

```tex
user/user.h:		用户态程序调用跳板函数 trace()
user/usys.S:		跳板函数 trace() 使用 CPU 提供的 ecall 指令，调用到内核态
kernel/syscall.c	到达内核态统一系统调用处理函数 syscall()，所有系统调用都会跳到这里来处理。
kernel/syscall.c	syscall() 根据跳板传进来的系统调用编号，查询 syscalls[] 表，找到对应的内核函数并调用。
kernel/sysproc.c	到达 sys_trace() 函数，执行具体内核操作
```

`这么繁琐的调用流程的主要目的是实现用户态和内核态的良好隔离。`

==由于内核与用户进程的页表不同，寄存器也不互通，所以参数无法直接通过 C 语言参数的形式传过来，而是需要使用 argaddr、argint、argstr 等系列函数，从进程的 trapframe 中读取用户进程寄存器中的参数。同时由于页表不同，指针也不能直接互通访问（也就是内核不能直接对用户态传进来的指针进行解引用），而是需要使用 copyin、copyout 方法结合进程的页表，才能顺利找到用户态指针即逻辑地址对应的物理内存地址。==

##### 2.1.2Tracing代码

首先在 proc.h 中修改 proc 结构的定义，添加 syscall_trace field，用 mask 的方式记录要 trace 的 system call。

```c
// kernel/proc.h
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  uint64 syscall_trace;        // Mask for syscall tracing (新添加的用于标识追踪哪些 system call 的 mask)
};
```

在 proc.c 中，创建新进程的时候，为新添加的 syscall_trace 附上默认值 0（否则初始状态下可能会有垃圾数据）。

```c
// kernel/proc.c
static struct proc*
allocproc(void)
{
  ......
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  p->syscall_trace = 0; // (newly added) 为 syscall_trace 设置一个 0 的默认值

  return p;
}
```

在 sysproc.c 中，实现 system call 的具体代码，也就是设置当前进程的 syscall_trace mask：

```c
// kernel/sysproc.c
uint64
sys_trace(void)
{
  int mask;

  if(argint(0, &mask) < 0) // 通过读取进程的 trapframe，获得 mask 参数
    return -1;
  
  myproc()->syscall_trace = mask; // 设置调用进程的 syscall_trace mask
  return 0;
}
```

修改 fork 函数，使得子进程可以继承父进程的 syscall_trace mask：

```c
// kernel/proc.c
int
fork(void)
{
  ......

  safestrcpy(np->name, p->name, sizeof(p->name));

  np->syscall_trace = p->syscall_trace; // HERE!!! 子进程继承父进程的 syscall_trace

  pid = np->pid;
  np->state = RUNNABLE;
  release(&np->lock);
  return pid;
}
```

根据上方提到的系统调用的全流程，可以知道，所有的系统调用到达内核态后，都会进入到 syscall() 这个函数进行处理，所以要跟踪所有的内核函数，只需要在 syscall() 函数里埋点就行了。

```c
// kernel/syscall.c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) { // 如果系统调用编号有效
    p->trapframe->a0 = syscalls[num](); // 通过系统调用编号，获取系统调用处理函数的指针，调用并将返回值存到用户进程的 a0 寄存器中
	// 如果当前进程设置了对该编号系统调用的 trace，则打出 pid、系统调用名称和返回值。
    if((p->syscall_trace >> num) & 1) {
      printf("%d: syscall %s -> %d\n",p->pid, syscall_names[num], p->trapframe->a0); // syscall_names[num]: 从 syscall 编号到 syscall 名的映射表
    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

上面打出日志的过程还需要知道系统调用的名称字符串，在这里定义一个字符串数组进行映射：

```c
// kernel/syscall.c
const char *syscall_names[] = {
[SYS_fork]    "fork",
[SYS_exit]    "exit",
[SYS_wait]    "wait",
[SYS_pipe]    "pipe",
[SYS_read]    "read",
[SYS_kill]    "kill",
[SYS_exec]    "exec",
[SYS_fstat]   "fstat",
[SYS_chdir]   "chdir",
[SYS_dup]     "dup",
[SYS_getpid]  "getpid",
[SYS_sbrk]    "sbrk",
[SYS_sleep]   "sleep",
[SYS_uptime]  "uptime",
[SYS_open]    "open",
[SYS_write]   "write",
[SYS_mknod]   "mknod",
[SYS_unlink]  "unlink",
[SYS_link]    "link",
[SYS_mkdir]   "mkdir",
[SYS_close]   "close",
[SYS_trace]   "trace",
};
```

编译执行：

```shell
$ trace 32 grep hello README
3: syscall read -> 1023
3: syscall read -> 966
3: syscall read -> 70
3: syscall read -> 0
$
$ trace 2147483647 grep hello README
4: syscall trace -> 0
4: syscall exec -> 3
4: syscall open -> 3
4: syscall read -> 1023
4: syscall read -> 966
4: syscall read -> 70
4: syscall read -> 0
4: syscall close -> 0
$
```

`成功追踪并打印出相应的系统调用。`

#### 2.2Sysinfo (moderate)

> In this assignment you will add a system call, sysinfo, that collects information about the running system. The system call takes one argument: a pointer to a struct sysinfo (see kernel/sysinfo.h). The kernel should fill out the fields of this struct: the freemem field should be set to the number of bytes of free memory, and the nproc field should be set to the number of processes whose state is not UNUSED. We provide a test program sysinfotest; you pass this assignment if it prints “sysinfotest: OK”.

添加一个系统调用，返回空闲的内存、以及已创建的进程数量。大多数步骤和上个实验是一样的，所以不再描述。唯一不同就是需要把结构体从内核内存拷贝到用户进程内存中。其他的难点可能就是在如何获取空闲内存和如何获取已创建进程上面了，因为涉及到了一些后面的知识。

##### 2.2.1获取空闲内存

在内核的头文件中声明计算空闲内存的函数，因为是内存相关的，所以放在 kalloc、kfree 等函数的的声明之后。

```c
// kernel/defs.h
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);
uint64 			count_free_mem(void); // here
```

在 kalloc.c 中添加计算空闲内存的函数：

```c
// kernel/kalloc.c
uint64
count_free_mem(void) // added for counting free memory in bytes (lab2)
{
  acquire(&kmem.lock); // 必须先锁内存管理结构，防止竞态条件出现
  
  // 统计空闲页数，乘上页大小 PGSIZE 就是空闲的内存字节数
  uint64 mem_bytes = 0;
  struct run *r = kmem.freelist;
  while(r){
    mem_bytes += PGSIZE;
    r = r->next;
  }

  release(&kmem.lock);

  return mem_bytes;
} 
```

xv6 中，空闲内存页的记录方式是，将空闲内存页本身直接用作链表节点，形成一个空闲页链表，每次需要分配，就把链表根部对应的页分配出去。每次需要回收，就把这个页作为新的根节点，把原来的 freelist 链表接到后面。注意这里是直接使用空闲页本身作为链表节点，所以不需要使用额外空间来存储空闲页链表，在 kalloc() 里也可以看到，分配内存的最后一个阶段，是直接将 freelist 的根节点的址（物理地址）返回出去了：

```c
// kernel/kalloc.c
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist; // 获得空闲页链表的根节点
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r; // 把空闲页链表的根节点返回出去，作为内存页使用（长度是 4096）
}
```

==常见的记录空闲页的方法有：空闲表法、空闲链表法、位示图法（位图法）、成组链接法。==这里 xv6 采用的是`空闲链表法。`

##### 2.2.2获取运行的进程数

同样在内核的头文件中添加函数声明：

```c
// kernel/defs.h
......
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(uint64);
void            wakeup(void*);
void            yield(void);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);
uint64			count_process(void); // here
```

在 proc.c 中实现该函数：

```c
uint64
count_process(void) { // added function for counting used process slots (lab2)
  uint64 cnt = 0;
  for(struct proc *p = proc; p < &proc[NPROC]; p++) {
    // acquire(&p->lock);
    // 不需要锁进程 proc 结构，因为我们只需要读取进程列表，不需要写
    if(p->state != UNUSED) { // 不是 UNUSED 的进程位，就是已经分配的
        cnt++;
    }
  }
  return cnt;
}
```

##### 2.2.3实现 sysinfo 系统调用

添加系统调用的流程与实验 1 类似，不再赘述。

这是具体系统信息函数的实现，其中调用了前面实现的 count_free_mem() 和 count_process()：

```c
uint64
sys_sysinfo(void)
{
  // 从用户态读入一个指针，作为存放 sysinfo 结构的缓冲区
  uint64 addr;
  if(argaddr(0, &addr) < 0)
    return -1;
  
  struct sysinfo sinfo;
  sinfo.freemem = count_free_mem(); // kalloc.c
  sinfo.nproc = count_process(); // proc.c
  
  // 使用 copyout，结合当前进程的页表，获得进程传进来的指针（逻辑地址）对应的物理地址
  // 然后将 &sinfo 中的数据复制到该指针所指位置，供用户进程使用。
  if(copyout(myproc()->pagetable, addr, (char *)&sinfo, sizeof(sinfo)) < 0)
    return -1;
  return 0;
}
```

在 user.h 提供用户态入口：

```c
// user.h
char* sbrk(int);
int sleep(int);
int uptime(void);
int trace(int);
struct sysinfo; // 这里要声明一下 sysinfo 结构，供用户态使用。
int sysinfo(struct sysinfo *);
```

编译运行：

```shell
$ sysinfotest
sysinfotest: start
sysinfotest: OK
```

### Lab 3: Page tables

> In this lab you will explore page tables and modify them to simplify the functions that copy data from user space to kernel space.

探索页表，修改页表以简化从用户态拷贝数据到内核态的方法。

#### 3.1Print a page table (easy)

> Define a function called vmprint(). It should take a pagetable_t argument, and print that pagetable in the format described below. Insert if(p->pid==1) vmprint(p->pagetable) in exec.c just before the return argc, to print the first process’s page table. You receive full credit for this assignment if you pass the pte printout test of make grade.

添加一个打印页表的内核函数，以如下格式打印出传进的页表，用于后面两个实验调试用：

```tex
page table 0x0000000087f6e000
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
.. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
.. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
.. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
.. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
.. .. ..510: pte 0x0000000021fdd807 pa 0x0000000087f76000
.. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
```

RISC-V 的逻辑地址寻址是采用`三级页表`的形式，9 bit 一级索引找到二级页表，9 bit 二级索引找到三级页表，9 bit 三级索引找到内存页，最低 12 bit 为页内偏移（即一个页 4096 bytes）。

本函数需要模拟如上的 CPU 查询页表的过程，对三级页表进行遍历，然后按照一定格式输出

```c
// kernel/defs.h
......
int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(pagetable_t, char *, uint64, uint64);
int             copyinstr(pagetable_t, char *, uint64, uint64);
int             vmprint(pagetable_t pagetable); // 添加函数声明
```

因为需要递归打印页表，而 xv6 已经有一个递归释放页表的函数 freewalk()，将其复制一份，并将释放部分代码改为打印即可：

```c
// kernel/vm.c
int pgtblprint(pagetable_t pagetable, int depth) {
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V) { // 如果页表项有效
      // 按格式打印页表项
      printf("..");
      for(int j=0;j<depth;j++) {
        printf(" ..");
      }
      printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));

      // 如果该节点不是叶节点，递归打印其子节点。
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // this PTE points to a lower-level page table.
        uint64 child = PTE2PA(pte);
        pgtblprint((pagetable_t)child,depth+1);
      }
    }
  }
  return 0;
}

int vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  return pgtblprint(pagetable, 0);
}
```

```c
// exec.c

int
exec(char *path, char **argv)
{
  // ......

  vmprint(p->pagetable); // 按照实验要求，在 exec 返回之前打印一下页表。
  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;

}
```

#### 3.2A kernel page table per process (hard)

> Your first job is to modify the kernel so that every process uses its own copy of the kernel page table when executing in the kernel. Modify struct proc to maintain a kernel page table for each process, and modify the scheduler to switch kernel page tables when switching processes. For this step, each per-process kernel page table should be identical to the existing global kernel page table. You pass this part of the lab if usertests runs correctly.

xv6 原本的设计是，用户进程在用户态使用各自的用户态页表，但是一旦进入内核态（例如使用了系统调用），则切换到内核页表（通过修改 satp 寄存器，trampoline.S）。然而这个内核页表是全局共享的，也就是全部进程进入内核态都共用同一个内核态页表：

```c
// vm.c
pagetable_t kernel_pagetable; // 全局变量，共享的内核页表
```

本 Lab 目标是让每一个进程进入内核态后，都能有自己的独立内核页表，为第三个实验做准备。

##### 3.2.1创建进程内核页表与内核栈

首先在进程的结构体 proc 中，添加一个 kernelpgtbl，用于存储进程专享的内核态页表。

```c
// kernel/proc.h
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  pagetable_t kernelpgtbl;     // Kernel page table （在 proc 中添加该 field）
};
```

接下来暴改 kvminit。内核需要依赖内核页表内一些固定的映射的存在才能正常工作，例如 UART 控制、硬盘界面、中断控制等。而 kvminit 原本只为全局内核页表 kernel_pagetable 添加这些映射。我们抽象出来一个可以为任何我们自己创建的内核页表添加这些映射的函数 kvm_map_pagetable()。

```c
void kvm_map_pagetable(pagetable_t pgtbl) {
  // 将各种内核需要的 direct mapping 添加到页表 pgtbl 中。
  // uart registers
  kvmmap(pgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  // virtio mmio disk interface
  kvmmap(pgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  // CLINT
  kvmmap(pgtbl, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  // PLIC
  kvmmap(pgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  // map kernel text executable and read-only.
  kvmmap(pgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  // map kernel data and the physical RAM we'll make use of.
  kvmmap(pgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

pagetable_t
kvminit_newpgtbl()
{
  pagetable_t pgtbl = (pagetable_t) kalloc();
  memset(pgtbl, 0, PGSIZE);

  kvm_map_pagetable(pgtbl);

  return pgtbl;
}

/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = kvminit_newpgtbl(); // 仍然需要有全局的内核页表，用于内核 boot 过程，以及无进程在运行时使用。
}

// ......

// 将某个逻辑地址映射到某个物理地址（添加第一个参数 pgtbl）
void
kvmmap(pagetable_t pgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(pgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// kvmpa 将内核逻辑地址转换为物理地址（添加第一个参数 kernelpgtbl）
uint64
kvmpa(pagetable_t pgtbl, uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;

  pte = walk(pgtbl, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}
 
```

现在可以创建进程间相互独立的内核页表了，但是还有一个东西需要处理：内核栈。 原本的 xv6 设计中，所有处于内核态的进程都共享同一个页表，即意味着共享同一个地址空间。由于 xv6 支持多核/多进程调度，同一时间可能会有多个进程处于内核态，所以需要对所有处于内核态的进程创建其独立的内核态内的栈，也就是内核栈，供给其内核态代码执行过程。

`xv6 在启动过程中，会在 procinit() 中为所有可能的 64 个进程位都预分配好内核栈 kstack，具体为在高地址空间里，每个进程使用一个页作为 kstack，并且两个不同 kstack 中间隔着一个无映射的 guard page 用于检测栈溢出错误。`

在 xv6 原来的设计中，内核页表本来是只有一个的，所有进程共用，所以需要为不同进程创建多个内核栈，并 map 到不同位置（见 procinit() 和 KSTACK 宏）。而我们的新设计中，每一个进程都会有自己独立的内核页表，并且每个进程也只需要访问自己的内核栈，而不需要能够访问所有 64 个进程的内核栈。所以可以将所有进程的内核栈 map 到其`各自内核页表内的固定位置`（不同页表内的同一逻辑地址，指向不同物理内存）。

```c
// initialize the proc table at boot time.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
// 这里删除了为所有进程预分配内核栈的代码，变为创建进程的时候再创建内核栈，见 allocproc()
  }

  kvminithart();
}

```

然后，在创建进程的时候，为进程分配独立的内核页表，以及内核栈：

```c
// kernel/proc.c

static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

////// 新加部分 start //////

  // 为新进程创建独立的内核页表，并将内核所需要的各种映射添加到新页表上
  p->kernelpgtbl = kvminit_newpgtbl();
  // printf("kernel_pagetable: %p\n", p->kernelpgtbl);

  // 分配一个物理页，作为新进程的内核栈使用
  char *pa = kalloc();
  if(pa == 0)
    panic("kalloc");
  uint64 va = KSTACK((int)0); // 将内核栈映射到固定的逻辑地址上
  // printf("map krnlstack va: %p to pa: %p\n", va, pa);
  kvmmap(p->kernelpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  p->kstack = va; // 记录内核栈的逻辑地址，其实已经是固定的了，依然这样记录是为了避免需要修改其他部分 xv6 代码

////// 新加部分 end //////

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}
```

到这里进程独立的内核页表就创建完成了，但是目前只是创建而已，用户进程进入内核态后依然会使用全局共享的内核页表，因此还需要在 scheduler() 中进行相关修改。

##### 3.2.2切换到进程内核页表

在调度器将 CPU 交给进程执行之前，切换到该进程对应的内核页表：

```c
// kernel/proc.c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    
    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;

        // 切换到进程独立的内核页表
        w_satp(MAKE_SATP(p->kernelpgtbl));
        sfence_vma(); // 清除快表缓存
        
        // 调度，执行进程
        swtch(&c->context, &p->context);

        // 切换回全局内核页表
        kvminithart();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;

        found = 1;
      }
      release(&p->lock);
    }
#if !defined (LAB_FS)
    if(found == 0) {
      intr_on();
      asm volatile("wfi");
    }
#else
    ;
#endif
  }
}

```

到这里，每个进程执行的时候，就都会在内核态采用自己独立的内核页表了。

##### 3.2.3释放进程内核页表

最后需要做的事情就是在进程结束后，应该释放进程独享的页表以及内核栈，回收资源，否则会导致内存泄漏。

如果 usertests 在 reparent2 的时候出现了 `panic: kvmmap`，大概率是因为大量内存泄漏消耗完了内存，导致 kvmmap 分配页表项所需内存失败，这时候应该检查是否正确释放了每一处分配的内存，尤其是页表是否每个页表项都释放干净了。 

```c
 // kernel/proc.c
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  
  // 释放进程的内核栈
  void *kstack_pa = (void *)kvmpa(p->kernelpgtbl, p->kstack);
  // printf("trace: free kstack %p\n", kstack_pa);
  kfree(kstack_pa);
  p->kstack = 0;
  
  // 注意：此处不能使用 proc_freepagetable，因为其不仅会释放页表本身，还会把页表内所有的叶节点对应的物理页也释放掉。
  // 这会导致内核运行所需要的关键物理页被释放，从而导致内核崩溃。
  // 这里使用 kfree(p->kernelpgtbl) 也是不足够的，因为这只释放了一级页表本身，而不释放二级以及三级页表所占用的空间。
  
  // 递归释放进程独享的页表，释放页表本身所占用的空间，但**不释放页表指向的物理页**
  kvm_free_kernelpgtbl(p->kernelpgtbl);
  p->kernelpgtbl = 0;
  p->state = UNUSED;
}
```

kvm_free_kernelpgtbl() 用于递归释放整个多级页表树，也是从 freewalk() 修改而来。

```c
// kernel/vm.c

// 递归释放一个内核页表中的所有 mapping，但是不释放其指向的物理页
void
kvm_free_kernelpgtbl(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    uint64 child = PTE2PA(pte);
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){ // 如果该页表项指向更低一级的页表
      // 递归释放低一级页表及其页表项
      kvm_free_kernelpgtbl((pagetable_t)child);
      pagetable[i] = 0;
    }
  }
  kfree((void*)pagetable); // 释放当前级别页表所占用空间
}
```

这里释放部分就实现完成了。

注意到我们的修改影响了其他代码： virtio 磁盘驱动 virtio_disk.c 中调用了 kvmpa() 用于将虚拟地址转换为物理地址，这一操作在我们修改后的版本中，需要传入进程的内核页表。对应修改即可。

```c
// virtio_disk.c
#include "proc.h" // 添加头文件引入

// ......

void
virtio_disk_rw(struct buf *b, int write)
{
// ......
disk.desc[idx[0]].addr = (uint64) kvmpa(myproc()->kernelpgtbl, (uint64) &buf0); // 调用 myproc()，获取进程内核页表
// ......
}
```

#### 3.3Simplify copyin/copyinstr (hard)

> Replace the body of copyin in kernel/vm.c with a call to copyin_new (defined in kernel/vmcopyin.c); do the same for copyinstr and copyinstr_new. Add mappings for user addresses to each process’s kernel page table so that copyin_new and copyinstr_new work. You pass this assignment if usertests runs correctly and all the make grade tests pass.

在上一个实验中，已经使得每一个进程都拥有独立的内核态页表了，这个实验的目标是，`在进程的内核态页表中维护一个用户态页表映射的副本，这样使得内核态也可以对用户态传进来的指针（逻辑地址）进行解引用。`这样做相比原来 copyin 的实现的优势是，==原来的 copyin 是通过软件模拟访问页表的过程获取物理地址的，而在内核页表内维护映射副本的话，可以利用 CPU 的硬件寻址功能进行寻址，效率更高并且可以受快表加速。==

> 要实现这样的效果，我们需要在每一处内核对用户页表进行修改的时候，将同样的修改也同步应用在进程的内核页表上，使得两个页表的程序段（0 到 PLIC 段）地址空间的映射同步。

##### 3.3.1准备页表映射转换的工具方法

首先实现一些工具方法，多数是参考现有方法改造得来：

```c
// kernel/vm.c

// 注：需要在 defs.h 中添加相应的函数声明，这里省略。

// 将 src 页表的一部分页映射关系拷贝到 dst 页表中。
// 只拷贝页表项，不拷贝实际的物理页内存。
// 成功返回0，失败返回 -1
int
kvmcopymappings(pagetable_t src, pagetable_t dst, uint64 start, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  // PGROUNDUP: prevent re-mapping already mapped pages (eg. when doing growproc)
  for(i = PGROUNDUP(start); i < start + sz; i += PGSIZE){
    if((pte = walk(src, i, 0)) == 0)
      panic("kvmcopymappings: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("kvmcopymappings: page not present");
    pa = PTE2PA(*pte);
    // `& ~PTE_U` 表示将该页的权限设置为非用户页
    // 必须设置该权限，RISC-V 中内核是无法直接访问用户页的。
    flags = PTE_FLAGS(*pte) & ~PTE_U;
    if(mappages(dst, i, PGSIZE, pa, flags) != 0){
      goto err;
    }
  }

  return 0;

 err:
  // thanks @hdrkna for pointing out a mistake here.
  // original code incorrectly starts unmapping from 0 instead of PGROUNDUP(start)
  uvmunmap(dst, PGROUNDUP(start), (i - PGROUNDUP(start)) / PGSIZE, 0);
  return -1;
}

// 与 uvmdealloc 功能类似，将程序内存从 oldsz 缩减到 newsz。但区别在于不释放实际内存
// 用于内核页表内程序内存映射与用户页表程序内存映射之间的同步
uint64
kvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 0);
  }

  return newsz;
}

```

接下来，为映射程序内存做准备。实验中提示内核启动后，能够用于映射程序内存的地址范围是 [0,PLIC)，我们将把进程程序内存映射到其内核页表的这个范围内，`首先要确保这个范围没有和其他映射冲突。`

查阅 xv6 book 可以看到，在 PLIC 之前还有一个 CLINT（核心本地中断器）的映射，该映射会与我们要 map 的程序内存冲突。`查阅 xv6 book 的 Chapter 5 以及 start.c 可以知道 CLINT 仅在内核启动的时候需要使用到，而用户进程在内核态中的操作并不需要使用到该映射。`

![image-20221119142053545](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221119142053545.png)

`所以修改 kvm_map_pagetable()，去除 CLINT 的映射，这样进程内核页表就不会有 CLINT 与程序内存映射冲突的问题。但是由于全局内核页表也使用了 kvm_map_pagetable() 进行初始化，并且内核启动的时候需要 CLINT 映射存在，故在 kvminit() 中，另外单独给全局内核页表映射 CLINT。`

```c
// kernel/vm.c


void kvm_map_pagetable(pagetable_t pgtbl) {
  
  // uart registers
  kvmmap(pgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(pgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  // kvmmap(pgtbl, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(pgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // ......
}

// ......

void
kvminit()
{
  kernel_pagetable = kvminit_newpgtbl();
  // CLINT *is* however required during kernel boot up and
  // we should map it for the global kernel pagetable
  kvmmap(kernel_pagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
}
```

同时在 exec 中加入检查，防止程序内存超过 PLIC：

```c
 int
exec(char *path, char **argv)
{
  // ......

  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(sz1 >= PLIC) { // 添加检测，防止程序大小超过 PLIC
      goto bad;
    }
    sz = sz1;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;
  // .......
```

##### 3.3.2同步映射用户页表和用户内核页表

后面的步骤就是在每个修改到进程用户页表的位置，都将相应的修改同步到进程内核页表中。一共要修改：fork()、exec()、growproc()、userinit()。

fork()

```c
 // kernel/proc.c
int
fork(void)
{
  // ......

  // Copy user memory from parent to child. （调用 kvmcopymappings，将新进程用户页表映射拷贝一份到新进程内核页表中）
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0 ||
     kvmcopymappings(np->pagetable, np->kernelpgtbl, 0, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // ......
}

```

exec()

```c
 // kernel/exec.c
int
exec(char *path, char **argv)
{
  // ......

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));

  // 清除内核页表中对程序内存的旧映射，然后重新建立映射。
  uvmunmap(p->kernelpgtbl, 0, PGROUNDUP(oldsz)/PGSIZE, 0);
  kvmcopymappings(pagetable, p->kernelpgtbl, 0, sz);
  
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);
  // ......
}
```

growproc()

```c
// kernel/proc.c
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    uint64 newsz;
    if((newsz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
    // 内核页表中的映射同步扩大
    if(kvmcopymappings(p->pagetable, p->kernelpgtbl, sz, n) != 0) {
      uvmdealloc(p->pagetable, newsz, sz);
      return -1;
    }
    sz = newsz;
  } else if(n < 0){
    uvmdealloc(p->pagetable, sz, sz + n);
    // 内核页表中的映射同步缩小
    sz = kvmdealloc(p->kernelpgtbl, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}
```

userinit()

对于 init 进程，由于不像其他进程，init 不是 fork 得来的，所以需要在 userinit 中也添加同步映射的代码。

```c
// kernel/proc.c
void
userinit(void)
{
  // ......

  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;
  kvmcopymappings(p->pagetable, p->kernelpgtbl, 0, p->sz); // 同步程序内存映射到进程内核页表中
  // ......
}
```

到这里，两个页表的同步操作就都完成了。

##### 3.3.3替换 copyin、copyinstr 实现

```c
// kernel/vm.c

// 声明新函数原型
int copyin_new(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
int copyinstr_new(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);

// 将 copyin、copyinstr 改为转发到新函数
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  return copyin_new(pagetable, dst, srcva, len);
}

int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  return copyinstr_new(pagetable, dst, srcva, max);
}
```

### Lab 4: Traps

> This lab explores how system calls are implemented using traps. You will first do a warm-up exercises with stacks and then you will implement an example of user-level trap handling.

探索 trap 实现系统调用的方式。

注意本部分主要内容其实都在lecture里（lecture 5、lecture 6），实验不是非常复杂但是以理解概念为重，trap机制、trampoline作用、函数calling convention、调用栈、特权模式、riscv汇编，这些即使都不知道可能依然能完成 lab。但是不代表这些不重要，相反这些才是主要内容，否则 lab 就算跑起来也只是盲狙，没有真正达到学习效果。

#### 4.1RISC-V assembly (easy)

It will be important to understand a bit of RISC-V assembly, which you were exposed to in 6.004. There is a file user/call.c in your xv6 repo. make fs.img compiles it and also produces a readable assembly version of the program in user/call.asm.

> Read the code in call.asm for the functions g, f, and main. The instruction manual for RISC-V is on the reference page. Here are some questions that you should answer (store the answers in a file answers-traps.txt)

阅读 call.asm，以及 RISC-V 指令集教程，回答问题。（学习 RISC-V 汇编）

```tex
Q: Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?
A: a0-a7; a2;

Q: Where is the call to function f in the assembly code for main? Where is the call to g? (Hint: the compiler may inline functions.)
A: There is none. g(x) is inlined within f(x) and f(x) is further inlined into main()

Q: At what address is the function printf located?
A: 0x0000000000000628, main calls it with pc-relative addressing.

Q: What value is in the register ra just after the jalr to printf in main?
A: 0x0000000000000038, next line of assembly right after the jalr

Q: Run the following code.

	unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, &i);      

What is the output?
If the RISC-V were instead big-endian what would you set i to in order to yield the same output?
Would you need to change 57616 to a different value?
A: "He110 World"; 0x726c6400; no, 57616 is 110 in hex regardless of endianness.

Q: In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?

	printf("x=%d y=%d", 3);

A: A random value depending on what codes there are right before the call.Because printf tried to read more arguments than supplied.
The second argument `3` is passed in a1, and the register for the third argument, a2, is not set to any specific value before the
call, and contains whatever there is before the call.
```

简单翻译：

```tex
Q: 哪些寄存器存储了函数调用的参数？举个例子，main 调用 printf 的时候，13 被存在了哪个寄存器中？
A: a0-a7; a2;

Q: main 中调用函数 f 对应的汇编代码在哪？对 g 的调用呢？ (提示：编译器有可能会内链(inline)一些函数)
A: 没有这样的代码。 g(x) 被内链到 f(x) 中，然后 f(x) 又被进一步内链到 main() 中

Q: printf 函数所在的地址是？
A: 0x0000000000000628, main 中使用 pc 相对寻址来计算得到这个地址。

Q: 在 main 中 jalr 跳转到 printf 之后，ra 的值是什么？
A: 0x0000000000000038, jalr 指令的下一条汇编指令的地址。

Q: 运行下面的代码

	unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, &i);      

输出是什么？
如果 RISC-V 是大端序的，要实现同样的效果，需要将 i 设置为什么？需要将 57616 修改为别的值吗？
A: "He110 World"; 0x726c6400; 不需要，57616 的十六进制是 110，无论端序（十六进制和内存中的表示不是同个概念）

Q: 在下面的代码中，'y=' 之后会答应什么？ (note: 答案不是一个具体的值) 为什么?

	printf("x=%d y=%d", 3);

A: 输出的是一个受调用前的代码影响的“随机”的值。因为 printf 尝试读的参数数量比提供的参数数量多。
第二个参数 `3` 通过 a1 传递，而第三个参数对应的寄存器 a2 在调用前不会被设置为任何具体的值，而是会
包含调用发生前的任何已经在里面的值。
```

#### 4.2Backtrace (moderate)

For debugging it is often useful to have a backtrace: a list of the function calls on the stack above the point at which the error occurred.

> Implement a backtrace() function in kernel/printf.c. Insert a call to this function in sys_sleep, and then run bttest, which calls sys_sleep. Your output should be as follows:
>
> ```
> backtrace: 0x0000000080002cda 0x0000000080002bb6 0x0000000080002898 
> ```
>
> After bttest exit qemu. In your terminal: the addresses may be slightly different but if you run addr2line -e kernel/kernel (or riscv64-unknown-elf-addr2line -e kernel/kernel) and cut-and-paste the above addresses as follows:
>
> ```
> $ addr2line -e kernel/kernel 0x0000000080002de2 0x0000000080002f4a 0x0000000080002bfc Ctrl-D 
> ```
>
> You should see something like this:
>
> ```tex
> kernel/sysproc.c:74 kernel/syscall.c:224 kernel/trap.c:85
> ```

添加 backtrace 功能，打印出调用栈，用于调试。

在 defs.h 中添加声明

```c
// defs.h
void            printf(char*, ...);
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);
void            backtrace(void); // new
```

在 riscv.h 中添加获取当前 fp（frame pointer）寄存器的方法：

```c
// riscv.h
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x));
  return x;
}
```

fp 指向当前栈帧的开始地址，sp 指向当前栈帧的结束地址。 （栈从高地址往低地址生长，所以 fp 虽然是帧开始地址，但是地址比 sp 高）
栈帧中从高到低第一个 8 字节 `fp-8` 是 return address，也就是当前调用层应该返回到的地址。
栈帧中从高到低第二个 8 字节 `fp-16` 是 previous address，指向上一层栈帧的 fp 开始地址。
剩下的为保存的寄存器、局部变量等。一个栈帧的大小不固定，但是至少 16 字节。
在 xv6 中，使用一个页来存储栈，如果 fp 已经到达栈页的上界，则说明已经到达栈底。

查看 call.asm，可以看到，一个函数的函数体最开始首先会扩充一个栈帧给该层调用使用，在函数执行完毕后再回收，例子：

```c
int g(int x) {
   0:	1141                  addi  sp,sp,-16  // 扩张调用栈，得到一个 16 字节的栈帧
   2:	e422                  sd    s0,8(sp)   // 将返回地址存到栈帧的第一个 8 字节中
   4:	0800                  addi  s0,sp,16
  return x+3;
}
   6:	250d                  addiw a0,a0,3
   8:	6422                  ld    s0,8(sp)   // 从栈帧读出返回地址
   a:	0141                  addi  sp,sp,16   // 回收栈帧
   c:	8082                  ret              // 返回
```

注意栈的生长方向是从高地址到低地址，所以扩张是 -16，而回收是 +16。

实现 backtrace 函数：

```c
// printf.c

void backtrace() {
  uint64 fp = r_fp();
  while(fp != PGROUNDUP(fp)) { // 如果已经到达栈底
    uint64 ra = *(uint64*)(fp - 8); // return address
    printf("%p\n", ra);
    fp = *(uint64*)(fp - 16); // previous fp
  }
}
```

在 sys_sleep 的开头调用一次 backtrace()

```c
// sysproc.c
uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  backtrace(); // print stack backtrace.

  if(argint(0, &n) < 0)
    return -1;
  
  // ......

  return 0;
}
```

编译运行：

```shell
$ bttest
0x0000000080002dea
0x0000000080002cc4
0x00000000800028d0
```

#### 4.3Alarm (hard)

> In this exercise you’ll add a feature to xv6 that periodically alerts a process as it uses CPU time. This might be useful for compute-bound processes that want to limit how much CPU time they chew up, or for processes that want to compute but also want to take some periodic action. More generally, you’ll be implementing a primitive form of user-level interrupt/fault handlers; you could use something similar to handle page faults in the application, for example. Your solution is correct if it passes alarmtest and usertests.

按照如下原型添加系统调用 `sigalarm` 和 `sigreturn`（具体步骤不再赘述）：

```c
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```

首先，在 proc 结构体的定义中，增加 alarm 相关字段：

- alarm_interval：时钟周期，0 为禁用
- alarm_handler：时钟回调处理函数
- alarm_ticks：下一次时钟响起前还剩下的 ticks 数
- alarm_trapframe：时钟中断时刻的 trapframe，用于中断处理完成后恢复原程序的正常执行
- alarm_goingoff：是否已经有一个时钟回调正在执行且还未返回（用于防止在 alarm_handler 中途闹钟到期再次调用 alarm_handler，导致 alarm_trapframe 被覆盖）

```c
struct proc {
  // ......
  int alarm_interval;          // Alarm interval (0 for disabled)
  void(*alarm_handler)();      // Alarm handler
  int alarm_ticks;             // How many ticks left before next alarm goes off
  struct trapframe *alarm_trapframe;  // A copy of trapframe right before running alarm_handler
  int alarm_goingoff;          // Is an alarm currently going off and hasn't not yet returned? (prevent re-entrance of alarm_handler)
};
```

sigalarm 与 sigreturn 具体实现：

```c
// sysproc.c
uint64 sys_sigalarm(void) {
  int n;
  uint64 fn;
  if(argint(0, &n) < 0)
    return -1;
  if(argaddr(1, &fn) < 0)
    return -1;
  
  return sigalarm(n, (void(*)())(fn));
}

uint64 sys_sigreturn(void) {
	return sigreturn();
}
```

```c
// trap.c
int sigalarm(int ticks, void(*handler)()) {
  // 设置 myproc 中的相关属性
  struct proc *p = myproc();
  p->alarm_interval = ticks;
  p->alarm_handler = handler;
  p->alarm_ticks = ticks;
  return 0;
}

int sigreturn() {
  // 将 trapframe 恢复到时钟中断之前的状态，恢复原本正在执行的程序流
  struct proc *p = myproc();
  *p->trapframe = *p->alarm_trapframe;
  p->alarm_goingoff = 0;
  return 0;
}
```

在 proc.c 中添加初始化与释放代码：

```c
// proc.c
static struct proc*
allocproc(void)
{
  // ......

found:
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  // Allocate a trapframe page for alarm_trapframe.
  if((p->alarm_trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  p->alarm_interval = 0;
  p->alarm_handler = 0;
  p->alarm_ticks = 0;
  p->alarm_goingoff = 0;

  // ......

  return p;
}

static void
freeproc(struct proc *p)
{
  // ......

  if(p->alarm_trapframe)
    kfree((void*)p->alarm_trapframe);
  p->alarm_trapframe = 0;
  
  // ......
  
  p->alarm_interval = 0;
  p->alarm_handler = 0;
  p->alarm_ticks = 0;
  p->alarm_goingoff = 0;
  p->state = UNUSED;
}
```

在 usertrap() 函数中，实现时钟机制具体代码：

```c
void
usertrap(void)
{
  int which_dev = 0;

  // ......

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  // if(which_dev == 2) {
  //   yield();
  // }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2) {
    if(p->alarm_interval != 0) { // 如果设定了时钟事件
      if(--p->alarm_ticks <= 0) { // 时钟倒计时 -1 tick，如果已经到达或超过设定的 tick 数
        if(!p->alarm_goingoff) { // 确保没有时钟正在运行
          p->alarm_ticks = p->alarm_interval;
          // jump to execute alarm_handler
          *p->alarm_trapframe = *p->trapframe; // backup trapframe
          p->trapframe->epc = (uint64)p->alarm_handler;
          p->alarm_goingoff = 1;
        }
        // 如果一个时钟到期的时候已经有一个时钟处理函数正在运行，则会推迟到原处理函数运行完成后的下一个 tick 才触发这次时钟
      }
    }
    yield();
  }

  usertrapret();
}
```

这样，在每次时钟中断的时候，如果进程有已经设置的时钟（`alarm_interval != 0`），则进行 alarm_ticks 倒数。当 alarm_ticks 倒数到小于等于 0 的时候，如果没有正在处理的时钟，则尝试触发时钟，将原本的程序流保存起来（`*alarm_trapframe = *trapframe`），然后通过修改 pc 寄存器的值，将程序流转跳到 alarm_handler 中，alarm_handler 执行完毕后再恢复原本的执行流（`*trapframe = *alarm_trapframe`）。这样从原本程序执行流的视角，就是不可感知的中断了。

编译运行：

```shell
$ alarmtest
test0 start
.............alarm!
test0 passed
test1 start
..alarm!
..alarm!
..alarm!
..alarm!
..alarm!
..alarm!
..alarm!
.alarm!
...alarm!
..alarm!
test1 passed
test2 start
..............alarm!
test2 passed
```

### Lab 5: Lazy Page Allocation

> One of the many neat tricks an O/S can play with page table hardware is lazy allocation of user-space heap memory. Xv6 applications ask the kernel for heap memory using the sbrk() system call. In the kernel we’ve given you, sbrk() allocates physical memory and maps it into the process’s virtual address space. It can take a long time for a kernel to allocate and map memory for a large request. Consider, for example, that a gigabyte consists of 262,144 4096-byte pages; that’s a huge number of allocations even if each is individually cheap. In addition, some programs allocate more memory than they actually use (e.g., to implement sparse arrays), or allocate memory well in advance of use. To allow sbrk() to complete more quickly in these cases, sophisticated kernels allocate user memory lazily. That is, sbrk() doesn’t allocate physical memory, but just remembers which user addresses are allocated and marks those addresses as invalid in the user page table. When the process first tries to use any given page of lazily-allocated memory, the CPU generates a page fault, which the kernel handles by allocating physical memory, zeroing it, and mapping it. You’ll add this lazy allocation feature to xv6 in this lab.

实现一个内存页懒分配机制，在调用 sbrk() 的时候，不立即分配内存，而是只作记录。在访问到这一部分内存的时候才进行实际的物理内存分配。

首先修改 sys_sbrk，使其不再调用 growproc()，而是只修改 p->sz 的值而不分配物理内存。

```c
// kernel/sysproc.c
uint64
sys_sbrk(void)
{
  int addr;
  int n;
  struct proc *p = myproc();
  if(argint(0, &n) < 0)
    return -1;
  addr = p->sz;
  if(n < 0) {
    uvmdealloc(p->pagetable, p->sz, p->sz+n); // 如果是缩小空间，则马上释放
  }
  p->sz += n; // 懒分配
  return addr;
}
```

修改 usertrap 用户态 trap 处理函数，为缺页异常添加检测，如果为缺页异常（`(r_scause() == 13 || r_scause() == 15)`），且发生异常的地址是由于懒分配而没有映射的话，就为其分配物理内存，并在页表建立映射：

```c
// kernel/trap.c

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  // ......

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    uint64 va = r_stval();
    if((r_scause() == 13 || r_scause() == 15) && uvmshouldtouch(va)){ // 缺页异常，并且发生异常的地址进行过懒分配
      uvmlazytouch(va); // 分配物理内存，并在页表创建映射
    } else { // 如果不是缺页异常，或者是在非懒加载地址上发生缺页异常，则抛出错误并杀死进程
      printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
      printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
      p->killed = 1;
    }
  }

  // ......

}
```

uvmlazytouch 函数负责分配实际的物理内存并建立映射。懒分配的内存页在被 touch 后就可以被使用了。uvmshouldtouch 用于检测一个虚拟地址是不是一个需要被 touch 的懒分配内存地址，具体检测的是：

1. 处于 `[0, p->sz)`地址范围之中（进程申请的内存范围）
2. `不是栈的 guard page`（具体见 xv6 book，栈页的低一页故意留成不映射，作为哨兵用于捕捉 stack overflow 错误。懒分配不应该给这个地址分配物理页和建立映射，而应该直接抛出异常）
3. `页表项不存在`

```c
// kernel/vm.c

// touch a lazy-allocated page so it's mapped to an actual physical page.
void uvmlazytouch(uint64 va) {
  struct proc *p = myproc();
  char *mem = kalloc();
  if(mem == 0) {
    // failed to allocate physical memory
    printf("lazy alloc: out of memory\n");
    p->killed = 1;
  } else {
    memset(mem, 0, PGSIZE);
    if(mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      printf("lazy alloc: failed to map page\n");
      kfree(mem);
      p->killed = 1;
    }
  }
  // printf("lazy alloc: %p, p->sz: %p\n", PGROUNDDOWN(va), p->sz);
}

// whether a page is previously lazy-allocated and needed to be touched before use.
int uvmshouldtouch(uint64 va) {
  pte_t *pte;
  struct proc *p = myproc();
  
  return va < p->sz // within size of memory for the process
    && PGROUNDDOWN(va) != r_sp() // not accessing stack guard page (it shouldn't be mapped)
    && (((pte = walk(p->pagetable, va, 0))==0) || ((*pte & PTE_V)==0)); // page table entry does not exist
}
```

`由于懒分配的页，在刚分配的时候是没有对应的映射的，所以要把一些原本在遇到无映射地址时会 panic 的函数的行为改为直接忽略这样的地址。`

uvmummap()：取消虚拟地址映射

```c
// kernel/vm.c
// 修改这个解决了 proc_freepagetable 时的 panic
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0) {
      continue; // 如果页表项不存在，跳过当前地址 （原本是直接panic）
    }
    if((*pte & PTE_V) == 0){
      continue; // 如果页表项不存在，跳过当前地址 （原本是直接panic）
    }
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```

uvmcopy()：将父进程的页表以及内存拷贝到子进程

```c
// kernel/vm.c
// 修改这个解决了 fork 时的 panic
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue; // 如果一个页不存在，则认为是懒加载的页，忽略即可
    if((*pte & PTE_V) == 0)
      continue; // 如果一个页不存在，则认为是懒加载的页，忽略即可
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

copyin() 和 copyout()：内核/用户态之间互相拷贝数据

`由于这里可能会访问到懒分配但是还没实际分配的页，所以要加一个检测，确保 copy 之前，用户态地址对应的页都有被实际分配和映射。`

```c
// kernel/vm.c
// 修改这个解决了 read/write 时的错误 (usertests 中的 sbrkarg 失败的问题)
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  if(uvmshouldtouch(dstva))
    uvmlazytouch(dstva);

  // ......

}

int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  if(uvmshouldtouch(srcva))
    uvmlazytouch(srcva);

  // ......

}
```

至此修改完成，在 xv6 中运行 lazytests 和 usertests 都应该能够成功了。如果在某一步出现了 remap 或者 leaf 之类的 panic，可能是由于页表项没有释放干净。可以从之前 pgtbl 实验中借用打印页表的函数 vmprint 的代码，并在可能有关的系统调用中打出，方便对页表进行调试。

> tip. 如果 usertests 某一步失败了，可以用 `usertests [测试名称]` 直接单独运行某个之前失败过的测试，例如 `usertests stacktest` 可以直接运行栈 guard page 的测试，而不用等待其他测试漫长的运行。

### Lab 6: Copy-on-write fork

> COW fork() creates just a pagetable for the child, with PTEs for user memory pointing to the parent’s physical pages. COW fork() marks all the user PTEs in both parent and child as not writable. When either process tries to write one of these COW pages, the CPU will force a page fault. The kernel page-fault handler detects this case, allocates a page of physical memory for the faulting process, copies the original page into the new page, and modifies the relevant PTE in the faulting process to refer to the new page, this time with the PTE marked writeable. When the page fault handler returns, the user process will be able to write its copy of the page.
>
> COW fork() makes freeing of the physical pages that implement user memory a little trickier. A given physical page may be referred to by multiple processes’ page tables, and should be freed only when the last reference disappears.

实现 fork 懒复制机制，在进程 fork 后，不立刻复制内存页，而是将虚拟地址指向与父进程相同的物理地址。在父子任意一方尝试对内存页进行修改时，才对内存页进行复制。 物理内存页必须保证在所有引用都消失后才能被释放，这里需要有引用计数机制。

> 为了便于区分，本文将只创建引用而不进行实际内存分配的页复制过程称为「懒复制」，将分配新的内存空间并将数据复制到其中的过程称为「实复制」

#### 6.1fork 时不立刻复制内存

首先修改 uvmcopy()，在复制父进程的内存到子进程的时候，不立刻复制数据，而是建立指向原物理页的映射，`并将父子两端的页表项都设置为不可写`。

```c
// kernel/vm.c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    // 清除父进程的 PTE_W 标志位，设置 PTE_COW 标志位表示是一个懒复制页（多个进程引用同个物理页）
    *pte = (*pte & ~PTE_W) | PTE_COW;
    flags = PTE_FLAGS(*pte);
    // 将父进程的物理页直接 map 到子进程 （懒复制）
    // 权限设置和父进程一致（不可写，PTE_COW）
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      goto err;
    }
    // 将物理页的引用次数增加 1
    krefpage((void*)pa);
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

上面用到了 PTE_COW 标志位，用于标示一个映射对应的物理页是否是懒复制页。这里 PTE_COW 需要在 riscv.h 中定义：

```c
// kernel/riscv.h
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // 1 -> user can access
#define PTE_COW (1L << 8) // 是否为懒复制页，使用页表项 flags 中保留的第 8 位表示
// （页表项 flags 中，第 8、9、10 位均为保留给操作系统使用的位，可以用作任意自定义用途）
```

这样，fork 时就不会立刻复制内存，只会创建一个映射了。这时候如果尝试修改懒复制的页，会出现 page fault 被 usertrap() 捕获。接下来需要在 usertrap() 中捕捉这个 page fault，并在尝试修改页的时候，执行实复制操作。

#### 6.2捕获写操作并执行复制

与 lazy allocation lab 类似，在 usertrap() 中添加对 page fault 的检测，并在当前访问的地址符合懒复制页条件时，对懒复制页进行实复制操作：

```c
// kernel/trap.c
void
usertrap(void)
{

  // ......

  } else if((which_dev = devintr()) != 0){
    // ok
  } else if((r_scause() == 13 || r_scause() == 15) && uvmcheckcowpage(r_stval())) { // copy-on-write
    if(uvmcowcopy(r_stval()) == -1){ // 如果内存不足，则杀死进程
      p->killed = 1;
    }
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  // ......

}
```

`同时 copyout() 由于是软件访问页表，不会触发缺页异常，所以需要手动添加同样的监测代码（同 lab5），检测接收的页是否是一个懒复制页，若是，执行实复制操作`：

```c
// kernel/vm.c
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  if(uvmcheckcowpage(dstva))
    uvmcowcopy(dstva);

  // ......
}
```

实现懒复制页的检测（`uvmcheckcowpage()`）与实复制（`uvmcowcopy()`）操作：

```c
// kernel/vm.c
// 检查一个地址指向的页是否是懒复制页
int uvmcheckcowpage(uint64 va) {
  pte_t *pte;
  struct proc *p = myproc();
  
  return va < p->sz // 在进程内存范围内
    && ((pte = walk(p->pagetable, va, 0))!=0)
    && (*pte & PTE_V) // 页表项存在
    && (*pte & PTE_COW); // 页是一个懒复制页
}

// 实复制一个懒复制页，并重新映射为可写
int uvmcowcopy(uint64 va) {
  pte_t *pte;
  struct proc *p = myproc();

  if((pte = walk(p->pagetable, va, 0)) == 0)
    panic("uvmcowcopy: walk");
  
  // 调用 kalloc.c 中的 kcopy_n_deref 方法，复制页
  // (如果懒复制页的引用已经为 1，则不需要重新分配和复制内存页，只需清除 PTE_COW 标记并标记 PTE_W 即可)
  uint64 pa = PTE2PA(*pte);
  uint64 new = (uint64)kcopy_n_deref((void*)pa); // 将一个懒复制的页引用变为一个实复制的页
  if(new == 0)
    return -1;
  
  // 重新映射为可写，并清除 PTE_COW 标记
  uint64 flags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
  uvmunmap(p->pagetable, PGROUNDDOWN(va), 1, 0);
  if(mappages(p->pagetable, va, 1, new, flags) == -1) {
    panic("uvmcowcopy: mappages");
  }
  return 0;
}
```

到这里，就已经确定了大体的逻辑了：==在 fork 的时候不复制数据只建立映射+标记，在进程尝试写入的时候进行实复制并重新映射为可写。==

`接下来，还需要做页的生命周期管理，确保在所有进程都不使用一个页时才将其释放`

#### 6.3物理页生命周期以及引用计数

在 kalloc.c 中，我们需要定义一系列的新函数，用于完成在支持懒复制的条件下的物理页生命周期管理。在原本的 xv6 实现中，一个物理页的生命周期内，可以支持以下操作：

- kalloc(): 分配物理页
- kfree(): 释放回收物理页

而在支持了懒分配后，由于一个物理页可能被多个进程（多个虚拟地址）引用，并且必须在最后一个引用消失后才可以释放回收该物理页，所以一个物理页的生命周期内，现在需要支持以下操作：

- `kalloc(): 分配物理页，将其引用计数置为 1`
- `krefpage(): 创建物理页的一个新引用，引用计数加 1`
- `kcopy_n_deref(): 将物理页的一个引用实复制到一个新物理页上（引用计数为 1），返回得到的副本页；并将本物理页的引用计数减 1`
- `kfree(): 释放物理页的一个引用，引用计数减 1；如果计数变为 0，则释放回收物理页`

> 一个物理页 p 首先会被父进程使用 kalloc() 创建，fork 的时候，新创建的子进程会使用 krefpage() 声明自己对父进程物理页的引用。当尝试修改父进程或子进程中的页时，kcopy_n_deref() 负责将想要修改的页实复制到独立的副本，并记录解除旧的物理页的引用（引用计数减 1）。最后 kfree() 保证只有在所有的引用者都释放该物理页的引用时，才释放回收该物理页。

这里首先定义一个数组 pageref[] 以及对应的宏，用于记录与获取某个物理页的引用计数：

```c
// kernel/kalloc.c

// 用于访问物理页引用计数数组
#define PA2PGREF_ID(p) (((p)-KERNBASE)/PGSIZE)
#define PGREF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP)

struct spinlock pgreflock; // 用于 pageref 数组的锁，防止竞态条件引起内存泄漏
int pageref[PGREF_MAX_ENTRIES]; // 从 KERNBASE 开始到 PHYSTOP 之间的每个物理页的引用计数
// note:  reference counts are incremented on fork, not on mapping. this means that multiple mappings of the same physical page within a single process are only counted as one reference. this shouldn't be a problem, though. as there's no way for a user program to map a physical page twice within it's address space in xv6.
// 通过物理地址获得引用计数
#define PA2PGREF(p) pageref[PA2PGREF_ID((uint64)(p))]

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pgreflock, "pgref"); // 初始化锁
  freerange(end, (void*)PHYSTOP);
}

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&pgreflock);
  if(--PA2PGREF(pa) <= 0) {
    // 当页面的引用计数小于等于 0 的时候，释放页面

    // Fill with junk to catch dangling refs.
    // pa will be memset multiple times if race-condition occurred.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pgreflock);
}

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 新分配的物理页的引用计数为 1
    // (这里无需加锁)
    PA2PGREF(r) = 1;
  }
  
  return (void*)r;
}

// Decrease reference to the page by one if it's more than one, then
// allocate a new physical page and copy the page into it.
// (Effectively turing one reference into one copy.)
// 
// Do nothing and simply return pa when reference count is already
// less than or equal to 1.
// 
// 当引用已经小于等于 1 时，不创建和复制到新的物理页，而是直接返回该页本身
void *kcopy_n_deref(void *pa) {
  acquire(&pgreflock);
  
  // 这一步很关键，当子进程先执行exec释放原来的物理页时此时父进程写pa地址处内容会触发COW，由于没有子进程引用pa，父进程不需要重新分配一块物理内容可以直接复用原来的物理页，并修改PTE权限
  if(PA2PGREF(pa) <= 1) { // 只有 1 个引用，无需复制
    release(&pgreflock);
    return pa;
  }

  // 分配新的内存页，并复制旧页中的数据到新页
  uint64 newpa = (uint64)kalloc();
  if(newpa == 0) {
    release(&pgreflock);
    return 0; // out of memory
  }
  memmove((void*)newpa, (void*)pa, PGSIZE);

  // 旧页的引用减 1
  PA2PGREF(pa)--;

  release(&pgreflock);
  return (void*)newpa;
}

// 为 pa 的引用计数增加 1
void krefpage(void *pa) {
  acquire(&pgreflock);
  PA2PGREF(pa)++;
  release(&pgreflock);
}
```

这里可以看到，为 pageref[] 数组定义了自旋锁 pgreflock，并且在除了 kalloc 的其他操作中，都使用了 `acquire(&pgreflock);` 和 `release(&pgreflock);` 获取和释放锁来保护操作的代码。这里的锁的作用是防止竞态条件（race-condition）下导致的内存泄漏。

举一个很常见的 fork() 后 exec() 的例子：

```tex
父进程: 分配物理页 p（p 引用计数 = 1）
父进程: fork()（p 引用计数 = 2）
父进程: 尝试修改 p，触发页异常
父进程: 由于 p 引用计数大于 1，开始实复制 p（p 引用计数 = 2）
--- 调度器切换到子进程
子进程: exec() 替换进程影像，释放所有旧的页
子进程: 尝试释放 p（引用计数减 1），子进程丢弃对 p 的引用（p 引用计数 = 1）
--- 调度器切换到父进程
父进程: （继续实复制p）创建新页 q，将 p 复制到 q，将 q 标记为可写并建立映射，在这过程中父进程丢弃对旧 p 的引用
```

`在这一个执行流过后，最终结果是物理页p并没有被释放回收，然而父进程和子进程都已经丢弃了对p的引用（页表中均没有指向p的页表项），这样一来p占用的内存就属于泄漏内存了，永远无法被回收。加了锁pgreflock之后，保证了这种情况不会出现。`

==注意 kalloc() 可以不用加锁，因为 kmem 的锁已经保证了同一个物理页不会同时被两个进程分配，并且在 kalloc() 返回前，其他操作 pageref() 的函数也不会被调用，因为没有任何其他进程能够在 kalloc() 返回前得到这个新页的地址。==

### Lab 7: Multithreading

> This lab will familiarize you with multithreading. You will implement switching between threads in a user-level threads package, use multiple threads to speed up a program, and implement a barrier.

实现一个用户态的线程库；尝试使用线程来为程序提速；并且尝试实现一个同步屏障。

#### 7.1Uthread: switching between threads (moderate)

补全 uthread.c，完成用户态线程功能的实现。

这里的线程相比现代操作系统中的线程而言，更接近一些语言中的“协程”（coroutine）。原因是这里的“线程”是完全用户态实现的，多个线程也只能运行在一个 CPU 上，并且没有时钟中断来强制执行调度，需要线程函数本身在合适的时候主动 yield 释放 CPU。这样实现起来的线程并不对线程函数透明，所以比起操作系统的线程而言更接近 coroutine。

这个实验其实相当于在用户态重新实现一遍 xv6 kernel 中的 scheduler() 和 swtch() 的功能，所以大多数代码都是可以借鉴的。

uthread_switch.S 中需要实现上下文切换的代码，这里借鉴 swtch.S：

```assembly
// uthread_switch.S
	.text
	/*
		 * save the old thread's registers,
		 * restore the new thread's registers.
		 */
// void thread_switch(struct context *old, struct context *new);
	.globl thread_switch
thread_switch:
	sd ra, 0(a0)
	sd sp, 8(a0)
	sd s0, 16(a0)
	sd s1, 24(a0)
	sd s2, 32(a0)
	sd s3, 40(a0)
	sd s4, 48(a0)
	sd s5, 56(a0)
	sd s6, 64(a0)
	sd s7, 72(a0)
	sd s8, 80(a0)
	sd s9, 88(a0)
	sd s10, 96(a0)
	sd s11, 104(a0)

	ld ra, 0(a1)
	ld sp, 8(a1)
	ld s0, 16(a1)
	ld s1, 24(a1)
	ld s2, 32(a1)
	ld s3, 40(a1)
	ld s4, 48(a1)
	ld s5, 56(a1)
	ld s6, 64(a1)
	ld s7, 72(a1)
	ld s8, 80(a1)
	ld s9, 88(a1)
	ld s10, 96(a1)
	ld s11, 104(a1)

	ret    /* return to ra */
```

在调用本函数 uthread_switch() 的过程中，caller-saved registers 已经被调用者保存到栈帧中了，所以这里无需保存这一部分寄存器。

> 引申：内核调度器无论是通过时钟中断进入（usertrap），还是线程自己主动放弃 CPU（sleep、exit），最终都会调用到 yield 进一步调用 swtch。 由于上下文切换永远都发生在函数调用的边界（swtch 调用的边界），恢复执行相当于是 swtch 的返回过程，会从堆栈中恢复 caller-saved 的寄存器， 所以用于保存上下文的 context 结构体只需保存 callee-saved 寄存器，以及 返回地址 ra、栈指针 sp 即可。恢复后执行到哪里是通过 ra 寄存器来决定的（swtch 末尾的 ret 转跳到 ra）
>
> 而 trapframe 则不同，一个中断可能在任何地方发生，不仅仅是函数调用边界，也有可能在函数执行中途，所以恢复的时候需要靠 pc 寄存器来定位。 并且由于切换位置不一定是函数调用边界，所以几乎所有的寄存器都要保存（无论 caller-saved 还是 callee-saved），才能保证正确的恢复执行。 这也是内核代码中 `struct trapframe` 中保存的寄存器比 `struct context` 多得多的原因。
>
> 另外一个，无论是程序主动 sleep，还是时钟中断，都是通过 trampoline 跳转到内核态 usertrap（保存 trapframe），然后再到达 swtch 保存上下文的。 恢复上下文都是恢复到 swtch 返回前（依然是内核态），然后返回跳转回 usertrap，再继续运行直到 usertrapret 跳转到 trampoline 读取 trapframe，并返回用户态。 也就是上下文恢复并不是直接恢复到用户态，而是恢复到内核态 swtch 刚执行完的状态。负责恢复用户态执行流的其实是 trampoline 以及 trapframe。

从 proc.h 中借鉴一下 context 结构体，用于保存 ra、sp 以及 callee-saved registers：

```c
// uthread.c
// Saved registers for thread context switches.
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

struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context ctx; // 在 thread 中添加 context 结构体
};
struct thread all_thread[MAX_THREAD];
struct thread *current_thread;

extern void thread_switch(struct context* old, struct context* new); // 修改 thread_switch 函数声明
```

在 thread_schedule 中调用 thread_switch 进行上下文切换：

```c
// uthread.c
void 
thread_schedule(void)
{
  // ......

  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    thread_switch(&t->ctx, &next_thread->ctx); // 切换线程
  } else
    next_thread = 0;
}
```

这里有个小坑是要从 t 切换到 next_thread，不是从 current_thread 切换到 next_thread（因为前面有两句赋值，没错，我在这里眼瞎了卡了一下 QAQ）

再补齐 thread_create：

```c
// uthread.c
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  t->ctx.ra = (uint64)func;       // 返回地址
  // thread_switch 的结尾会返回到 ra，从而运行线程代码
  t->ctx.sp = (uint64)&t->stack + (STACK_SIZE - 1);  // 栈指针
  // 将线程的栈指针指向其独立的栈，注意到栈的生长是从高地址到低地址，所以
  // 要将 sp 设置为指向 stack 的最高地址
}
```

添加的部分为设置上下文中 ra 指向的地址为线程函数的地址，这样在第一次调度到该线程，执行到 thread_switch 中的 ret 之后就可以跳转到线程函数从而开始执行了。设置 sp 使得线程拥有自己独有的栈，也就是独立的执行流。

#### 7.2Using threads (moderate)

分析并解决一个哈希表操作的例子内，由于 race-condition 导致的数据丢失的问题。

> Why are there missing keys with 2 threads, but not with 1 thread? Identify a sequence of events with 2 threads that can lead to a key being missing. Submit your sequence with a short explanation in answers-thread.txt

```tex
[假设键 k1、k2 属于同个 bucket]

thread 1: 尝试设置 k1
thread 1: 发现 k1 不存在，尝试在 bucket 末尾插入 k1
--- scheduler 切换到 thread 2
thread 2: 尝试设置 k2
thread 2: 发现 k2 不存在，尝试在 bucket 末尾插入 k2
thread 2: 分配 entry，在桶末尾插入 k2
--- scheduler 切换回 thread 1
thread 1: 分配 entry，没有意识到 k2 的存在，在其认为的 “桶末尾”（实际为 k2 所处位置）插入 k1

[k1 被插入，但是由于被 k1 覆盖，k2 从桶中消失了，引发了键值丢失]
```

首先先暂时忽略速度，为 put 和 get 操作加锁保证安全：

```c
// ph.c
pthread_mutex_t lock;

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;
  
  pthread_mutex_init(&lock, NULL);

  // ......
}

static 
void put(int key, int value)
{
   NBUCKET;

  pthread_mutex_lock(&lock);
  
  // ......

  pthread_mutex_unlock(&lock);
}

static struct entry*
get(int key)
{
  $ NBUCKET;

  pthread_mutex_lock(&lock);
  
  // ......

  pthread_mutex_unlock(&lock);

  return e;
}
```

加完这个锁，就可以通过 ph_safe 测试了，编译执行：

```tex\
$ ./ph 1
100000 puts, 4.652 seconds, 21494 puts/second
0: 0 keys missing
100000 gets, 5.098 seconds, 19614 gets/second
$ ./ph 2
100000 puts, 5.224 seconds, 19142 puts/second
0: 0 keys missing
1: 0 keys missing
200000 gets, 10.222 seconds, 19566 gets/second
```

可以发现，多线程执行的版本也不会丢失 key 了，说明加锁成功防止了 race-condition 的出现。

但是仔细观察会发现，加锁后多线程的性能变得比单线程还要低了，虽然不会出现数据丢失，但是失去了多线程并行计算的意义：提升性能。

这里的原因是，我们为整个操作加上了互斥锁，意味着每一时刻只能有一个线程在操作哈希表，这里实际上等同于将哈希表的操作变回单线程了，又由于锁操作（加锁、解锁、锁竞争）是有开销的，所以性能甚至不如单线程版本。

这里的优化思路，也是多线程效率的一个常见的优化思路，就是降低锁的粒度。由于哈希表中，不同的 bucket 是互不影响的，一个 bucket 处于修改未完全的状态并不影响 put 和 get 对其他 bucket 的操作，所以实际上只需要确保两个线程不会同时操作同一个 bucket 即可，并不需要确保不会同时操作整个哈希表。

所以可以将加锁的粒度，从整个哈希表一个锁降低到每个 bucket 一个锁。

```c
// ph.c
pthread_mutex_t locks;

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;
  
  for(int i=0;i<NBUCKET;i++) {
    pthread_mutex_init(&locks[i], NULL); 
  }

  // ......
}

static 
void put(int key, int value)
{
  int i = key % NBUCKET;

  pthread_mutex_lock(&locks[i]);
  
  // ......

  pthread_mutex_unlock(&locks[i]);
}

static struct entry*
get(int key)
{
  int i = key % NBUCKET;

  pthread_mutex_lock(&locks[i]);
  
  // ......

  pthread_mutex_unlock(&locks[i]);

  return e;
}
```

在这样修改后，编译执行：

```tex
$ ./ph 1
100000 puts, 4.940 seconds, 20241 puts/second
0: 0 keys missing
100000 gets, 4.934 seconds, 20267 gets/second
$ ./ph 2
100000 puts, 3.489 seconds, 28658 puts/second
0: 0 keys missing
1: 0 keys missing
200000 gets, 6.104 seconds, 32766 gets/second
$ ./ph 4
100000 puts, 1.881 seconds, 53169 puts/second
0: 0 keys missing
3: 0 keys missing
2: 0 keys missing
1: 0 keys missing
400000 gets, 7.376 seconds, 54229 gets/second
```

可以看到，多线程版本的性能有了显著提升（虽然由于锁开销，依然达不到理想的 `单线程速度 * 线程数` 那么快），并且依然没有 missing key。

此时再运行 grade，就可以通过 ph_fast 测试了。

#### 7.3Barrier (moderate)

利用 pthread 提供的条件变量方法，实现同步屏障机制。

```c
// barrier.c
static void 
barrier()
{
  pthread_mutex_lock(&bstate.barrier_mutex);
  if(++bstate.nthread < nthread) {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  } else {
    bstate.nthread = 0;
    bstate.round++;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```

`线程进入同步屏障 barrier 时，将已进入屏障的线程数量增加 1，然后再判断是否已经达到总线程数。如果未达到，则进入睡眠，等待其他线程。如果已经达到，则唤醒所有在 barrier 中等待的线程，所有线程继续执行；屏障轮数 + 1；`

「将已进入屏障的线程数量增加 1，然后再判断是否已经达到总线程数」这一步并不是原子操作，并且这一步和后面的两种情况中的操作「睡眠」和「唤醒」之间也不是原子的，如果在这里发生 race-condition，则会导致出现 「lost wake-up 问题」（线程 1 即将睡眠前，线程 2 调用了唤醒，然后线程 1 才进入睡眠，导致线程 1 本该被唤醒而没被唤醒）

> 解决方法是，「屏障的线程数量增加 1；判断是否已经达到总线程数；进入睡眠」这三步必须原子。所以使用一个互斥锁 barrier_mutex 来保护这一部分代码。pthread_cond_wait 会在进入睡眠的时候原子性的释放 barrier_mutex，从而允许后续线程进入 barrier，防止死锁。

### Lab 8: Locks

重新设计代码以降低锁竞争，提高多核机器上系统的并行性。

#### 8.1Memory allocator (moderate)

通过拆分 kmem 中的空闲内存链表，降低 kalloc 实现中的 kmem 锁竞争。

##### 8.1.1原理与分析

kalloc 原本的实现中，使用 freelist 链表，将空闲物理页本身直接用作链表项（这样可以不使用额外空间）连接成一个链表，在分配的时候，将物理页从链表中移除，回收时将物理页放回链表中。

```c
// kernel/kalloc.c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
```

分配物理页的实现（原版）：

```c
// kernel/kalloc.c
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist; // 取出一个物理页。页表项本身就是物理页。
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

在这里无论是分配物理页或释放物理页，都需要修改 freelist 链表。由于修改是多步操作，为了保持多线程一致性，必须加锁。但这样的设计也使得多线程无法并发申请内存，限制了并发效率。

证据是 kmem 锁上频繁的锁竞争：

```shell
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 83375 #acquire() 433015
lock: bcache: #fetch-and-add 0 #acquire() 1260
--- top 5 contended locks:
lock: kmem: #fetch-and-add 83375 #acquire() 433015  // kmem 是整个系统中竞争最激烈的锁
lock: proc: #fetch-and-add 23737 #acquire() 130718
lock: virtio_disk: #fetch-and-add 11159 #acquire() 114
lock: proc: #fetch-and-add 5937 #acquire() 130786
lock: proc: #fetch-and-add 4080 #acquire() 130786
tot= 83375
test1 FAIL
```

这里体现了一个先 profile 再进行优化的思路。如果一个大锁并不会引起明显的性能问题，有时候大锁就足够了。只有在万分确定性能热点是在该锁的时候才进行优化，「过早优化是万恶之源」。

这里解决性能热点的思路是「将共享资源变为不共享资源」。锁竞争优化一般有几个思路：

- `只在必须共享的时候共享（对应为将资源从 CPU 共享拆分为每个 CPU 独立）`
- `必须共享时，尽量减少在关键区中停留的时间（对应“大锁化小锁”，降低锁的粒度）`

> 该 lab 的实验目标，即是为每个 CPU 分配独立的 freelist，这样多个 CPU 并发分配物理页就不再会互相排斥了，提高了并行性。

`但由于在一个 CPU freelist 中空闲页不足的情况下，仍需要从其他 CPU 的 freelist 中“偷”内存页，所以一个 CPU 的 freelist 并不是只会被其对应 CPU 访问，还可能在“偷”内存页的时候被其他 CPU 访问，故仍然需要使用单独的锁来保护每个 CPU 的 freelist。`但一个 CPU freelist 中空闲页不足的情况相对来说是比较稀有的，所以总体性能依然比单独 kmem 大锁要快。在最佳情况下，也就是没有发生跨 CPU “偷”页的情况下，这些小锁不会发生任何锁竞争。

##### 8.1.2代码实现

```c
// kernel/kalloc.c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU]; // 为每个 CPU 分配独立的 freelist，并用独立的锁保护它。

char *kmem_lock_names[] = {
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",
};

void
kinit()
{
  for(int i=0;i<NCPU;i++) { // 初始化所有锁
    initlock(&kmem[i].lock, kmem_lock_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}
```

```c
// kernel/kalloc.c
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  int cpu = cpuid();

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();
}

void *
kalloc(void)
{
  struct run *r;

  push_off();

  int cpu = cpuid();

  acquire(&kmem[cpu].lock);

  if(!kmem[cpu].freelist) { // no page left for this cpu
    int steal_left = 64; // steal 64 pages from other cpu(s)
    for(int i=0;i<NCPU;i++) {
      if(i == cpu) continue; // no self-robbery
      acquire(&kmem[i].lock);
      struct run *rr = kmem[i].freelist;
      while(rr && steal_left) {
        kmem[i].freelist = rr->next;
        rr->next = kmem[cpu].freelist;
        kmem[cpu].freelist = rr;
        rr = kmem[i].freelist;
        steal_left--;
      }
      release(&kmem[i].lock);
      if(steal_left == 0) break; // done stealing
    }
  }

  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock);

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

这里选择在内存页不足的时候，从其他的 CPU “偷” 64 个页，这里的数值是随意取的，`在现实场景中，最好进行测量后选取合适的数值，尽量使得“偷”页频率低。`

> 上述代码可能产生死锁（cpu_a 尝试偷 cpu_b，cpu_b 尝试偷 cpu_a）

### Lab 9: File Systems

为 xv6 的文件系统添加大文件以及符号链接支持。该 lab 难度较低。

#### 9.1Large files (moderate)

##### 9.1.1原理与分析

与 FAT 文件系统类似，xv6 文件系统中的每一个 inode 结构体中，采用了`混合索引`的方式记录数据的所在具体盘块号。每个文件所占用的前 12 个盘块的盘块号是直接记录在 inode 中的（每个盘块 1024 字节），所以对于任何文件的前 12 KB 数据，都可以通过访问 inode 直接得到盘块号。这一部分称为直接记录盘块。

对于大于 12 个盘块的文件，大于 12 个盘块的部分，会分配一个额外的一级索引表（一盘块大小，1024Byte），用于存储这部分数据的所在盘块号。

由于一级索引表可以包含 BSIZE(1024) / 4 = 256 个盘块号，加上 inode 中的 12 个盘块号，一个文件最多可以使用 12+256 = 268 个盘块，也就是 268KB。

inode 结构（含有 NDIRECT=12 个直接记录盘块，还有一个一级索引盘块，后者又可额外包含 256 个盘块号）：

```c
// kernel/fs.c

// note: NDIRECT=12
// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};
```

本 lab 的目标是通过为混合索引机制添加二级索引页，来扩大能够支持的最大文件大小。

![image-20221119195251727](C:\Users\lan\AppData\Roaming\Typora\typora-user-images\image-20221119195251727.png)

本 lab 比较简单，主要前置是需要对文件系统的理解，确保充分理解 xv6 book 中的 file system 相关部分。

##### 9.1.2代码实现

首先修改 struct inode（内存中的 inode 副本结构体）以及 struct dinode（磁盘上的 inode 结构体），将 NDIRECT 直接索引的盘块号减少 1，腾出 inode 中的空间来存储二级索引的索引表盘块号。

```c
// kernel/fs.h
#define NDIRECT 11 // 12 -> 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses (NDIRECT+1 -> NDIRECT+2)
};
```

```c
// kernel/file.h
// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+2]; // NDIRECT+1 -> NDIRECT+2
};
```

修改 bmap（获取 inode 中第 bn 个块的块号）和 itrunc（释放该 inode 所使用的所有数据块），让其能够识别二级索引。（基本上和复制粘贴一致，只是在查出一级块号后，需将一级块中的数据读入，然后再次查询）

```c
// kernel/fs.c

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){ // singly-indirect
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  bn -= NINDIRECT;

  if(bn < NINDIRECT * NINDIRECT) { // doubly-indirect
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT+1]) == 0)
      ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn/NINDIRECT]) == 0){
      a[bn/NINDIRECT] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    bn %= NINDIRECT;
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Caller must hold ip->lock.
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  if(ip->addrs[NDIRECT+1]){
    bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j]) {
        struct buf *bp2 = bread(ip->dev, a[j]);
        uint *a2 = (uint*)bp2->data;
        for(int k = 0; k < NINDIRECT; k++){
          if(a2[k])
            bfree(ip->dev, a2[k]);
        }
        brelse(bp2);
        bfree(ip->dev, a[j]);
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT+1]);
    ip->addrs[NDIRECT + 1] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}
```

#### 9.2Symbolic links (moderate)

实现符号链接机制。

##### 9.2.1原理与分析

`符号链接（软链接）是一类特殊的文件， 其包含有一条以绝对路径或者相对路径的形式指向其它文件或者目录的引用。`符号链接的操作是透明的：对符号链接文件进行读写的程序会表现得直接对目标文件进行操作。某些需要特别处理符号链接的程序（如备份程序）可能会识别并直接对其进行操作。

==一个符号链接文件仅包含有一个文本字符串，其被操作系统解释为一条指向另一个文件或者目录的路径。它是一个独立文件，其存在并不依赖于目标文件。如果删除一个符号链接，它指向的目标文件不受影响。如果目标文件被移动、重命名或者删除，任何指向它的符号链接仍然存在，但是它们将会指向一个不复存在的文件。这种情况被有时被称为被遗弃。==

##### 9.2.2代码实现

首先实现 symlink 系统调用，用于创建符号链接。 符号链接与普通的文件一样，需要占用 inode 块。这里使用 inode 中的第一个 direct-mapped 块（1024字节）来存储符号链接指向的文件。

```c
// kernel/sysfile.c

uint64
sys_symlink(void)
{
  struct inode *ip;
  char target[MAXPATH], path[MAXPATH];
  if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
    return -1;

  begin_op();

  ip = create(path, T_SYMLINK, 0, 0);
  if(ip == 0){
    end_op();
    return -1;
  }

  // use the first data block to store target path.
  if(writei(ip, 0, (uint64)target, 0, strlen(target)) < 0) {
    end_op();
    return -1;
  }

  iunlockput(ip);

  end_op();
  return 0;
}
```

在 fcntl.h 中补齐 O_NOFOLLOW 的定义：

```c
#define O_RDONLY   0x000
#define O_WRONLY   0x001
#define O_RDWR     0x002
#define O_CREATE   0x200
#define O_TRUNC    0x400
#define O_NOFOLLOW 0x800
```

修改 sys_open，使其在遇到符号链接的时候，可以递归跟随符号链接，直到跟随到非符号链接的 inode 为止。

```c
uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    int symlink_depth = 0;
    while(1) { // recursively follow symlinks
      if((ip = namei(path)) == 0){
        end_op();
        return -1;
      }
      ilock(ip);
      if(ip->type == T_SYMLINK && (omode & O_NOFOLLOW) == 0) {
        if(++symlink_depth > 10) {
          // too many layer of symlinks, might be a loop
          iunlockput(ip);
          end_op();
          return -1;
        }
        if(readi(ip, 0, (uint64)path, 0, MAXPATH) < 0) {
          iunlockput(ip);
          end_op();
          return -1;
        }
        iunlockput(ip);
      } else {
        break;
      }
    }
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  // .......

  iunlock(ip);
  end_op();

  return fd;
}

```

### Lab 10: Mmap

这一个实验是要实现最基础的`mmap`功能。mmap即内存映射文件，将一个文件直接映射到内存当中，之后对文件的读写就可以直接通过对内存进行读写来进行，而对文件的同步则由操作系统来负责完成。使用`mmap`可以避免对文件大量`read`和`write`操作带来的内核缓冲区和用户缓冲区之间的频繁的数据拷贝。在Kafka消息队列等软件中借助`mmap`来实现零拷贝（zero-copy）。

#### 10.1原理与分析

`mmap` 函数的原型如下：

```c
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```

下面介绍一下 `mmap` 函数的各个参数作用：

- `addr`：指定映射的虚拟内存地址，可以设置为 NULL，让 Linux 内核自动选择合适的虚拟内存地址。

- `length`：映射的长度。

- `prot`：映射内存的保护模式，可选值如下：

- - `PROT_EXEC`：可以被执行。
  - `PROT_READ`：可以被读取。
  - `PROT_WRITE`：可以被写入。
  - `PROT_NONE`：不可访问。

- `flags`：指定映射的类型，常用的可选值如下：

- - `MAP_FIXED`：使用指定的起始虚拟内存地址进行映射。
  - `MAP_SHARED`：与其它所有映射到这个文件的进程共享映射空间（可实现共享内存）。
  - `MAP_PRIVATE`：建立一个写时复制（Copy on Write）的私有映射空间。
  - `MAP_LOCKED`：锁定映射区的页面，从而防止页面被交换出内存。
  - ...

- `fd`：进行映射的文件句柄。

- `offset`：文件偏移量（从文件的何处开始映射）。

#### 10.2代码实现

首先定义`vma`结构体用于保存内存映射信息，并在`proc`结构体中加入`struct vma *vma`指针：

```c
#define NVMA 16
#define VMA_START (MAXVA / 2)
struct vma{
  uint64 start;
  uint64 end;
  uint64 length; // 0 means vma not used
  uint64 off;
  int permission;
  int flags;
  struct file *file;
  struct vma *next;

  struct spinlock lock;
};

// Per-process state
struct proc {
  ...
  struct vma *vma;
  ...
};
```

之后实现对`vma`分配的代码：

```c
struct vma vma_list[NVMA];

struct vma* vma_alloc(){
  for(int i = 0; i < NVMA; i++){
    acquire(&vma_list[i].lock);
    if(vma_list[i].length == 0){
      return &vma_list[i];
    }else{
      release(&vma_list[i].lock);
    }
  }
  panic("no enough vma");
}
```

实现`mmap`系统调用，这个函数主要就是申请一个`vma`，之后查找一块空闲内存，填入相关信息，将`vma`插入到进程的`vma`链表中去：

```c
uint64
sys_mmap(void)
{
  uint64 addr;
  int length, prot, flags, fd, offset;
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 || argint(3, &flags) < 0 || argint(4, &fd) < 0 || argint(5, &offset) < 0){
    return -1;
  }

  if(addr != 0)
    panic("mmap: addr not 0");
  if(offset != 0)
    panic("mmap: offset not 0");

  struct proc *p = myproc();
  struct file* f = p->ofile[fd];

  int pte_flag = PTE_U;
  if (prot & PROT_WRITE) {
    if(!f->writable && !(flags & MAP_PRIVATE)) return -1; // map to a unwritable file with PROT_WRITE
    pte_flag |= PTE_W;
  }
  if (prot & PROT_READ) {
    if(!f->readable) return -1; // map to a unreadable file with PROT_READ
    pte_flag |= PTE_R;
  }

  struct vma* v = vma_alloc();
  v->permission = pte_flag;
  v->length = length;
  v->off = offset;
  v->file = myproc()->ofile[fd];
  v->flags = flags;
  filedup(f);
  struct vma* pv = p->vma;
  if(pv == 0){
    v->start = VMA_START;
    v->end = v->start + length;
    p->vma = v;
  }else{
    while(pv->next) pv = pv->next;
    v->start = PGROUNDUP(pv->end);
    v->end = v->start + length;
    pv->next = v;
    v->next = 0;
  }
  addr = v->start;
  printf("mmap: [%p, %p)\n", addr, v->end);

  release(&v->lock);
  return addr;
}
```

接下来就可以在`usertrap`中对缺页中断进行处理：查找进程的`vma`链表，判断该地址是否为映射地址，如果不是就说明出错，直接返回；如果在`vma`链表中，就可以申请并映射一个页面，之后根据`vma`从对应的文件中读取数据：

```c
int
mmap_handler(uint64 va, int scause)
{
  struct proc *p = myproc();
  struct vma* v = p->vma;
  while(v != 0){
    if(va >= v->start && va < v->end){
      break;
    }
    //printf("%p\n", v);
    v = v->next;
  }

  if(v == 0) return -1; // not mmap addr
  if(scause == 13 && !(v->permission & PTE_R)) return -2; // unreadable vma
  if(scause == 15 && !(v->permission & PTE_W)) return -3; // unwritable vma

  // load page from file
  va = PGROUNDDOWN(va);
  char* mem = kalloc();
  if (mem == 0) return -4; // kalloc failed
  
  memset(mem, 0, PGSIZE);

  if(mappages(p->pagetable, va, PGSIZE, (uint64)mem, v->permission) != 0){
    kfree(mem);
    return -5; // map page failed
  }

  struct file *f = v->file;
  ilock(f->ip);
  readi(f->ip, 0, (uint64)mem, v->off + va - v->start, PGSIZE);
  iunlock(f->ip);
  return 0;
}
```

之后就是`munmap`的实现，同样先从链表中找到对应的`vma`结构体，之后根据三种不同情况（头部、尾部、整个）来写回并释放对应的页面并更新`vma`信息，如果整个区域都被释放就将`vma`和文件释放。

```c
uint64
sys_munmap(void)
{
  uint64 addr;
  int length;
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0){
    return -1;
  }

  struct proc *p = myproc();
  struct vma *v = p->vma;
  struct vma *pre = 0;
  while(v != 0){
    if(addr >= v->start && addr < v->end) break; // found
    pre = v;
    v = v->next;
  }

  if(v == 0) return -1; // not mapped
  printf("munmap: %p %d\n", addr, length);
  if(addr != v->start && addr + length != v->end) 
      panic("munmap middle of vma");

  if(addr == v->start){
    writeback(v, addr, length);
    // length最后一页的后半部分需要保留，因此只需要 length / PGSIZE 个页被unmap
    uvmunmap(p->pagetable, addr, length / PGSIZE, 1);
    // 关闭vma结构体的文件
    if(length == v->length){
      // 1、free all
      fileclose(v->file);
      if(pre == 0){
        p->vma = v->next; // head
      }else{
        pre->next = v->next;
        v->next = 0;
      }
      acquire(&v->lock);
      v->length = 0;
      release(&v->lock);
    }else{
      // 2、free head
      v->start += length;
      v->off += length;
      v->length -= length;
    }
  }else{
    // 3、free tail
    v->length -= length;
    v->end -= length;
  }
  return 0;
}
```

写回函数先判断是否需要写回，当需要写回时就仿照`filewrite`的实现，将数据写回到对应的文件当中去，这里的实现是直接写回所有页面，但实际可以根据`PTE_D`来判断内存是否被写入，如果没有写入就不用写回：

```c
void
writeback(struct vma* v, uint64 addr, int n)
{
  if(!(v->permission & PTE_W) || (v->flags & MAP_PRIVATE)) // no need to writeback
    return;

  if((addr % PGSIZE) != 0)
    panic("unmap: not aligned");

  printf("starting writeback: %p %d\n", addr, n);

  struct file* f = v->file;

  int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
  int i = 0;
  while(i < n){
    int n1 = n - i;
    if(n1 > max)
      n1 = max;

    begin_op();
    ilock(f->ip);
    printf("%p %d %d\n",addr + i, v->off + v->start - addr, n1);
    int r = writei(f->ip, 1, addr + i, v->off + addr + i - v->start, n1);
    iunlock(f->ip);
    end_op();
    i += r;
    max -= r
  }
}
```

最后就是在`fork`当中复制`vma`到子进程，在`exit`中当前进程的`vma`链表释放，在`exit`时要对页面进行写回：

```c
int
fork(void)
{
  ...
  np->state = RUNNABLE;

  np->vma = 0;
  struct vma *pv = p->vma;
  struct vma *pre = 0;
  while(pv){
    struct vma *vma = vma_alloc();
    vma->start = pv->start;
    vma->end = pv->end;
    vma->off = pv->off;
    vma->length = pv->length;
    vma->permission = pv->permission;
    vma->flags = pv->flags;
    vma->file = pv->file;
    filedup(vma->file);
    vma->next = 0;
    if(pre == 0){
      np->vma = vma;
    }else{
      pre->next = vma;
    }
    pre = vma;
    release(&vma->lock);
    pv = pv->next;
  }
  ...
}

void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // munmap all mmap vma
  struct vma* v = p->vma;
  struct vma* pv;
  while(v){
    writeback(v, v->start, v->length);
    uvmunmap(p->pagetable, v->start, PGROUNDUP(v->length) / PGSIZE, 1);
    fileclose(v->file);
    pv = v->next;
    acquire(&v->lock);
    v->next = 0;
    v->length = 0;
    release(&v->lock);
    v = pv;
  }
  ...
}
```

### Lab 11: Networking

熟悉系统驱动与外围设备的交互、内存映射寄存器与 DMA 数据传输，实现与 E1000 网卡交互的核心方法：transmit 与 recv。

本 lab 的难度主要在于阅读文档以及理解 CPU 与操作系统是如何与外围设备交互的。换言之，更重要的是理解概念以及 lab 已经写好的模版代码的作用。

```c
int
e1000_transmit(struct mbuf *m)
{
  acquire(&e1000_lock); // 获取 E1000 的锁，防止多进程同时发送数据出现 race

  uint32 ind = regs[E1000_TDT]; // 下一个可用的 buffer 的下标
  struct tx_desc *desc = &tx_ring[ind]; // 获取 buffer 的描述符，其中存储了关于该 buffer 的各种信息
  // 如果该 buffer 中的数据还未传输完，则代表我们已经将环形 buffer 列表全部用完，缓冲区不足，返回错误
  if(!(desc->status & E1000_TXD_STAT_DD)) {
    release(&e1000_lock);
    return -1;
  }
  
  // 如果该下标仍有之前发送完毕但未释放的 mbuf，则释放
  if(tx_mbufs[ind]) {
    mbuffree(tx_mbufs[ind]);
    tx_mbufs[ind] = 0;
  }

  // 将要发送的 mbuf 的内存地址与长度填写到发送描述符中
  desc->addr = (uint64)m->head;
  desc->length = m->len;
  // 设置参数，EOP 表示该 buffer 含有一个完整的 packet
  // RS 告诉网卡在发送完成后，设置 status 中的 E1000_TXD_STAT_DD 位，表示发送完成。
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  // 保留新 mbuf 的指针，方便后续再次用到同一下标时释放。
  tx_mbufs[ind] = m;

  // 环形缓冲区内下标增加一。
  regs[E1000_TDT] = (regs[E1000_TDT] + 1) % TX_RING_SIZE;
  
  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  while(1) { // 每次 recv 可能接收多个包

    uint32 ind = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    
    struct rx_desc *desc = &rx_ring[ind];
    // 如果需要接收的包都已经接收完毕，则退出
    if(!(desc->status & E1000_RXD_STAT_DD)) {
      return;
    }

    rx_mbufs[ind]->len = desc->length;
    
    net_rx(rx_mbufs[ind]); // 传递给上层网络栈。上层负责释放 mbuf

    // 分配并设置新的 mbuf，供给下一次轮到该下标时使用
    rx_mbufs[ind] = mbufalloc(0); 
    desc->addr = (uint64)rx_mbufs[ind]->head;
    desc->status = 0;

    regs[E1000_RDT] = ind;
  }

}
```

操作系统想要发送数据的时候，将数据放入环形缓冲区数组 tx_ring 内，然后递增 E1000_TDT，网卡会自动将数据发出。当网卡收到数据的时候，网卡首先使用 direct memory access，将数据放入 rx_ring 环形缓冲区数组中，然后向 CPU 发起一个硬件中断，CPU 在收到中断后，直接读取 rx_ring 中的数据即可。

最后来个图片放松一下心情(bushi):stuffed_flatbread::stuffed_flatbread::stuffed_flatbread:

![wallhaven-o5dj1p](C:\Users\lan\Pictures\壁纸\wallhaven-o5dj1p.jpg)

