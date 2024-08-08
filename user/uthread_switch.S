	.text
	.globl thread_switch//这个函数需要保存旧线程的寄存器以及恢复新线程的寄存器
thread_switch:// 保存旧线程的寄存器
	sd ra, 0(a0)// 将返回地址寄存器 (ra) 保存到 a0 指向的内存位置
	sd sp, 8(a0) // 将栈指针寄存器 (sp) 保存到 a0 + 8 指向的内存位置
	sd s0, 16(a0)// 将 s0 寄存器保存到 a0 + 16 指向的内存位置
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

	// 恢复新线程的寄存器
	ld ra, 0(a1)// 从 a1 指向的内存位置加载返回地址寄存器 (ra)
	ld sp, 8(a1)// 从 a1 + 8 指向的内存位置加载栈指针寄存器 (sp)
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

	ret     /* 返回到 ra 寄存器保存的地址 */