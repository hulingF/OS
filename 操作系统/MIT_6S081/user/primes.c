#include "kernel/types.h"
#include "user/user.h"
#define PRIME_NUM 35
#define READEND 0
#define WRITEEND 1

void child(int* pl);

int main(int argc, char *argv[]) {
	// 管道是单方向的，main函数这里的pipe管道是初始数据输入管道
	int p[2];
	pipe(p);
	if (fork() == 0) {
		child(p);	
	} else {
		// 只写数据
		close(p[READEND]);
		// feed the int array
		for (int i=2; i<PRIME_NUM+1; i++) {
			write(p[WRITEEND], &i, sizeof(int));
		}
		// 写完关闭
		close(p[WRITEEND]);
		// 等待子进程结束
		wait((int *) 0);
	}
	exit(0);
}

void child(int* pl) {
	// primes程序中每一个进程拥有两个方向的管道，pl相当于输入管道，pr相当于输出管道
	int pr[2];
	int n;

    // 父进程的pl只负责读入数据
	close(pl[WRITEEND]);
	// tries to read the first number
	int read_result = read(pl[READEND], &n, sizeof(int));
	// 素数已经全部找出，到达最后一个子进程了
	if (read_result == 0)
		exit(0);
	// right side pipe
	pipe(pr);

	if (fork() == 0) {
		// 这种方法fork出来的子进程浪费了很多资源例如父进程的pl、pr全部都复制了一遍，然而子进程根本用不到父进程的pl
		// 合适的解决方式是写时复制
		child(pr);
	} else {
		// 父进程的pr只负责写入新数据
		close(pr[READEND]);
		printf("prime %d\n", n);
		int prime = n;
		while (read(pl[READEND], &n, sizeof(int)) != 0) {
			if (n%prime != 0) {
				write(pr[WRITEEND], &n, sizeof(int));
			}
		}
		// 写完关闭
		close(pl[READEND]);
		close(pr[WRITEEND]);
		// 等待子进程完成
		wait((int *) 0);
		exit(0);
	}
}
