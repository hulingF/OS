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
        # 算出栈底地址（栈向下生长）并且放到 sp 寄存器中
        add sp, sp, a0
	# jump to start() in start.c
        call start
spin:
        j spin
