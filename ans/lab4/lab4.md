# Lab4

## Part1.多核支持

> 思考题 1：阅读汇编代码`kernel/arch/aarch64/boot/raspi3/init/start.S`。说明ChCore是如何选定主CPU，并阻塞其他其他CPU的执行的。****

`start.S`中代码如下:

```assembly
BEGIN_FUNC(_start)
	mrs	x8, mpidr_el1
	and	x8, x8,	#0xFF
	cbz	x8, primary
...
wait_until_smp_enabled:
...
/* Set CPU id */
	mov	x0, x8
	bl 	secondary_init_c
primary:
	/* Turn to el1 from other exception levels. */
	bl 	arm64_elX_to_el1

	/* Prepare stack pointer and jump to C. */
	ldr 	x0, =boot_cpu_stack
	add 	x0, x0, #INIT_STACK_SIZE
	mov 	sp, x0

	bl 	init_c
```

如上所示，函数START先将`mpdir_el1`中的cpu序号放入寄存器`x8`中，如果`x8`为0，则选为主cpu，运行init_c,其他cpu则运行`wait_until_smp_enabled`,直到`secondary_boot_flag`被设为true。

> 思考题 2：阅读汇编代码`kernel/arch/aarch64/boot/raspi3/init/start.S, init_c.c`以及`kernel/arch/aarch64/main.c`，解释用于阻塞其他CPU核心的`secondary_boot_flag`是物理地址还是虚拟地址？是如何传入函数`enable_smp_cores`中，又该如何赋值的（考虑虚拟地址/物理地址）？

是物理地址，`secondary_boot_flag`在`init_c`中被定义，并且调用函数`start_kernel`时传参，在`head.S`调用`FUNC(start_kernel)`后进入c函数`start_kernel`中，调用`enable_smp_cores`传参。

赋值的时候使用的是虚拟地址

```c
        secondary_boot_flag = (long *)phys_to_virt(boot_flag);
```

将地址改到虚拟地址后赋值为1。

> 练习题 3：完善主CPU激活各个其他CPU的函数：`enable_smp_cores`和`kernel/arch/aarch64/main.c`中的`secondary_start`

`enable_smp_cores`中将`secondary_boot_flag`设为true并更新内存，直到这个cpu启动之后开启下一个cpu。

`secondary_start`中将`cpu_status`设置为`cpu_run`。

> 练习题 4：本练习分为以下几个步骤：
>
> 1. 请熟悉排号锁的基本算法，并在`kernel/arch/aarch64/sync/ticket.c`中完成`unlock`和`is_locked`的代码。
> 2. 在`kernel/arch/aarch64/sync/ticket.c`中实现`kernel_lock_init`、`lock_kernel`和`unlock_kernel`。
> 3. 在适当的位置调用`lock_kernel`。
> 4. 判断什么时候需要放锁，添加`unlock_kernel`。（注意：由于这里需要自行判断，没有在需要添加的代码周围插入TODO注释）

Question: 代码中 `lock`结构为`owner`和`next`两个，lock函数用自旋的方式

```c++
void lock(struct lock *l)
{
        struct lock_impl *lock = (struct lock_impl *)l;
        u32 lockval = 0, newval = 0, ret = 0;

        /**
         * The following asm code means:
         *
         * lockval = atomic_fetch_and_add(lock->next, 1);
         * while(lockval != lock->owner);
         */
        BUG_ON(!lock);
        asm volatile(
                "       prfm    pstl1strm, %3\n"
                "1:     ldaxr   %w0, %3\n"
                "       add     %w1, %w0, #0x1\n"
                "       stxr    %w2, %w1, %3\n"
                "       cbnz    %w2, 1b\n"
                "2:     ldar    %w2, %4\n"
                "       cmp     %w0, %w2\n"
                "       b.ne    2b\n"
                : "=&r"(lockval), "=&r"(newval), "=&r"(ret), "+Q"(lock->next)
                : "Q"(lock->owner)
                : "memory");
}
```

为什么会是lock->next和lock->owner都初始化为0，那第一个不是就被阻塞死了？（lock->owner=0,lock_val = 1）lock无法返回

```c++
sync_el1h:
	exception_enter
	mov	x0, #SYNC_EL1h
	mrs	x1, esr_el1
	mrs	x2, elr_el1
	/* LAB 3 TODO BEGIN */
	bl handle_entry_c
	/* LAB 3 TODO END */
	exception_exit /* Lab4: Do not unlock! */
```

同时因为`handle_entry_c`的`lock_kernel`在内核态发生异常时不会加锁，所以

Maybe: 都是在el0状态下需要unlock，el1寄存器还没退出内核态，不用unlock

> 思考题 5：在`el0_syscall`调用`lock_kernel`时，在栈上保存了寄存器的值。这是为了避免调用`lock_kernel`时修改这些寄存器。在`unlock_kernel`时，是否需要将寄存器的值保存到栈中，试分析其原因。

不需要，因为`lock_kernel`后的syscall需要使用寄存器的值，而`unlock_kernel`后不再需要使用寄存器的值，而是`exception_exit`返回原先保存的寄存器上下文。

## Part2. 调度

