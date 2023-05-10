# 实验3
姓名: 黄嘉敏
学号: 520021911392

> 思考题 1: 内核从完成必要的初始化到用户态程序的过程是怎么样的？尝试描述一下调用关系。
内核从初始化到用户态程序过程为：先start,初始化uart模块，初始化页表，mmu模块，再初始化异常向量表，后创建根进程create_root_thread，将elf文件load_binary至内存中，创建process结构体并分配一块虚拟地址空间，最后切换到用户态程序。
start -> uart_init -> mmu_init -> exception_init -> create_root_thread -> load_binary -> eret_to_thread

> 思考题 8： ChCore中的系统调用是通过使用汇编代码直接跳转到syscall_table中的相应条目来处理的。请阅读kernel/arch/aarch64/irq/irq_entry.S中的代码，并简要描述ChCore是如何将系统调用从异常向量分派到系统调用表中对应条目的。

在irq_entry.S中，首先判断中断号是否为0x80，如果是则跳转到对应的条目，跳转过后先进入exception_enter，将寄存器保存到栈中防止上下文切换过后寄存器内容改变，再将变量加入到x0,x1,x2寄存器中，调用函数handle_entry_c或unexpected_handler，最后调exception_exit，将寄存器恢复到原来的值。