> 思考题 6：为何`idle_threads`不会加入到等待队列中？请分析其原因？

`idle_thread`调用了指令`wfi`，是特权指令，不能在用户态执行。如果`idle_threads`空转，加入到等待队列之中，那就意味着如果有多个线程加入工作队列中，在调度的过程中会有一段时间执行`idle_threads`，浪费了cpu的资源。

> 练习题 7：完善`kernel/sched/policy_rr.c`中的调度功能

`rr_sched_enqueue`：thread非空时将队列加入节点至queue，并修改`queue_len`。

`rr_sched_dequeue`: thread非空时删除节点并修改`queue_len`

`rr_sched_choose_thread`:若队列非空，从队列中取出线程，否则

`rr_sched`:若thread的状态不是`TE_EXITING`,加入工作队列中，并从中取出新线程，进行线程切换。

```c
struct thread *cur = current_thread;
        if (cur != NULL && cur->thread_ctx != NULL
            && cur->thread_ctx->type != TYPE_IDLE) {
                if (cur->thread_ctx->sc->budget > 0
                    && cur->thread_ctx->state != TS_WAITING
                    && cur->thread_ctx->thread_exit_state != TE_EXITING) {
                        return 0;
                }
                if (cur->thread_ctx->thread_exit_state == TE_EXITING) {
                        cur->thread_ctx->state = TS_EXIT;
                        cur->thread_ctx->thread_exit_state = TE_EXITED;
                } else if (cur->thread_ctx->state != TS_WAITING) {
                        rr_sched_enqueue(cur);
                }
        }

        cur = rr_sched_choose_thread();
        rr_sched_refill_budget(cur, DEFAULT_BUDGET);
        switch_to_thread(cur);
```

> 思考题 8：如果异常是从内核态捕获的，CPU核心不会在`kernel/arch/aarch64/irq/irq_entry.c`的`handle_irq`中获得大内核锁。但是，有一种特殊情况，即如果空闲线程（以内核态运行）中捕获了错误，则CPU核心还应该获取大内核锁。否则，内核可能会被永远阻塞。请思考一下原因。

因为空闲线程在`handle_irq`退出的时候会调用`eret_thread`从而调用`unlock_kernel`，使得排号锁出现问题，可能会永远阻塞。

> 练习题 9：在`kernel/sched/sched.c`中实现系统调用`sys_yield()`，此外，ChCore还添加了一个新的系统调用`sys_get_cpu_id`，其将返回当前线程运行的CPU的核心id。

在`sys_get_cpu_id`中调用`smp_get_cpu_id`获取cpuid值并返回。

`sys_yield`中调用`sched()`并切换上下文，返回到用户态。

> 练习题 10：定时器中断初始化的相关代码已包含在本实验的初始代码中（`timer_init`）。请在主CPU以及其他CPU的初始化流程中加入对该函数的调用。此时，`yield_spin.bin`应可以正常工作：主线程应能在一定时间后重新获得对CPU核心的控制并正常终止。

在`main`函数以及`secondary_`加入`timer_init()`

> 练习题 11：在`kernel/sched/sched.c`/练习题 12

加入`budget`，在`sched_handle_timer_irq`中减少`budge`,

## Part3.进程管理器（Process Manager）

> 练习题 13：在`userland/servers/procm/launch.c`中填写`launch_process`函数中缺少的代码

需要创建为栈创建pmo,调用内核函数`__chcore_sys_create_pmo`,为延迟映射。

`stack_top`栈顶地址为`MAIN_THREAD_STACK_BASE + MAIN_THREAD_STACK_SIZE`,`offset`偏移量为去掉env初始化，由后文可知大小为0x1000(4096byte)

同时第一个`pmo_request`需要传入栈的基地址映射到当前进程的能力组中。

## Part4.进程间通讯

> 练习题 14：实现`libchcore/src/ipc/ipc.c`与`kernel/ipc/connection.c`

`ipc_register_server` 设置 `vm_config`参数

`ipc_register_client`设置`vm_config`参数，只需设置`buf_base_addr`和`buf_size`

`connection.c`:`thread_migrate_to_server` 客户端线程唤醒并切换到server线程。

## Part5.内核信号量

> 练习题 15：ChCore在`kernel/semaphore/semaphore.h`中定义了内核信号量的结构体，并在`kernel/semaphore/semaphore.c`中提供了创建信号量`init_sem`与信号量对应syscall的处理函数。

`wait_sem`消耗资源，`signal_sem`生产资源，所以`wait_sem`减少`sem->sem_count`，`signal_sem`增加`sem->sem_count`。

`wait_sem`: `is_block`为1时，要把当前线程阻塞并且还需要减少信号量的引用计数，因为当前线程不再需要使用该信号量了，被切换，即

```c
current_thread->thread_ctx->state = TS_WAITING;
list_append(&current_thread->sem_queue_node, &sem->waiting_threads);
++sem->waiting_threads_count;
obj_put(sem);
shed();
eret_to_thread(switch_context());
```

而如果sem->count不为0，则直接减少并返回。

> 练习题 16：在`userland/apps/lab4/prodcons_impl.c`中实现`producer`和`consumer`。

生产者-消费模型